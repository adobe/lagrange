/*
 * Copyright 2022 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/io/load_mesh.h>

#include <lagrange/Attribute.h>
#include <lagrange/IndexedAttribute.h>
#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/safe_cast.h>
#include <lagrange/utils/strings.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <tbb/parallel_for.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <numeric>
#include <sstream>

namespace lagrange {
namespace io {

template <typename MeshType>
auto load_mesh_obj(const fs::path& filename, const ObjReaderOptions& options)
    -> ObjReaderResult<typename MeshType::Scalar, typename MeshType::Index>
{
    ObjReaderResult<typename MeshType::Scalar, typename MeshType::Index> result;

    using Scalar = typename MeshType::Scalar;
    using Index = typename MeshType::Index;
    using SignedIndex = typename MeshType::SignedIndex;

    tinyobj::ObjReader reader;
    {
        logger().trace("[load_mesh_obj] Parsing obj file: {}", filename.string());
        tinyobj::ObjReaderConfig config;
        config.triangulate = options.triangulate;
        config.vertex_color = options.load_vertex_colors;
        config.mtl_search_path = options.mtl_search_path;
        reader.ParseFromFile(filename.string(), config);
    }

    result.success = reader.Valid();
    if (!reader.Warning().empty()) {
        auto lines = string_split(reader.Warning(), '\n');
        for (const auto& msg : lines) {
            logger().warn("[load_mesh_obj] {}", msg);
        }
    }
    if (!reader.Error().empty()) {
        auto lines = string_split(reader.Error(), '\n');
        for (const auto& msg : lines) {
            logger().error("[load_mesh_obj] {}", msg);
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
        std::copy_n(attrib.vertices.begin() + static_cast<ptrdiff_t>(v * dim), dim, p.begin());
    });

    // Copy texcoord values
    IndexedAttribute<Scalar, Index>* uv_attr = nullptr;
    if (options.load_uvs && !attrib.texcoords.empty()) {
        logger().trace("[load_mesh_obj] Copying uvs");
        la_runtime_assert(attrib.texcoords.size() % uv_dim == 0);
        auto id = mesh.template create_attribute<Scalar>(
            "uv",
            AttributeElement::Indexed,
            AttributeUsage::UV,
            uv_dim);
        uv_attr = &mesh.template ref_indexed_attribute<Scalar>(id);
        uv_attr->values().resize_elements(attrib.texcoords.size() / uv_dim);
        std::copy(
            attrib.texcoords.begin(),
            attrib.texcoords.end(),
            uv_attr->values().ref_all().begin());
    }

    // Copy normal values
    IndexedAttribute<Scalar, Index>* nrm_attr = nullptr;
    if (options.load_normals && !attrib.normals.empty()) {
        logger().trace("[load_mesh_obj] Copying normals");
        la_runtime_assert(attrib.normals.size() % dim == 0);
        auto id = mesh.template create_attribute<Scalar>(
            "normal",
            AttributeElement::Indexed,
            AttributeUsage::Normal,
            dim);
        nrm_attr = &mesh.template ref_indexed_attribute<Scalar>(id);
        nrm_attr->values().resize_elements(attrib.normals.size() / dim);
        std::copy(
            attrib.normals.begin(),
            attrib.normals.end(),
            nrm_attr->values().ref_all().begin());
    }

    // Copy vertex colors
    if (options.load_vertex_colors && !attrib.colors.empty()) {
        logger().trace("[load_mesh_obj] Copying vertex colors");
        la_runtime_assert(mesh.get_num_vertices() == attrib.colors.size() / dim);
        auto id = mesh.template create_attribute<Scalar>(
            "color",
            AttributeElement::Vertex,
            AttributeUsage::Color,
            dim);
        auto& attr = mesh.template ref_attribute<Scalar>(id);
        std::copy(attrib.colors.begin(), attrib.colors.end(), attr.ref_all().begin());
    }

    // Reserve facet indices
    logger().trace("[load_mesh_obj] Reserve facet indices");
    std::vector<Index> facet_sizes;
    for (const auto& shape : shapes) {
        facet_sizes.insert(
            facet_sizes.end(),
            shape.mesh.num_face_vertices.begin(),
            shape.mesh.num_face_vertices.end());
        result.names.push_back(shape.name);
    }
    mesh.add_hybrid(facet_sizes);

    // Initialize material id attr
    Attribute<SignedIndex>* mat_attr = nullptr;
    if (options.load_materials) {
        auto id = mesh.template create_attribute<SignedIndex>(
            "material_id",
            AttributeElement::Facet,
            AttributeUsage::Scalar);
        mat_attr = &mesh.template ref_attribute<SignedIndex>(id);
    }

    // Initialize object id
    Attribute<Index>* id_attr = nullptr;
    if (options.load_object_id) {
        auto id = mesh.template create_attribute<Index>(
            "object_id",
            AttributeElement::Facet,
            AttributeUsage::Scalar);
        id_attr = &mesh.template ref_attribute<Index>(id);
    }

    span<Index> vtx_indices = mesh.ref_corner_to_vertex().ref_all();
    span<Index> uv_indices = (uv_attr ? uv_attr->indices().ref_all() : span<Index>{});
    span<Index> nrm_indices = (nrm_attr ? nrm_attr->indices().ref_all() : span<Index>{});

    logger().trace("[load_mesh_obj] Copy facet indices");
    std::partial_sum(facet_sizes.begin(), facet_sizes.end(), facet_sizes.begin());
    tbb::parallel_for(Index(0), Index(shapes.size()), [&](Index i) {
        const auto& shape = shapes[i];
        const Index first_facet = (i == 0 ? 0 : facet_sizes[i - 1]);
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
                    uv_indices[c] = safe_cast<Index>(index.texcoord_index);
                }
                if (!nrm_indices.empty()) {
                    nrm_indices[c] = safe_cast<Index>(index.normal_index);
                }
            }
        }

        // TODO: Support smoothing groups + subd tags
    });

    logger().trace("[load_mesh_obj] Loading complete");

    return result;
}

#define LA_X_load_mesh(_, Scalar, Index)                                               \
    template ObjReaderResult<Scalar, Index> load_mesh_obj<SurfaceMesh<Scalar, Index>>( \
        const fs::path& filename,                                                      \
        const ObjReaderOptions& options);
LA_SURFACE_MESH_X(load_mesh, 0)

} // namespace io
} // namespace lagrange
