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

// this .cpp provides implementations for functions defined in those headers:
#include <lagrange/io/api.h>
#include <lagrange/io/load_mesh_fbx.h>
#include <lagrange/io/load_scene_fbx.h>
#include <lagrange/io/load_simple_scene_fbx.h>

#include "stitch_mesh.h"

// ====

#include <lagrange/IndexedAttribute.h>
#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/attribute_names.h>
#include <lagrange/io/internal/scene_utils.h>
#include <lagrange/scene/SceneTypes.h>
#include <lagrange/scene/SimpleSceneTypes.h>
#include <lagrange/scene/scene_utils.h>
#include <lagrange/scene/simple_scene_convert.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/safe_cast.h>
#include <lagrange/utils/utils.h>

#include <ufbx.h>

namespace lagrange::io {
namespace {

template <typename AffineTransform>
AffineTransform convert_transform_ufbx_to_lagrange(const ufbx_matrix& m)
{
    AffineTransform transform;
    // clang-format off
    transform.matrix() <<
        m.m00, m.m01, m.m02, m.m03,
        m.m10, m.m11, m.m12, m.m13,
        m.m20, m.m21, m.m22, m.m23,
        0, 0, 0, 1;
    // clang-format on
    return transform;
}
template <typename AffineTransform>
AffineTransform convert_transform_ufbx_to_lagrange(const ufbx_transform* t)
{
    ufbx_matrix m = ufbx_transform_to_matrix(t);
    return convert_transform_ufbx_to_lagrange<AffineTransform>(m);
}

template <typename MeshType>
MeshType convert_mesh_ufbx_to_lagrange(const ufbx_mesh* mesh, const LoadOptions& opt)
{
    using Index = typename MeshType::Index;
    using Scalar = typename MeshType::Scalar;

    constexpr size_t dim = 3;
    constexpr size_t uv_dim = 2;
    constexpr size_t color_dim = 4;

    MeshType lmesh;
    lmesh.add_vertices(safe_cast<Index>(mesh->num_vertices), [&](Index v, span<Scalar> p) -> void {
        for (size_t i = 0; i < dim; ++i) {
            p[i] = Scalar(mesh->vertices[v].v[i]);
        }
    });
    lmesh.add_hybrid(
        safe_cast<Index>(mesh->num_faces),
        [&](Index f) -> Index { return mesh->faces[f].num_indices; },
        [&](Index f, span<Index> t) -> void {
            const uint32_t index_begin = mesh->faces[f].index_begin;
            const uint32_t num_indices = mesh->faces[f].num_indices;
            for (size_t i = 0; i < num_indices; ++i) {
                t[i] = mesh->vertex_indices[index_begin + i];
            }
        });

    // normals
    if (opt.load_normals && mesh->vertex_normal.exists) {
        auto id = lmesh.template create_attribute<Scalar>(
            AttributeName::normal,
            AttributeElement::Indexed,
            dim,
            AttributeUsage::Normal);
        auto& attr = lmesh.template ref_indexed_attribute<Scalar>(id);
        attr.indices().resize_elements(mesh->vertex_normal.indices.count);
        attr.values().resize_elements(mesh->vertex_normal.values.count * dim);
        std::copy_n(
            mesh->vertex_normal.indices.begin(),
            mesh->vertex_normal.indices.count,
            attr.indices().ref_all().begin());
        std::copy_n(
            &mesh->vertex_normal.values[0].v[0],
            mesh->vertex_normal.values.count * dim,
            attr.values().ref_all().begin());
    }

    // uvs
    if (opt.load_uvs) {
        for (size_t i = 0; i < mesh->uv_sets.count; ++i) {
            const ufbx_uv_set& uv_set = mesh->uv_sets[i];
            std::string name{uv_set.name.data};

            auto id = lmesh.template create_attribute<Scalar>(
                name,
                AttributeElement::Indexed,
                AttributeUsage::UV,
                uv_dim);
            auto& attr = lmesh.template ref_indexed_attribute<Scalar>(id);
            attr.indices().resize_elements(uv_set.vertex_uv.indices.count);
            attr.values().resize_elements(uv_set.vertex_uv.values.count * uv_dim);
            std::copy_n(
                uv_set.vertex_uv.indices.begin(),
                uv_set.vertex_uv.indices.count,
                attr.indices().ref_all().begin());
            std::copy_n(
                &uv_set.vertex_uv.values[0].v[0],
                uv_set.vertex_uv.values.count * 2,
                attr.values().ref_all().begin());
        }
    }

    if (opt.load_weights) {
        for (size_t i = 0; i < mesh->skin_deformers.count; ++i) {
            // multiple deformers per mesh is rare but possible
            const ufbx_skin_deformer* deformer = mesh->skin_deformers[i];

            // deformer->weights is a list of {weight, bone index}
            // deformer->vertices is a list of {index, count} into the list above.
            //
            // here we are converting the data to a more traditional layout. This will not cause
            // loss of data, but it can increase the memory footprint, especially if the max number
            // of weights per vertex is much higher than the average number of weights per vertex.
            //
            // we could avoid this by having two indexed attributes instead, for both weights and
            // indices, but this will waste indexing data, and will be inconsistent with other
            // skinning loaders in lagrange.

            auto num_weights_per_vertex = deformer->max_weights_per_vertex;

            auto bone_id = lmesh.template create_attribute<Index>(
                AttributeName::indexed_joint,
                AttributeElement::Vertex,
                AttributeUsage::Vector,
                num_weights_per_vertex);
            auto weight_id = lmesh.template create_attribute<Scalar>(
                AttributeName::indexed_weight,
                AttributeElement::Vertex,
                AttributeUsage::Vector,
                num_weights_per_vertex);
            auto& bone_attr = lmesh.template ref_attribute<Index>(bone_id);
            auto& weight_attr = lmesh.template ref_attribute<Scalar>(weight_id);
            bone_attr.resize_elements(deformer->vertices.count);
            weight_attr.resize_elements(deformer->vertices.count);

            for (size_t v = 0; v < deformer->vertices.count; ++v) {
                const ufbx_skin_vertex& skin_vertex = deformer->vertices[v];
                for (size_t j = 0; j < num_weights_per_vertex; ++j) {
                    double weight = 0;
                    unsigned int bone_index = 0;
                    if (j < skin_vertex.num_weights) {
                        const auto& skin_weight = deformer->weights[skin_vertex.weight_begin + j];
                        weight = skin_weight.weight;
                        bone_index = skin_weight.cluster_index;
                    }
                    bone_attr.ref_all()[v * num_weights_per_vertex + j] = bone_index;
                    weight_attr.ref_all()[v * num_weights_per_vertex + j] = weight;
                }
            }
        }
    }

    // tangent
    if (opt.load_tangents && mesh->vertex_tangent.exists) {
        auto id = lmesh.template create_attribute<Scalar>(
            AttributeName::tangent,
            AttributeElement::Indexed,
            AttributeUsage::Vector,
            dim);
        auto& attr = lmesh.template ref_indexed_attribute<Scalar>(id);
        attr.indices().resize_elements(mesh->vertex_tangent.indices.count);
        attr.values().resize_elements(mesh->vertex_tangent.values.count * dim);
        std::copy_n(
            mesh->vertex_tangent.indices.begin(),
            mesh->vertex_tangent.indices.count,
            attr.indices().ref_all().begin());
        std::copy_n(
            &mesh->vertex_tangent.values[0].v[0],
            mesh->vertex_tangent.values.count * dim,
            attr.indices().ref_all().begin());
    }

    // bitangent
    if (opt.load_tangents && mesh->vertex_bitangent.exists) {
        auto id = lmesh.template create_attribute<Scalar>(
            AttributeName::bitangent,
            AttributeElement::Indexed,
            AttributeUsage::Vector,
            dim);
        auto& attr = lmesh.template ref_indexed_attribute<Scalar>(id);
        attr.indices().resize_elements(mesh->vertex_bitangent.indices.count);
        attr.values().resize_elements(mesh->vertex_bitangent.values.count * dim);
        std::copy_n(
            mesh->vertex_bitangent.indices.begin(),
            mesh->vertex_bitangent.indices.count,
            attr.indices().ref_all().begin());
        std::copy_n(
            &mesh->vertex_bitangent.values[0].v[0],
            mesh->vertex_bitangent.values.count * dim,
            attr.indices().ref_all().begin());
    }

    // vertex color
    if (opt.load_vertex_colors && mesh->vertex_color.exists) {
        auto id = lmesh.template create_attribute<Scalar>(
            AttributeName::color,
            AttributeElement::Indexed,
            AttributeUsage::Color,
            color_dim);
        auto& attr = lmesh.template ref_indexed_attribute<Scalar>(id);
        attr.indices().resize_elements(mesh->vertex_color.indices.count);
        attr.values().resize_elements(mesh->vertex_color.values.count * color_dim);
        std::copy_n(
            mesh->vertex_color.indices.begin(),
            mesh->vertex_color.indices.count,
            attr.indices().ref_all().begin());
        std::copy_n(
            &mesh->vertex_color.values[0].v[0],
            mesh->vertex_color.values.count * color_dim,
            attr.indices().ref_all().begin());
    }

    if (opt.stitch_vertices) {
        stitch_mesh(lmesh);
    }

    return lmesh;
}

struct UfbxScene
{
    ufbx_scene* ptr;
    UfbxScene(ufbx_scene* _ptr)
        : ptr(_ptr)
    {}
    ~UfbxScene() { ufbx_free_scene(ptr); }

    UfbxScene(const UfbxScene&) = delete;
    UfbxScene(UfbxScene&&) = delete;
    UfbxScene& operator=(const UfbxScene&) = delete;
    UfbxScene& operator=(UfbxScene&&) = delete;
};

UfbxScene load_ufbx(const fs::path& filename)
{
    std::string filename_s = filename.string();
    ufbx_load_opts opts{};
    ufbx_error error{};
    return ufbx_load_file(filename_s.c_str(), &opts, &error);
}

UfbxScene load_ufbx(std::istream& input_stream)
{
    std::istreambuf_iterator<char> data_itr(input_stream), end_of_stream;
    std::string data(data_itr, end_of_stream);

    ufbx_load_opts opts{};
    ufbx_error error{};
    return ufbx_load_memory(data.data(), data.size(), &opts, &error);
}

template <typename SceneType>
SceneType load_simple_scene_fbx(const ufbx_scene* scene, const LoadOptions& opt)
{
    using MeshType = typename SceneType::MeshType;
    using AffineTransform = typename SceneType::AffineTransform;
    using Index = typename MeshType::Index;

    // in ufbx, everything is an element (meshes, materials, nodes).
    // each element has an unique id.
    // all references between nodes is done with pointers, not through indices or element id.
    // so get back to our index-based referencing, we construct a vector. This is indexed by element
    // id, and contains the index in its respective list

    std::vector<size_t> element_index;
    constexpr size_t invalid_element_index = lagrange::invalid<size_t>();
    element_index.resize(scene->elements.count, invalid_element_index);

    SceneType lscene;

    for (size_t i = 0; i < scene->meshes.count; ++i) {
        const ufbx_mesh* mesh = scene->meshes[i];
        auto lmesh = convert_mesh_ufbx_to_lagrange<MeshType>(mesh, opt);
        element_index[mesh->element_id] = lscene.add_mesh(lmesh);
    }

    for (size_t i = 0; i < scene->nodes.count; ++i) {
        const ufbx_node* node = scene->nodes[i];
        if (node->mesh) {
            size_t mesh_idx = element_index[node->mesh->element_id];
            la_runtime_assert(mesh_idx != invalid_element_index);
            AffineTransform t =
                convert_transform_ufbx_to_lagrange<AffineTransform>(node->node_to_world);
            lscene.add_instance({Index(mesh_idx), t});
        }
    }

    return lscene;
}

Eigen::Vector3f to_vec3(const ufbx_vec3& v)
{
    return Eigen::Vector3f(v.x, v.y, v.z);
}

Eigen::Vector4f to_vec4(const ufbx_vec4& v)
{
    return Eigen::Vector4f(v.x, v.y, v.z, v.w);
}

template <typename SceneType>
SceneType load_scene_fbx(const ufbx_scene* scene, const LoadOptions& opt)
{
    // in ufbx, everything is an element (meshes, materials, nodes).
    // each element has an unique id.
    // all references between nodes is done with pointers, not through indices or element id.
    // so get back to our index-based referencing, we construct a vector. This is indexed by element
    // id, and contains the index in its respective list

    std::vector<size_t> element_index;
    constexpr size_t invalid_element_index = lagrange::invalid<size_t>();
    element_index.resize(scene->elements.count, invalid_element_index);

    SceneType lscene;
    using MeshType = typename SceneType::MeshType;

    for (const ufbx_mesh* mesh : scene->meshes) {
        auto lmesh = convert_mesh_ufbx_to_lagrange<MeshType>(mesh, opt);
        element_index[mesh->element_id] = lscene.add(std::move(lmesh));
    }

    for (const ufbx_light* light : scene->lights) {
        element_index[light->element_id] = lscene.lights.size();

        scene::Light llight;
        llight.name = light->name.data;
        llight.color_ambient = llight.color_diffuse = llight.color_specular = to_vec3(light->color);
        llight.intensity = light->intensity;
        llight.direction = to_vec3(light->local_direction);
        llight.type = [](ufbx_light_type type) -> scene::Light::Type {
            switch (type) {
            // note that ufbx does not define ambient light
            case UFBX_LIGHT_POINT: return scene::Light::Type::Point;
            case UFBX_LIGHT_DIRECTIONAL: return scene::Light::Type::Directional;
            case UFBX_LIGHT_SPOT: return scene::Light::Type::Spot;
            case UFBX_LIGHT_AREA: return scene::Light::Type::Area;
            case UFBX_LIGHT_VOLUME: // passthrough, unsupported
            default: return scene::Light::Type::Undefined;
            }
        }(light->type);
        llight.attenuation_constant = (light->decay == UFBX_LIGHT_DECAY_NONE ? 1.f : 0.f);
        llight.attenuation_linear = (light->decay == UFBX_LIGHT_DECAY_LINEAR ? 1.f : 0.f);
        llight.attenuation_quadratic = (light->decay == UFBX_LIGHT_DECAY_QUADRATIC ? 1.f : 0.f);
        llight.attenuation_cubic = (light->decay == UFBX_LIGHT_DECAY_CUBIC ? 1.f : 0.f);
        // light->area_shape is unsupported
        llight.angle_inner_cone = light->inner_angle;
        llight.angle_outer_cone = light->outer_angle;
        // light->cast_light is unsupported
        // light->cast_shadows is unsupported
        lscene.lights.push_back(llight);
    }

    for (const ufbx_camera* camera : scene->cameras) {
        element_index[camera->element_id] = lscene.cameras.size();
        // ufbx has a lot of information here. Some is useful, some is redundant, and some is lost.
        scene::Camera lcam;
        lcam.name = camera->name.data;
        if (camera->projection_mode == UFBX_PROJECTION_MODE_PERSPECTIVE) {
            lcam.type = scene::Camera::Type::Perspective;
            lcam.horizontal_fov = lagrange::to_radians(camera->field_of_view_deg.x);
        } else {
            lcam.type = scene::Camera::Type::Orthographic;
            lcam.orthographic_width = camera->orthographic_size.y;
        }
        lcam.aspect_ratio = camera->aspect_ratio;
        lcam.near_plane = camera->near_plane;
        lcam.far_plane = camera->far_plane;

        lscene.cameras.push_back(lcam);
    }

    auto convert_map_mode = [](ufbx_wrap_mode mode) -> scene::Texture::WrapMode {
        switch (mode) {
        case UFBX_WRAP_REPEAT: return scene::Texture::WrapMode::Wrap;
        case UFBX_WRAP_CLAMP: return scene::Texture::WrapMode::Clamp;
        default: return scene::Texture::WrapMode::Wrap;
        }
    };
    for (const ufbx_texture* texture : scene->textures) {
        element_index[texture->element_id] = lscene.textures.size();
        int img_idx = static_cast<int>(lscene.images.size());

        scene::Texture ltex;
        ltex.name = texture->name.data;
        ltex.wrap_u = convert_map_mode(texture->wrap_u);
        ltex.wrap_v = convert_map_mode(texture->wrap_v);
        if (texture->has_uv_transform) {
            ltex.offset(0) = texture->uv_transform.translation.x;
            ltex.offset(1) = texture->uv_transform.translation.y;
            ltex.scale(0) = texture->uv_transform.scale.x;
            ltex.scale(1) = texture->uv_transform.scale.y;
        }
        ltex.image = img_idx;
        lscene.textures.push_back(ltex);

        scene::ImageExperimental limage;
        limage.name = texture->name.data;
        limage.uri = texture->relative_filename.data;
        // note: there is no width/height anywhere. read the image from disk, or read png from texture->content.
        bool loaded = false;
        if (texture->content.size > 0) {
            // TODO: read image from embedded data.
            // But our image_io module does not support loading from buffer
            logger().warn(
                "Loading fbx embedded textures is currently unsupported, missing data for {}",
                limage.name);
        } else if (opt.load_images) {
            loaded |= internal::try_load_image(texture->relative_filename.data, opt, limage);
            loaded |= internal::try_load_image(texture->absolute_filename.data, opt, limage);
        } else {
            // opt.load_images is false: don't load, but consider it ok.
            loaded = true;
        }

        if (loaded) {
            lscene.add(std::move(limage));
        }
    }

    auto try_load_texture = [&](const ufbx_texture* texture, scene::TextureInfo& tex_info) -> bool {
        if (texture == nullptr) return false;
        if (tex_info.index != invalid<scene::ElementId>()) return false;
        tex_info.index = static_cast<int>(element_index[texture->element_id]);
        return true;
    };
    for (const ufbx_material* material : scene->materials) {
        element_index[material->element_id] = lscene.materials.size();

        scene::MaterialExperimental lmat;
        lmat.name = material->name.data;
        if (material->pbr.base_color.has_value) {
            lmat.base_color_value = to_vec4(material->pbr.base_color.value_vec4);
        }
        if (material->pbr.emission_color.has_value) {
            lmat.emissive_value = to_vec3(material->pbr.emission_color.value_vec3);
        }
        try_load_texture(material->pbr.base_color.texture, lmat.base_color_texture);
        try_load_texture(material->pbr.roughness.texture, lmat.metallic_roughness_texture);
        try_load_texture(material->pbr.metalness.texture, lmat.metallic_roughness_texture);
        try_load_texture(material->pbr.normal_map.texture, lmat.normal_texture);

        lscene.materials.push_back(lmat);
    }

    // TODO fbx has a ton of extra information. We want to support some of it, but probably not all.
    // This includes:
    // - animations, bones, deformers, poses
    // - line curves, nurbs curves and surfaces
    // - procedural geometries
    // - videos

    std::function<size_t(const ufbx_node*, size_t parent_idx)> create_node;
    create_node = [&element_index,
                   &lscene,
                   &create_node](const ufbx_node* node, size_t parent_idx) -> size_t {
        // define node
        size_t node_idx = lscene.nodes.size();
        lscene.nodes.push_back(scene::Node());
        scene::Node& lnode = lscene.nodes[node_idx];

        lnode.name = std::string(node->name.data);
        lnode.transform =
            convert_transform_ufbx_to_lagrange<Eigen::Affine3f>(&node->local_transform);
        lnode.parent = parent_idx;

        if (node->mesh) {
            size_t mesh_idx = element_index[node->mesh->element_id];
            la_runtime_assert(mesh_idx != lagrange::invalid<size_t>());
            std::vector<size_t> material_idxs;

            for (const ufbx_material* material : node->materials) {
                if (material) {
                    material_idxs.push_back(element_index[material->element_id]);
                }
            }
            lnode.meshes.push_back({mesh_idx, material_idxs});
        }

        size_t children_count = node->children.count;
        lnode.children.resize(children_count);
        for (size_t i = 0; i < children_count; ++i) {
            lnode.children[i] = create_node(node->children[i], node_idx);
        }

        return node_idx;
    };

    lscene.nodes.reserve(scene->nodes.count);
    size_t root_index = create_node(scene->root_node, lagrange::invalid<size_t>());
    lscene.root_nodes.push_back(root_index);

    return lscene;
}

void display_ufbx_scene_warnings(const ufbx_scene* scene)
{
    const ufbx_metadata& metadata = scene->metadata;
    for (const ufbx_warning& warning : metadata.warnings) {
        logger().warn(
            "fbx loader warning: {} (happened {} times)",
            warning.description.data,
            warning.count);
    }
    if (metadata.may_contain_no_index)
        logger().warn("fbx warning: index arrays may contain invalid indices");
    if (metadata.may_contain_null_materials)
        logger().warn("fbx warning: file may contain null materials");
    if (metadata.may_contain_missing_vertex_position)
        logger().warn("fbx warning: vertex positions may be missing");
    if (metadata.may_contain_broken_elements)
        logger().warn("fbx warning: arrays may contain null element references");
}

} // namespace

// =====================================
// load_mesh_fbx.h
// =====================================
template <typename MeshType>
MeshType load_mesh_fbx(std::istream& input_stream, const LoadOptions& options)
{
    using Scalar = typename MeshType::Scalar;
    using Index = typename MeshType::Index;
    using SceneType = scene::SimpleScene<Scalar, Index, 3u>; // we always load 3d scenes
    return scene::simple_scene_to_mesh(load_simple_scene_fbx<SceneType>(input_stream, options));
}

template <typename MeshType>
MeshType load_mesh_fbx(const fs::path& filename, const LoadOptions& options)
{
    using Scalar = typename MeshType::Scalar;
    using Index = typename MeshType::Index;
    using SceneType = scene::SimpleScene<Scalar, Index, 3u>; // we always load 3d scenes
    return scene::simple_scene_to_mesh(load_simple_scene_fbx<SceneType>(filename, options));
}

// =====================================
// load_simple_scene_fbx.h
// =====================================
template <typename SceneType>
SceneType load_simple_scene_fbx(const fs::path& filename, const LoadOptions& options)
{
    auto scene = load_ufbx(filename);
    display_ufbx_scene_warnings(scene.ptr);
    SceneType lscene = load_simple_scene_fbx<SceneType>(scene.ptr, options);
    return lscene;
}
template <typename SceneType>
SceneType load_simple_scene_fbx(std::istream& input_stream, const LoadOptions& options)
{
    auto scene = load_ufbx(input_stream);
    display_ufbx_scene_warnings(scene.ptr);
    SceneType lscene = load_simple_scene_fbx<SceneType>(scene.ptr, options);
    return lscene;
}

// =====================================
// load_scene_fbx.h
// =====================================
template <typename SceneType>
SceneType load_scene_fbx(const fs::path& filename, const LoadOptions& options)
{
    auto scene = load_ufbx(filename);
    display_ufbx_scene_warnings(scene.ptr);
    SceneType lscene = load_scene_fbx<SceneType>(scene.ptr, options);
    return lscene;
}
template <typename SceneType>
SceneType load_scene_fbx(std::istream& input_stream, const LoadOptions& options)
{
    auto scene = load_ufbx(input_stream);
    display_ufbx_scene_warnings(scene.ptr);
    SceneType lscene = load_scene_fbx<SceneType>(scene.ptr, options);
    return lscene;
}

#define LA_X_load_mesh_fbx(_, S, I)                                                          \
    template LA_IO_API SurfaceMesh<S, I> load_mesh_fbx(const fs::path&, const LoadOptions&); \
    template LA_IO_API SurfaceMesh<S, I> load_mesh_fbx(std::istream&, const LoadOptions&);
LA_SURFACE_MESH_X(load_mesh_fbx, 0);
#undef LA_X_load_mesh_fbx

#define LA_X_load_simple_scene_fbx(_, S, I, D)                            \
    template LA_IO_API scene::SimpleScene<S, I, D> load_simple_scene_fbx( \
        const fs::path& filename,                                         \
        const LoadOptions& options);                                      \
    template LA_IO_API scene::SimpleScene<S, I, D> load_simple_scene_fbx( \
        std::istream& input_stream,                                       \
        const LoadOptions& options);
LA_SIMPLE_SCENE_X(load_simple_scene_fbx, 0);
#undef LA_X_load_simple_scene_fbx

#define LA_X_load_scene_fbx(_, S, I)                      \
    template LA_IO_API scene::Scene<S, I> load_scene_fbx( \
        const fs::path& filename,                         \
        const LoadOptions& options);                      \
    template LA_IO_API scene::Scene<S, I> load_scene_fbx( \
        std::istream& input_stream,                       \
        const LoadOptions& options);
LA_SCENE_X(load_scene_fbx, 0);
#undef LA_X_load_scene_fbx

} // namespace lagrange::io
