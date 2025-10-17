/*
 * Copyright 2021 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#pragma once

#include <lagrange/ui/Entity.h>
#include <lagrange/ui/api.h>
#include <lagrange/ui/default_components.h>
#include <lagrange/ui/default_shaders.h>
#include <lagrange/ui/utils/io.h>
#include <lagrange/ui/utils/treenode.h>

namespace lagrange {
namespace ui {

LA_UI_API void set_material(Registry& r, Entity meshrender_entity, std::shared_ptr<Material> mat);

LA_UI_API Entity show_mesh(
    Registry& r,
    const Entity& mesh_entity,
    StringID shader = DefaultShaders::PBR,
    const ShaderDefines& shader_defines = {});
LA_UI_API Entity show_submesh(
    Registry& r,
    const Entity& mesh_entity,
    std::shared_ptr<Material> material,
    entt::id_type submesh_id);

LA_UI_API Entity show_mesh(
    Registry& r,
    Entity mesh_entity,
    Entity scene_node_entity,
    StringID shader = DefaultShaders::PBR,
    const ShaderDefines& shader_defines = {});

LA_UI_API Entity show_mesh(
    Registry& r,
    Entity mesh_entity,
    Entity scene_node_entity,
    std::shared_ptr<Material> material);


/*
    Attribute visualization
*/
LA_UI_API Entity show_vertex_attribute(
    Registry& r,
    const Entity& mesh_entity,
    const std::string& attribute,
    Glyph glyph);

LA_UI_API Entity show_facet_attribute(
    Registry& r,
    const Entity& mesh_entity,
    const std::string& attribute,
    Glyph glyph);

LA_UI_API Entity show_edge_attribute(
    Registry& r,
    const Entity& mesh_entity,
    const std::string& attribute,
    Glyph glyph);

LA_UI_API Entity show_corner_attribute(
    Registry& r,
    const Entity& mesh_entity,
    const std::string& attribute,
    Glyph glyph);

LA_UI_API Entity show_indexed_attribute(
    Registry& r,
    const Entity& mesh_entity,
    const std::string& attribute,
    Glyph glyph);


LA_UI_API void
set_colormap(Registry& r, Entity meshrender_entity, std::shared_ptr<Texture> texture);

LA_UI_API void set_colormap_range(
    Registry& r,
    Entity meshrender_entity,
    const Eigen::Vector4f& range_min,
    const Eigen::Vector4f& range_max);


LA_UI_API void set_colormap_range(
    Registry& r,
    Entity meshrender_entity,
    const std::pair<Eigen::VectorXf, Eigen::VectorXf>& range);


LA_UI_API std::shared_ptr<Material> get_material(Registry& r, Entity meshrender_entity);

inline Transform& get_transform(Registry& r, Entity e)
{
    return r.get<Transform>(e);
}


template <typename Derived>
inline Eigen::Affine3f& set_transform(Registry& r, Entity e, const Derived& local_transform)
{
    auto& t = r.get<Transform>(e).local;
    t = local_transform;
    return t;
}

template <typename Derived>
inline Eigen::Affine3f& apply_transform(Registry& r, Entity e, const Derived& local_transform)
{
    Eigen::Affine3f t;
    t = local_transform; // Convert to affine
    return set_transform(r, e, t * get_transform(r, e).local);
}

/*
    Mesh
*/
template <typename MeshType>
Entity add_mesh(
    Registry& r,
    std::shared_ptr<MeshType> mesh,
    const std::string& name = "Unnamed mesh",
    StringID shader = DefaultShaders::PBR)
{
    auto mesh_geometry = register_mesh<MeshType>(r, std::move(mesh));
    auto mesh_view = show_mesh(r, mesh_geometry, shader);
    ui::set_name(r, mesh_geometry, name);
    ui::set_name(r, mesh_view, name);
    return mesh_view;
}

template <typename MeshType>
Entity add_mesh(
    Registry& r,
    std::unique_ptr<MeshType> mesh,
    const std::string& name = "Unnamed mesh",
    StringID shader = DefaultShaders::PBR)
{
    return add_mesh(r, to_shared_ptr(std::move(mesh)), name, shader);
}

template <typename MeshType>
Entity load_mesh(
    Registry& r,
    const lagrange::fs::path& path_to_obj,
    bool load_materials = true,
    const std::string& name = "Unnamed mesh",
    StringID shader = DefaultShaders::PBR)
{
    if (!load_materials) {
        auto me = load_obj<MeshType>(r, path_to_obj);
        if (r.valid(me)) {
            auto e = show_mesh(r, me, shader);
            ui::set_name(r, e, name);
            return e;
        }
    } else {
        auto [me, mats] = load_obj_with_materials<MeshType>(r, path_to_obj);

        if (!r.valid(me)) return NullEntity;

        if (mats.size() <= 1) {
            auto e = show_mesh(r, me, shader);
            ui::set_name(r, e, name);
            if (mats.size() == 1) ui::set_material(r, e, mats.front());
            return e;
        } else {
            auto e = create_scene_node(r, name);
            for (auto mat_index = 0; mat_index < mats.size(); mat_index++) {
                auto sub = ui::show_submesh(r, me, mats[mat_index], mat_index);
                ui::set_name(r, sub, lagrange::string_format("{} submesh {}", name, mat_index));
                ui::set_parent(r, sub, e);
            }
            return e;
        }
    }

    return NullEntity;
}

/*
 * Mesh update
 */
LA_UI_API void set_mesh_vertices_dirty(Registry& r, Entity mesh_entity);
LA_UI_API void set_mesh_normals_dirty(Registry& r, Entity mesh_entity);
LA_UI_API void set_mesh_dirty(Registry& r, Entity mesh_entity);

LA_UI_API void set_show_attribute_dirty(Registry& r, Entity scene_entity);
LA_UI_API void set_mesh_attribute_dirty(
    Registry& r,
    Entity mesh_entity,
    IndexingMode mode,
    const std::string& name);

LA_UI_API Entity get_meshdata_entity(Registry& r, Entity scene_entity);
LA_UI_API MeshData& get_meshdata(Registry& r, Entity scene_or_mesh_entity);


/*
    Material
*/
std::shared_ptr<Material> LA_UI_API
create_material(Registry& r, entt::id_type shader_id, const ShaderDefines& shader_defines = {});


LA_UI_API Entity add_camera(Registry& r, const Camera& camera = Camera::default_camera(1, 1));


// Clears all user added entities
LA_UI_API void clear_scene(Registry& r);

/// Intersect ray with meshes in root's hierarchy
/// Returns a pair of intersected entity and the corresponding hit
/// If root == ui::NullEntity, entire scene is traversed
LA_UI_API std::optional<std::pair<Entity, RayFacetHit>> intersect_ray(
    Registry& r,
    const Eigen::Vector3f& origin,
    const Eigen::Vector3f& dir,
    ui::Entity root = ui::NullEntity,
    Layer visible_layers = Layer(true),
    Layer hidden_layers = Layer(false));


} // namespace ui
} // namespace lagrange
