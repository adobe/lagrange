/*
 * Copyright 2020 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */

#include <lagrange/ui/default_entities.h>
#include <lagrange/ui/default_shaders.h>
#include <lagrange/ui/utils/bounds.h>
#include <lagrange/ui/utils/mesh.h>
#include <lagrange/ui/utils/treenode.h>
#include <lagrange/utils/la_assert.h>

namespace lagrange {
namespace ui {


std::shared_ptr<Material> create_material(Registry& r, entt::id_type shader_id)
{
    return std::make_shared<Material>(r, shader_id);
}

PrimitiveType get_raster_primitive(Glyph g) {
    switch (g) {
    case lagrange::ui::Glyph::Surface: return PrimitiveType::TRIANGLES;
    case lagrange::ui::Glyph::Line: return PrimitiveType::LINES;
    case lagrange::ui::Glyph::Wire: return PrimitiveType::TRIANGLES;
    case lagrange::ui::Glyph::Arrow: return PrimitiveType::TRIANGLES;
    case lagrange::ui::Glyph::Point: return PrimitiveType::POINTS;
    }
    return PrimitiveType::TRIANGLES;
}

Entity show_attribute_surface(
    Registry& registry,
    const Entity& mesh_entity,
    IndexingMode attribute_type,
    const std::string source_attribute,
    PrimitiveType raster_primitive,
    StringID shader_id = DefaultShaders::SurfaceVertexAttribute)
{
    auto e = show_mesh(registry, mesh_entity, shader_id);
    auto& mat = *get_material(registry, e);
    auto& mr = registry.get<MeshRender>(e);
    auto& md = registry.get<MeshData>(mesh_entity);

    set_colormap_range(registry, e, get_mesh_attribute_range(md, attribute_type, source_attribute));

    AttributeRender ar;
    ar.attribute_type = attribute_type;
    ar.source_attribute = source_attribute;

    registry.emplace<AttributeRender>(e, std::move(ar));

    mr.indexing = IndexingMode::CORNER;
    mr.primitive = PrimitiveType::TRIANGLES;


    if (raster_primitive == PrimitiveType::LINES) {
        mat.set_int(RasterizerOptions::PolygonMode, GL_LINE);
    }
    else if (raster_primitive == PrimitiveType::POINTS) {
        mat.set_int(RasterizerOptions::PolygonMode, GL_POINT);
        mat.set_int(RasterizerOptions::PointSize, 3);
    } else {
        mat.set_int(RasterizerOptions::PolygonMode, GL_FILL);
    }

    return e;
}

void set_colormap(Registry& registry, Entity meshrender_entity, std::shared_ptr<Texture> texture)
{
    LA_ASSERT(registry.valid(meshrender_entity), "Invalid entity");
    LA_ASSERT(registry.has<MeshRender>(meshrender_entity), "Entity must have MeshRender component");

    auto mat = get_material(registry, meshrender_entity);
    mat->set_texture("colormap", texture);
}


void set_colormap_range(
    Registry& registry,
    Entity meshrender_entity,
    const Eigen::Vector4f& range_min,
    const Eigen::Vector4f& range_max)
{
    LA_ASSERT(registry.valid(meshrender_entity), "Invalid entity");
    LA_ASSERT(registry.has<MeshRender>(meshrender_entity), "Entity must have MeshRender component");

    auto& mr = registry.get<MeshRender>(meshrender_entity);

    mr.material->set_vec4("range_min", range_min);
    mr.material->set_vec4("range_max", range_max);
}

void set_colormap_range(
    Registry& registry,
    Entity meshrender_entity,
    const std::pair<Eigen::VectorXf, Eigen::VectorXf>& range)
{
    auto range_cpy = range;
    range_cpy.first.conservativeResizeLike(Eigen::Vector4f::Zero());
    range_cpy.second.conservativeResizeLike(Eigen::Vector4f::Zero());

    Eigen::Vector4f value_min = range_cpy.first;
    Eigen::Vector4f value_max = range_cpy.second;
    set_colormap_range(registry, meshrender_entity, value_min, value_max);
}

std::shared_ptr<Material> get_material(Registry& registry, Entity meshrender_entity)
{
    LA_ASSERT(registry.valid(meshrender_entity), "Invalid entity");
    LA_ASSERT(registry.has<MeshRender>(meshrender_entity), "Entity must have MeshRender component");

    auto& mr = registry.get<MeshRender>(meshrender_entity);
    return mr.material;
}

void set_material(Registry& registry, Entity meshrender_entity, std::shared_ptr<Material> mat)
{
    LA_ASSERT(registry.valid(meshrender_entity), "Invalid entity");
    LA_ASSERT(registry.has<MeshRender>(meshrender_entity), "Entity must have MeshRender component");

    auto& mr = registry.get<MeshRender>(meshrender_entity);
    mr.material = mat;
}

void set_mesh_vertices_dirty(Registry& registry, Entity mesh_entity)
{
    registry.get_or_emplace<MeshDataDirty>(mesh_entity).vertices = true;
}

void set_mesh_normals_dirty(Registry& registry, Entity mesh_entity)
{
    registry.get_or_emplace<MeshDataDirty>(mesh_entity).normals = true;
}

void set_mesh_dirty(Registry& registry, Entity mesh_entity)
{
    registry.get_or_emplace<MeshDataDirty>(mesh_entity).all = true;
}

void set_show_attribute_dirty(Registry& registry, Entity scene_entity)
{
    auto& ar = registry.get<AttributeRender>(scene_entity);
    ar.dirty = true;

    // Update colormap if applicable
    if (registry.has<MeshRender>(scene_entity)) {
        ui::set_colormap_range(
            registry,
            scene_entity,
            ui::get_mesh_attribute_range(
                ui::get_meshdata(registry, scene_entity),
                ar.attribute_type,
                ar.source_attribute));
    }
}

void set_mesh_attribute_dirty(
    Registry& registry,
    Entity mesh_entity,
    IndexingMode attribute_type,
    const std::string& name)
{
    registry.view<AttributeRender, MeshGeometry>().each(
        [&](Entity e, AttributeRender& ar, MeshGeometry& mg) {
            if (mg.entity != mesh_entity) return;
            if (ar.attribute_type != attribute_type) return;
            if (ar.source_attribute != name) return;
            set_show_attribute_dirty(registry, e);
        });
}

Entity get_meshdata_entity(Registry& registry, Entity scene_entity)
{
    LA_ASSERT(registry.valid(scene_entity) && registry.has<MeshGeometry>(scene_entity));
    auto g = registry.get<MeshGeometry>(scene_entity).entity;
    LA_ASSERT(registry.valid(g));
    return g;
}

MeshData& get_meshdata(Registry& registry, Entity scene_or_mesh_entity)
{
    if (registry.has<MeshData>(scene_or_mesh_entity))
        return registry.get<MeshData>(scene_or_mesh_entity);
    return registry.get<MeshData>(get_meshdata_entity(registry, scene_or_mesh_entity));
}

Entity show_vertex_attribute(
    Registry& registry,
    const Entity& mesh_entity,
    const std::string& attribute,
    Glyph glyph)
{
    LA_ASSERT(registry.has<MeshData>(mesh_entity), "mesh_entity must have MeshData component");
    auto& md = registry.get<MeshData>(mesh_entity);
    LA_ASSERT(
        md.mesh && has_mesh_vertex_attribute(md, attribute),
        "Mesh must have the specified vertex attribute");

    Entity e = ui::NullEntity;

    if (glyph == Glyph::Surface ||  glyph == Glyph::Point) {
        e = show_attribute_surface(registry, mesh_entity, IndexingMode::VERTEX, attribute, get_raster_primitive(glyph));
    } else if (glyph == Glyph::Line) {
        e = show_attribute_surface(
            registry,
            mesh_entity,
            IndexingMode::VERTEX,
            attribute,
            PrimitiveType::TRIANGLES,
            DefaultShaders::LineVertexAttribute);
    }

    if (e == ui::NullEntity) {
        lagrange::logger().error("Unsupported glyph");
        return e;
    }

    set_name(registry, e, "[Vertex Attribute] " + attribute);

    return e;
}

Entity show_facet_attribute(
    Registry& registry,
    const Entity& mesh_entity,
    const std::string& attribute,
    Glyph glyph)
{
    LA_ASSERT(registry.has<MeshData>(mesh_entity), "mesh_entity must have MeshData component");
    auto& md = registry.get<MeshData>(mesh_entity);
    LA_ASSERT(
        md.mesh && has_mesh_facet_attribute(md, attribute),
        "Mesh must have the specified facet attribute");

    Entity e = ui::NullEntity;

    if (glyph == Glyph::Surface || glyph == Glyph::Point) {
        e = show_attribute_surface(registry, mesh_entity, IndexingMode::FACE, attribute, get_raster_primitive(glyph));
    } else if (glyph == Glyph::Line) {
        e = show_attribute_surface(
            registry,
            mesh_entity,
            IndexingMode::FACE,
            attribute,
            PrimitiveType::TRIANGLES,
            DefaultShaders::LineVertexAttribute);
    }

    if (e == ui::NullEntity) {
        lagrange::logger().error("Unsupported glyph");
        return e;
    }

    set_name(registry, e, "[Facet Attribute] " + attribute);

    return e;
}

Entity show_corner_attribute(
    Registry& registry,
    const Entity& mesh_entity,
    const std::string& attribute,
    Glyph glyph)
{
    LA_ASSERT(registry.has<MeshData>(mesh_entity), "mesh_entity must have MeshData component");
    auto& md = registry.get<MeshData>(mesh_entity);
    LA_ASSERT(
        md.mesh && has_mesh_corner_attribute(md, attribute),
        "Mesh must have the specified corner attribute");

    Entity e = ui::NullEntity;

    if (glyph == Glyph::Surface ||glyph == Glyph::Point) {
        e = show_attribute_surface(
            registry,
            mesh_entity,
            IndexingMode::CORNER,
            attribute,
            get_raster_primitive(glyph));
    } else if (glyph == Glyph::Line) {
        e = show_attribute_surface(
            registry,
            mesh_entity,
            IndexingMode::CORNER,
            attribute,
            PrimitiveType::TRIANGLES,
            DefaultShaders::LineVertexAttribute);
    }

    if (e == ui::NullEntity) {
        lagrange::logger().error("Unsupported glyph");
        return e;
    }

    set_name(registry, e, "[Corner Attribute] " + attribute);

    return e;
}


Entity show_indexed_attribute(
    Registry& r,
    const Entity& mesh_entity,
    const std::string& attribute,
    Glyph glyph)
{
    LA_ASSERT(r.has<MeshData>(mesh_entity), "mesh_entity must have MeshData component");
    auto& md = r.get<MeshData>(mesh_entity);
    LA_ASSERT(
        md.mesh && has_mesh_indexed_attribute(md, attribute),
        "Mesh must have the specified indexed attribute");

    Entity e = ui::NullEntity;

    if (glyph == Glyph::Surface || glyph == Glyph::Point) {
        map_indexed_attribute_to_corner_attribute(md, attribute);
        e = show_attribute_surface(
            r,
            mesh_entity,
            IndexingMode::CORNER,
            attribute,
            get_raster_primitive(glyph));
    } else if (glyph == Glyph::Line) {
        map_indexed_attribute_to_corner_attribute(md, attribute);
        e = show_attribute_surface(
            r,
            mesh_entity,
            IndexingMode::CORNER,
            attribute,
            PrimitiveType::TRIANGLES,
            DefaultShaders::LineVertexAttribute);
    }

    if (e == ui::NullEntity) {
        lagrange::logger().error("Unsupported glyph");
        return e;
    }

    set_name(r, e, "[Indexed Attribute] " + attribute);

    return e;
}

Entity show_edge_attribute(
    Registry& registry,
    const Entity& mesh_entity,
    const std::string& attribute,
    Glyph glyph)
{
    LA_ASSERT("mesh_entity must have MeshData component", registry.has<MeshData>(mesh_entity));
    auto& md = registry.get<MeshData>(mesh_entity);
    LA_ASSERT(
        "Mesh must have the specified edge attribute",
        md.mesh && has_mesh_edge_attribute(md, attribute));

    Entity e = ui::NullEntity;

    if (glyph == Glyph::Surface || glyph == Glyph::Point) {
        e = show_attribute_surface(
            registry,
            mesh_entity,
            IndexingMode::EDGE,
            attribute,
            get_raster_primitive(glyph),
            DefaultShaders::SurfaceEdgeAttribute);
    } else if (glyph == Glyph::Line) {
        e = show_attribute_surface(
            registry,
            mesh_entity,
            IndexingMode::EDGE,
            attribute,
            PrimitiveType::TRIANGLES,
            DefaultShaders::LineVertexAttribute);
    }

    if (e == ui::NullEntity) {
        lagrange::logger().error("Unsupported glyph");
        return e;
    }

    set_name(registry, e, "[Edge Attribute] " + attribute);

    return e;
}

Entity add_camera(Registry& registry, const Camera& camera)
{
    auto e = registry.create();
    registry.emplace<Camera>(e, std::move(camera));
    registry.emplace<CameraController>(e);
    registry.emplace<Name>(e, "Camera");
    registry.emplace<TreeNode>(e);
    return e;
}


void clear_scene(Registry& r)
{
    // Do not delete viewport cameras
    std::unordered_set<Entity> protected_entities;
    r.view<ViewportComponent>().each(
        [&](Entity /*e*/, ViewportComponent& v) { protected_entities.insert(v.camera_reference); });

    // Otherwise delete everything with Tree
    r.view<TreeNode>().each([&](Entity e, TreeNode& /*t*/) {
        if (protected_entities.count(e)) return;
        r.destroy(e);
    });
}

Entity show_mesh(Registry& registry, const Entity& mesh_entity, StringID shader_id)
{
    return show_mesh(registry, mesh_entity, NullEntity, shader_id);
}

Entity show_submesh(
    Registry& r,
    const Entity& mesh_entity,
    std::shared_ptr<Material> material,
    entt::id_type material_id)
{
    auto e = show_mesh(r, mesh_entity, NullEntity, std::move(material));
    r.get<ui::MeshGeometry>(e).submesh_index = material_id;
    return e;
}


Entity show_mesh(
    Registry& r,
    Entity mesh_entity,
    Entity scene_node_entity,
    StringID shader /*= DefaultShaders::PBR*/)
{
    return show_mesh(r, mesh_entity, scene_node_entity, ui::create_material(r, shader));
}

Entity show_mesh(
    Registry& r,
    Entity mesh_entity,
    Entity scene_node_entity,
    std::shared_ptr<Material> material)
{
    if (scene_node_entity == NullEntity) scene_node_entity = create_scene_node(r, ui::get_name(r, mesh_entity));
    r.emplace<MeshGeometry>(scene_node_entity, mesh_entity);

    MeshRender mr;
    mr.material = std::move(material);
    r.emplace<MeshRender>(scene_node_entity, std::move(mr));

    return scene_node_entity;
}


} // namespace ui
} // namespace lagrange
