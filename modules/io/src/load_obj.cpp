/*
 * Copyright 2023 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */

// this .cpp provides implementation for functions defined in those headers:
#include <lagrange/io/api.h>
#include <lagrange/io/internal/load_obj.h>
#include <lagrange/io/load_mesh_obj.h>
#include <lagrange/io/load_scene_obj.h>

#include "stitch_mesh.h"

// ====

#include <lagrange/Attribute.h>
#include <lagrange/IndexedAttribute.h>
#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/attribute_names.h>
#include <lagrange/io/internal/scene_utils.h>
#include <lagrange/scene/Scene.h>
#include <lagrange/scene/SceneTypes.h>
#include <lagrange/scene/SimpleScene.h>
#include <lagrange/scene/SimpleSceneTypes.h>
#include <lagrange/scene/scene_utils.h>
#include <lagrange/utils/Error.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/invalid.h>
#include <lagrange/utils/safe_cast.h>
#include <lagrange/utils/strings.h>

#include <tiny_gltf.h>
#include <numeric>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <tbb/parallel_for.h>
#include <lagrange/utils/warnon.h>
// clang-format on

namespace lagrange::io {

namespace internal {

template <typename MeshType>
ObjReaderResult<typename MeshType::Scalar, typename MeshType::Index> extract_mesh(
    const tinyobj::ObjReader& reader,
    const LoadOptions& options)
{
    using Scalar = typename MeshType::Scalar;
    using Index = typename MeshType::Index;
    using SignedIndex = typename MeshType::SignedIndex;

    ObjReaderResult<typename MeshType::Scalar, typename MeshType::Index> result;
    result.success = reader.Valid();
    if (!reader.Warning().empty()) {
        auto lines = string_split(reader.Warning(), '\n');
        for (const auto& msg : lines) {
            if (!options.quiet) {
                logger().warn("[load_mesh_obj] {}", msg);
            }
        }
    }
    if (!reader.Error().empty()) {
        auto lines = string_split(reader.Error(), '\n');
        for (const auto& msg : lines) {
            if (!options.quiet) {
                logger().error("[load_mesh_obj] {}", msg);
            }
        }
    }
    if (!reader.Valid()) {
        return result;
    }

    logger().trace("[load_mesh_obj] Copying data into a mesh");

    auto& mesh = result.mesh;
    const Index dim = 3;
    const Index uv_dim = 2;
    const auto& attrib = reader.GetAttrib();
    const auto& shapes = reader.GetShapes();
    if (options.load_materials) {
        result.materials = reader.GetMaterials();
    }

    // Copy vertices
    logger().trace("[load_mesh_obj] Copying vertices");
    la_runtime_assert(attrib.vertices.size() % dim == 0);
    const Index num_vertices = safe_cast<Index>(attrib.vertices.size()) / dim;
    mesh.add_vertices(num_vertices, [&](Index v, span<Scalar> p) {
        la_debug_assert(dim == p.size());
        std::transform(
            attrib.vertices.begin() + static_cast<ptrdiff_t>(v * dim),
            attrib.vertices.begin() + static_cast<ptrdiff_t>(v * dim + dim),
            p.begin(),
            [](tinyobj::real_t val) { return static_cast<Scalar>(val); });
    });

    // Copy texcoord values
    IndexedAttribute<Scalar, Index>* uv_attr = nullptr;
    if (options.load_uvs && !attrib.texcoords.empty()) {
        logger().trace("[load_mesh_obj] Copying uvs");
        la_runtime_assert(attrib.texcoords.size() % uv_dim == 0);
        auto id = mesh.template create_attribute<Scalar>(
            AttributeName::texcoord,
            AttributeElement::Indexed,
            AttributeUsage::UV,
            uv_dim);
        uv_attr = &mesh.template ref_indexed_attribute<Scalar>(id);
        uv_attr->values().resize_elements(attrib.texcoords.size() / uv_dim);
        std::transform(
            attrib.texcoords.begin(),
            attrib.texcoords.end(),
            uv_attr->values().ref_all().begin(),
            [](tinyobj::real_t val) { return static_cast<Scalar>(val); });
    }

    // Copy normal values
    IndexedAttribute<Scalar, Index>* nrm_attr = nullptr;
    if (options.load_normals && !attrib.normals.empty()) {
        logger().trace("[load_mesh_obj] Copying normals");
        la_runtime_assert(attrib.normals.size() % dim == 0);
        auto id = mesh.template create_attribute<Scalar>(
            AttributeName::normal,
            AttributeElement::Indexed,
            AttributeUsage::Normal,
            dim);
        nrm_attr = &mesh.template ref_indexed_attribute<Scalar>(id);
        nrm_attr->values().resize_elements(attrib.normals.size() / dim);
        std::transform(
            attrib.normals.begin(),
            attrib.normals.end(),
            nrm_attr->values().ref_all().begin(),
            [](tinyobj::real_t val) { return static_cast<Scalar>(val); });
    }

    // Copy vertex colors
    if (options.load_vertex_colors && !attrib.colors.empty()) {
        logger().trace("[load_mesh_obj] Copying vertex colors");
        la_runtime_assert(mesh.get_num_vertices() == attrib.colors.size() / dim);
        auto id = mesh.template create_attribute<Scalar>(
            AttributeName::color,
            AttributeElement::Vertex,
            AttributeUsage::Color,
            dim);
        auto& attr = mesh.template ref_attribute<Scalar>(id);
        std::transform(
            attrib.colors.begin(),
            attrib.colors.end(),
            attr.ref_all().begin(),
            [](tinyobj::real_t val) { return static_cast<Scalar>(val); });
    }

    // Reserve facet indices
    logger().trace("[load_mesh_obj] Reserve facet indices");
    std::vector<Index> facet_sizes;
    std::vector<Index> facet_counts;
    for (const auto& shape : shapes) {
        facet_sizes.insert(
            facet_sizes.end(),
            shape.mesh.num_face_vertices.begin(),
            shape.mesh.num_face_vertices.end());
        facet_counts.push_back(static_cast<Index>(shape.mesh.num_face_vertices.size()));
        result.names.push_back(shape.name);
    }
    if (!facet_sizes.empty()) {
        mesh.add_hybrid(facet_sizes);
    }

    // Initialize material id attr
    Attribute<SignedIndex>* mat_attr = nullptr;
    if (options.load_materials) {
        auto id = mesh.template create_attribute<SignedIndex>(
            AttributeName::material_id,
            AttributeElement::Facet,
            AttributeUsage::Scalar);
        mat_attr = &mesh.template ref_attribute<SignedIndex>(id);
    }

    // Initialize object id
    Attribute<Index>* id_attr = nullptr;
    if (options.load_object_ids) {
        auto id = mesh.template create_attribute<Index>(
            AttributeName::object_id,
            AttributeElement::Facet,
            AttributeUsage::Scalar);
        id_attr = &mesh.template ref_attribute<Index>(id);
    }

    span<Index> vtx_indices = mesh.ref_corner_to_vertex().ref_all();
    span<Index> uv_indices = (uv_attr ? uv_attr->indices().ref_all() : span<Index>{});
    span<Index> nrm_indices = (nrm_attr ? nrm_attr->indices().ref_all() : span<Index>{});

    logger().trace("[load_mesh_obj] Copy facet indices");
    std::partial_sum(facet_counts.begin(), facet_counts.end(), facet_counts.begin());
    std::atomic_size_t num_invalid_uv = 0;
    tbb::parallel_for(Index(0), Index(shapes.size()), [&](Index i) {
        const auto& shape = shapes[i];
        const Index first_facet = (i == 0 ? 0 : facet_counts[i - 1]);
        const auto local_num_facets = safe_cast<Index>(shape.mesh.num_face_vertices.size());

        // Copy material id
        if (mat_attr) {
            la_debug_assert(local_num_facets <= mesh.get_num_facets());
            la_debug_assert(local_num_facets <= mat_attr->get_num_elements());
            auto span = mat_attr->ref_middle(first_facet, local_num_facets);
            if (shape.mesh.material_ids.empty()) {
                std::fill(span.begin(), span.end(), SignedIndex(-1));
            } else {
                la_runtime_assert(shape.mesh.material_ids.size() == local_num_facets);
                std::transform(
                    shape.mesh.material_ids.begin(),
                    shape.mesh.material_ids.end(),
                    span.begin(),
                    [](auto x) -> SignedIndex { return safe_cast<SignedIndex>(x); });
            }
        }

        // Copy object id
        if (id_attr) {
            auto span = id_attr->ref_middle(first_facet, local_num_facets);
            std::fill(span.begin(), span.end(), i);
        }

        // Copy indices
        for (Index f = 0, local_corner = 0; f < local_num_facets; ++f) {
            const Index first_corner = mesh.get_facet_corner_begin(first_facet + f);
            const Index last_corner = mesh.get_facet_corner_end(first_facet + f);

            for (Index c = first_corner; c < last_corner; ++c, ++local_corner) {
                const auto& index = shape.mesh.indices[local_corner];
                vtx_indices[c] = safe_cast<Index>(index.vertex_index);
                if (!uv_indices.empty()) {
                    if (index.texcoord_index < 0) {
                        uv_indices[c] = invalid<Index>();
                        ++num_invalid_uv;
                    } else {
                        uv_indices[c] = safe_cast<Index>(index.texcoord_index);
                    }
                }
                if (!nrm_indices.empty()) {
                    nrm_indices[c] = safe_cast<Index>(index.normal_index);
                }
            }
        }

        // TODO: Support smoothing groups + subd tags
    });

    if (num_invalid_uv) {
        // This one is a legit warning, so we do not silence it even in quiet mode.
        logger().warn(
            "Found {} vertices without UV indices. UV attribute will have invalid values.",
            num_invalid_uv.load());
    }
    logger().trace("[load_mesh_obj] Loading complete");

    if (options.stitch_vertices) {
        stitch_mesh(result.mesh);
    }

    return result;
}

tinyobj::ObjReader load_obj(const fs::path& filename, const LoadOptions& options)
{
    tinyobj::ObjReader reader;

    logger().trace("[load_mesh_obj] Parsing obj file: {}", filename.string());
    tinyobj::ObjReaderConfig config;
    config.triangulate = options.triangulate;
    config.vertex_color = false;
    config.mtl_search_path = options.search_path.string();
    reader.ParseFromFile(filename.string(), config);

    return reader;
}

tinyobj::ObjReader
load_obj(std::istream& input_stream_obj, std::istream& input_stream_mtl, const LoadOptions& options)
{
    tinyobj::ObjReader reader;

    logger().trace("[load_mesh_obj] Parsing obj from stream");
    tinyobj::ObjReaderConfig config;
    config.triangulate = options.triangulate;
    config.vertex_color = false;
    config.mtl_search_path = options.search_path.string();

    std::istreambuf_iterator<char> data_itr_obj(input_stream_obj), end_of_stream_obj;
    std::string obj_data(data_itr_obj, end_of_stream_obj);

    std::istreambuf_iterator<char> data_itr_mtl(input_stream_mtl), end_of_stream_mtl;
    std::string mtl_data(data_itr_mtl, end_of_stream_mtl);
    reader.ParseFromString(obj_data, mtl_data, config);

    return reader;
}

// =====================================
// internal/load_mesh_obj.h
// =====================================
template <typename MeshType>
MeshType load_mesh_obj(const tinyobj::ObjReader& reader, const LoadOptions& options)
{
    auto result = extract_mesh<MeshType>(reader, options);
    return std::move(result.mesh);
}
#define LA_X_load_mesh_obj(_, S, I)             \
    template SurfaceMesh<S, I> load_mesh_obj(   \
        const const tinyobj::ObjReader& reader, \
        const LoadOptions& options);            \
    LA_SURFACE_MESH_X(load_mesh_obj, 0)
#undef LA_X_load_mesh_obj

template <typename SceneType>
SceneType load_simple_scene_obj(const tinyobj::ObjReader& reader, const LoadOptions& options)
{
    // TODO:
    // this function is rather useless as it is. We should split mesh objects into separate meshes
    // (or rather, don't combine them in the first place).

    using MeshType = typename SceneType::MeshType;
    using Index = typename MeshType::Index;
    using AffineTransform = typename SceneType::AffineTransform;

    auto result = extract_mesh<MeshType>(reader, options);

    SceneType lscene;
    Index mesh_idx = lscene.add_mesh(result.mesh);
    lscene.add_instance({mesh_idx, AffineTransform()});
    return lscene;
}
#define LA_X_load_simple_scene_obj(_, S, I, D)                  \
    template scene::SimpleScene<S, I, D> load_simple_scene_obj( \
        const tinyobj::ObjReader& reader,                       \
        const LoadOptions& options);
LA_SIMPLE_SCENE_X(load_simple_scene_obj, 0);
#undef LA_X_load_simple_scene_obj

template <typename SceneType>
SceneType load_scene_obj(const tinyobj::ObjReader& reader, const LoadOptions& options)
{
    // TODO:
    // Just like the above, we should split mesh objects into separate meshes.
    // However, this function is not useless, as it also returns material information.

    using MeshType = typename SceneType::MeshType;

    auto result = extract_mesh<MeshType>(reader, options);

    SceneType lscene;
    scene::ElementId mesh_idx = lscene.add(std::move(result.mesh));

    // make a node to hold the meshes
    scene::Node lnode;
    lnode.meshes.push_back({mesh_idx, {}});

    for (const tinyobj::material_t& mat : reader.GetMaterials()) {
        scene::MaterialExperimental lmat;
        lmat.name = mat.name;

        // we use the PBR extension in tinyobj, but note that this data may not be in the mtl
        // http://exocortex.com/blog/extending_wavefront_mtl_to_support_pbr
        lmat.base_color_value =
            Eigen::Vector4<float>(mat.diffuse[0], mat.diffuse[1], mat.diffuse[2], 1.0);
        lmat.emissive_value =
            Eigen::Vector3<float>(mat.emission[0], mat.emission[1], mat.emission[2]);

        auto try_load_texture = [&](const std::string& name,
                                    const tinyobj::texture_option_t& tex_opt,
                                    scene::TextureInfo& tex_info) {
            scene::ImageExperimental limage;
            limage.name = name;
            limage.uri = name;
            if (options.load_images) {
                if (!io::internal::try_load_image(name, options, limage)) return;
            }
            int image_idx = static_cast<int>(lscene.add(std::move(limage)));

            tex_info.index = static_cast<int>(lscene.textures.size());

            scene::Texture ltex;
            ltex.name = name;
            ltex.image = image_idx;
            ltex.offset = Eigen::Vector2f(tex_opt.origin_offset[0], tex_opt.origin_offset[1]);
            ltex.scale = Eigen::Vector2f(tex_opt.scale[0], tex_opt.scale[1]);
            if (tex_opt.clamp) {
                ltex.wrap_u = ltex.wrap_v = scene::Texture::WrapMode::Clamp;
            }
            lscene.textures.push_back(ltex);
        };

        if (!mat.diffuse_texname.empty()) {
            try_load_texture(mat.diffuse_texname, mat.diffuse_texopt, lmat.base_color_texture);
        }

        if (!mat.roughness_texname.empty()) {
            try_load_texture(
                mat.roughness_texname,
                mat.roughness_texopt,
                lmat.metallic_roughness_texture);
        } else if (!mat.metallic_texname.empty()) {
            try_load_texture(
                mat.metallic_texname,
                mat.metallic_texopt,
                lmat.metallic_roughness_texture);
        }

        if (!mat.normal_texname.empty()) {
            try_load_texture(mat.normal_texname, mat.normal_texopt, lmat.normal_texture);
        } else if (!mat.bump_texname.empty()) {
            try_load_texture(mat.bump_texname, mat.bump_texopt, lmat.normal_texture);
        }

        if (!mat.emissive_texname.empty()) {
            try_load_texture(mat.emissive_texname, mat.emissive_texopt, lmat.emissive_texture);
        }

        lscene.materials.push_back(lmat);
        lnode.meshes.front().materials.push_back(lscene.materials.size() - 1);
    }

    lscene.nodes.push_back(lnode);
    lscene.root_nodes.push_back(0);

    return lscene;
}
#define LA_X_load_scene_obj(_, S, I)            \
    template scene::Scene<S, I> load_scene_obj( \
        const tinyobj::ObjReader& reader,       \
        const LoadOptions& options);
LA_SCENE_X(load_scene_obj, 0);
#undef LA_X_load_scene_obj

// =====================================
// internal/load_mesh_obj.h (old api)
// =====================================
template <typename MeshType>
auto load_mesh_obj(const fs::path& filename, const LoadOptions& options)
    -> ObjReaderResult<typename MeshType::Scalar, typename MeshType::Index>
{
    auto reader = load_obj(filename, options);
    return extract_mesh<MeshType>(reader, options);
}

template <typename MeshType>
auto load_mesh_obj(
    std::istream& input_stream_obj,
    std::istream& input_stream_mtl,
    const LoadOptions& options)
    -> ObjReaderResult<typename MeshType::Scalar, typename MeshType::Index>
{
    auto reader = load_obj(input_stream_obj, input_stream_mtl, options);
    return extract_mesh<MeshType>(reader, options);
}

#define LA_X_load_mesh(_, Scalar, Index)                                                         \
    template LA_IO_API ObjReaderResult<Scalar, Index> load_mesh_obj<SurfaceMesh<Scalar, Index>>( \
        const fs::path& filename,                                                                \
        const LoadOptions& options);                                                             \
    template LA_IO_API ObjReaderResult<Scalar, Index> load_mesh_obj<SurfaceMesh<Scalar, Index>>( \
        std::istream & input_stream_obj,                                                         \
        std::istream & input_stream_mtl,                                                         \
        const LoadOptions& options);
LA_SURFACE_MESH_X(load_mesh, 0)

} // namespace internal

// =====================================
// load_mesh_obj.h
// =====================================
template <typename MeshType>
MeshType load_mesh_obj(const fs::path& filename, const LoadOptions& options)
{
    auto ret = internal::load_mesh_obj<MeshType>(filename, options);
    if (!ret.success) {
        throw Error(fmt::format("Failed to load mesh from file: '{}'", filename.string()));
    }
    return std::move(ret.mesh);
}

template <typename MeshType>
MeshType load_mesh_obj(std::istream& input_stream_obj, const LoadOptions& options)
{
    std::istream stream(nullptr);

    auto ret = internal::load_mesh_obj<MeshType>(input_stream_obj, stream, options);
    if (!ret.success) {
        throw Error("Failed to load mesh from stream");
    }
    return std::move(ret.mesh);
}

#define LA_X_load_mesh_obj(_, S, I)                     \
    template LA_IO_API SurfaceMesh<S, I> load_mesh_obj( \
        const fs::path& filename,                       \
        const LoadOptions& options);                    \
    template LA_IO_API SurfaceMesh<S, I> load_mesh_obj(std::istream&, const LoadOptions& options);
LA_SURFACE_MESH_X(load_mesh_obj, 0)
#undef LA_X_load_mesh_obj

// =====================================
// load_scene_obj.h
// =====================================
template <typename SceneType>
SceneType load_scene_obj(const fs::path& filename, const LoadOptions& options)
{
    tinyobj::ObjReader reader = internal::load_obj(filename, options);
    LoadOptions opt2 = options;
    if (opt2.search_path.empty()) opt2.search_path = filename.parent_path();
    return internal::load_scene_obj<SceneType>(reader, opt2);
}

template <typename SceneType>
SceneType load_scene_obj(
    std::istream& input_stream_obj,
    std::istream& input_stream_mtl,
    const LoadOptions& options)
{
    tinyobj::ObjReader reader = internal::load_obj(input_stream_obj, input_stream_mtl, options);
    return internal::load_scene_obj<SceneType>(reader, options);
}
#define LA_X_load_scene_obj(_, S, I)                                                           \
    template LA_IO_API scene::Scene<S, I> load_scene_obj(const fs::path&, const LoadOptions&); \
    template LA_IO_API scene::Scene<S, I> load_scene_obj(                                      \
        std::istream&,                                                                         \
        std::istream&,                                                                         \
        const LoadOptions&);
LA_SCENE_X(load_scene_obj, 0);
#undef LA_X_load_scene_obj
} // namespace lagrange::io
