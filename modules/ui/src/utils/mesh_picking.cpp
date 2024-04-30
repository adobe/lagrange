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
#include <lagrange/ui/components/AcceleratedPicking.h>
#include <lagrange/ui/components/AttributeRender.h>
#include <lagrange/ui/components/Common.h>
#include <lagrange/ui/components/MeshGeometry.h>
#include <lagrange/ui/components/MeshRender.h>
#include <lagrange/ui/components/MeshSelectionRender.h>
#include <lagrange/ui/components/ObjectIDViewport.h>
#include <lagrange/ui/components/Transform.h>
#include <lagrange/ui/default_shaders.h>
#include <lagrange/ui/default_tools.h>
#include <lagrange/ui/systems/render_viewports.h>
#include <lagrange/ui/utils/colormap.h>
#include <lagrange/ui/utils/layer.h>
#include <lagrange/ui/utils/mesh_picking.h>
#include <lagrange/ui/utils/objectid_viewport.h>
#include <lagrange/ui/utils/selection.h>
#include <lagrange/ui/utils/tools.h>
#include <lagrange/ui/utils/viewport.h>

#include <lagrange/ui/default_entities.h>

namespace {
using namespace lagrange::ui;
void update_meshselection_render(Registry& r, Entity selected_entity)
{
    const auto& sr = r.get<MeshSelectionRender>(selected_entity);

    if (r.all_of<Layer>(selected_entity)) {
        const auto& layer = r.get<Layer>(selected_entity);
        r.emplace_or_replace<Layer>(sr.facet_render, layer);
        r.emplace_or_replace<Layer>(sr.edge_render, layer);
        r.emplace_or_replace<Layer>(sr.vertex_render, layer);
    }

    // Copy transform from selected entity
    // _render entities are not part of the scene tree
    // and thus require absolute transform
    if (r.all_of<Transform>(selected_entity)) {
        const auto& transform = r.get<Transform>(selected_entity);
        r.emplace_or_replace<Transform>(sr.facet_render, transform);
        r.emplace_or_replace<Transform>(sr.edge_render, transform);
        r.emplace_or_replace<Transform>(sr.vertex_render, transform);
    }

    // Make sure element selection render is on top of the selected entity render
    auto material = get_material(r, selected_entity);
    if (material && material->int_values.count(RasterizerOptions::Pass)) {
        const auto pass = material->int_values.at(RasterizerOptions::Pass);
        sr.mat_faces_in_facet_mode->set_int(RasterizerOptions::Pass, pass);
        sr.mat_faces_default->set_int(RasterizerOptions::Pass, pass);
        sr.mat_edges_in_facet_mode->set_int(RasterizerOptions::Pass, pass);
        sr.mat_edges_in_edge_mode->set_int(RasterizerOptions::Pass, pass);
        sr.mat_edges_in_vertex_mode->set_int(RasterizerOptions::Pass, pass);
        sr.mat_vertices->set_int(RasterizerOptions::Pass, pass);
    }
}
} // namespace

namespace lagrange {
namespace ui {


bool select_visible_elements(
    Registry& r,
    StringID current_element_type,
    SelectionBehavior sel_behavior,
    Entity selected_entity,
    Entity active_viewport,
    Frustum local_frustum)
{
    auto& mesh_data = get_mesh_data(r, selected_entity);
    const auto attrib_name = "is_selected";

    auto& offscreen_viewport = setup_offscreen_viewport(
        r,
        get_objectid_viewport_entity(r),
        active_viewport,
        DefaultShaders::MeshElementID);

    // Only show selected objects
    offscreen_viewport.visible_layers.set(DefaultLayers::Selection, true);
    offscreen_viewport.visible_layers.set(DefaultLayers::Default, false);
    offscreen_viewport.material_override =
        std::make_shared<Material>(r, DefaultShaders::MeshElementID);

    auto [x, y, w, h] = setup_scissor(r, offscreen_viewport, r.ctx().get<SelectionContext>());
    assert(w * h > 0);

    offscreen_viewport.background = Color(1.0f, 1.0f, 1.0f, 1.0f);

    if (is_element_type<ElementFace>(current_element_type)) {
        offscreen_viewport.material_override->set_int("element_mode", 0);
    } else if (is_element_type<ElementEdge>(current_element_type)) {
        offscreen_viewport.material_override->set_int("element_mode", 1);
    } else if (is_element_type<ElementVertex>(current_element_type)) {
        offscreen_viewport.material_override->set_int("element_mode", 2);
    }

    render_viewport(r, r.ctx().get<ObjectIDViewport>().viewport_entity);

    auto& buffer = read_pixels(r, offscreen_viewport, x, y, w, h);

    if (is_element_type<ElementFace>(current_element_type)) {
        select_facets_by_color(mesh_data, attrib_name, sel_behavior, buffer.data(), buffer.size());
        propagate_facet_selection(mesh_data, attrib_name);
    } else if (is_element_type<ElementEdge>(current_element_type)) {
        select_edges_by_color(mesh_data, attrib_name, sel_behavior, buffer.data(), buffer.size());
        propagate_corner_selection(mesh_data, attrib_name);
    } else if (is_element_type<ElementVertex>(current_element_type)) {
        select_vertices_by_color(
            mesh_data,
            attrib_name,
            sel_behavior,
            buffer.data(),
            buffer.size());

        // Select all in frustum (to get precision)
        select_vertices_in_frustum(mesh_data, sel_behavior, local_frustum);
        // Combine with corner selection (to select only visible)
        combine_vertex_and_corner_selection(mesh_data, attrib_name);
    }
    return true;
}


bool select_elements_in_frustum(
    Registry& r,
    StringID element_type,
    SelectionBehavior sel_behavior,
    Entity selected_entity,
    Frustum local_frustum)
{
    auto& mesh_data = get_mesh_data(r, selected_entity);

    if (is_element_type<ElementFace>(element_type)) {
        if (sel_behavior != SelectionBehavior::SET) {
            lagrange::logger().warn(
                "Frustum facet selection does not support add/subtract behavior yet");
        }
        select_facets_in_frustum(mesh_data, sel_behavior, local_frustum);
        propagate_facet_selection(mesh_data, "is_selected");

    } else if (is_element_type<ElementVertex>(element_type)) {
        select_vertices_in_frustum(mesh_data, sel_behavior, local_frustum);
        propagate_vertex_selection(mesh_data, "is_selected");

    } else if (is_element_type<ElementEdge>(element_type)) {
        lagrange::logger().warn("Edges-in-frustum selection not implemented");
        // select_edges_in_frustum(mesh_data, sel_behavior, local_frustum);
        return false;
    }
    return true;
}

void initialize_selection_materials(Registry& r, MeshSelectionRender& sr)
{
    const Color dark_orange = Color(252.0f / 255.0f, 86.0f / 255.0f, 3.0f / 255.0f, 1.0f);
    const Color light_orange = Color(255.0f / 255.0f, 149.0f / 255.0f, 43.0f / 255.0f, 1.0f);

    auto face_show_no_show_colormap = generate_colormap(
        [&](float x) {
            if (x < 1.0f - 1.0f / 1024.0f) return Color::zero();
            return light_orange;
        },
        1024);

    auto binary_colormap = generate_colormap(
        [&](float x) {
            if (x < 1.0f - 1.0f / 1024.0f) return Color::black();
            return dark_orange;
        },
        1024);

    auto edge_interpolation_colormap = generate_colormap([&](float x) {
        Color c = x * dark_orange;
        c.a() = 1.0f;
        return c;
    });

    {
        sr.mat_faces_in_facet_mode =
            std::make_shared<Material>(r, DefaultShaders::SurfaceVertexAttribute);
        sr.mat_faces_in_facet_mode->set_int(RasterizerOptions::Pass, 10);
        sr.mat_faces_in_facet_mode->set_float("opacity", 0.75f);
        sr.mat_faces_in_facet_mode->set_texture("colormap", face_show_no_show_colormap);
        sr.mat_faces_in_facet_mode->set_int(RasterizerOptions::CullFaceEnabled, GL_FALSE);

        sr.mat_faces_default =
            std::make_shared<Material>(r, DefaultShaders::SurfaceVertexAttribute);
        sr.mat_faces_default->set_int(RasterizerOptions::Pass, 10);
        sr.mat_faces_default->set_int(RasterizerOptions::CullFaceEnabled, GL_FALSE);
        sr.mat_faces_default->set_float("opacity", 0.1f);
        sr.mat_faces_default->set_texture("colormap", face_show_no_show_colormap);
    }

    {
        sr.mat_edges_in_facet_mode =
            std::make_shared<Material>(r, DefaultShaders::SurfaceVertexAttribute);
        sr.mat_edges_in_facet_mode->set_int(RasterizerOptions::Pass, 11);
        sr.mat_edges_in_facet_mode->set_int(RasterizerOptions::PolygonMode, GL_LINE);
        sr.mat_edges_in_facet_mode->set_int(RasterizerOptions::CullFaceEnabled, GL_FALSE);
        sr.mat_edges_in_facet_mode->set_float("opacity", 0.75f);
        sr.mat_edges_in_facet_mode->set_texture("colormap", binary_colormap);


        sr.mat_edges_in_edge_mode =
            std::make_shared<Material>(r, DefaultShaders::SurfaceVertexAttribute);
        sr.mat_edges_in_edge_mode->set_int(RasterizerOptions::Pass, 11);
        sr.mat_edges_in_edge_mode->set_int(RasterizerOptions::PolygonMode, GL_LINE);
        sr.mat_edges_in_edge_mode->set_int(RasterizerOptions::CullFaceEnabled, GL_FALSE);
        sr.mat_edges_in_edge_mode->set_float("opacity", 0.85f);
        sr.mat_edges_in_edge_mode->set_texture("colormap", binary_colormap);

        sr.mat_edges_in_vertex_mode =
            std::make_shared<Material>(r, DefaultShaders::SurfaceVertexAttribute);
        sr.mat_edges_in_vertex_mode->set_int(RasterizerOptions::Pass, 11);
        sr.mat_edges_in_vertex_mode->set_int(RasterizerOptions::PolygonMode, GL_LINE);
        sr.mat_edges_in_vertex_mode->set_int(RasterizerOptions::CullFaceEnabled, GL_FALSE);
        sr.mat_edges_in_vertex_mode->set_float("opacity", 0.75f);
        sr.mat_edges_in_vertex_mode->set_texture("colormap", edge_interpolation_colormap);
    }

    {
        sr.mat_vertices = std::make_shared<Material>(r, DefaultShaders::SurfaceVertexAttribute);

        sr.mat_vertices->set_int(RasterizerOptions::Pass, 12);
        sr.mat_vertices->set_int(RasterizerOptions::PolygonMode, GL_POINT);
        sr.mat_vertices->set_int(RasterizerOptions::CullFaceEnabled, GL_FALSE);
        sr.mat_vertices->set_float(RasterizerOptions::PointSize, 4.0f);
        sr.mat_vertices->set_texture("colormap", binary_colormap);
    }
}


void clear_element_selection_render(Registry& r, bool exclude_selected)
{
    auto prev_selected = r.view<MeshSelectionRender>();
    for (auto e : prev_selected) {
        if (exclude_selected && r.all_of<Selected>(e)) continue;

        auto& sr = prev_selected.get<MeshSelectionRender>(e);

        r.destroy(sr.facet_render);
        r.destroy(sr.edge_render);
        r.destroy(sr.vertex_render);

        r.remove<MeshSelectionRender>(e);
    }
}


MeshSelectionRender& ensure_selection_render(Registry& r, Entity e)
{
    if (r.all_of<MeshSelectionRender>(e)) {
        update_meshselection_render(r, e);
        return r.get<MeshSelectionRender>(e);
    }

    MeshSelectionRender sr;
    initialize_selection_materials(r, sr);

    sr.facet_render = r.create();
    sr.edge_render = r.create();
    sr.vertex_render = r.create();

    AttributeRender ar;
    ar.attribute_type = IndexingMode::CORNER;
    ar.source_attribute = "is_selected";
    ar.dirty = true;

    MeshRender mr;
    mr.primitive = PrimitiveType::TRIANGLES;
    mr.material = nullptr;

    const auto geom_entity = r.get<MeshGeometry>(e).entity;

    // Facet
    {
        set_name(r, sr.facet_render, "[FACET] Temporary Selection Render");
        r.emplace<MeshGeometry>(sr.facet_render, geom_entity);
        r.emplace<MeshRender>(sr.facet_render, mr);
        r.emplace<AttributeRender>(sr.facet_render, ar);
    }

    // Edge
    {
        set_name(r, sr.edge_render, "[EDGE] Temporary Selection Render");
        r.emplace<MeshGeometry>(sr.edge_render, geom_entity);
        r.emplace<MeshRender>(sr.edge_render, mr);
        r.emplace<AttributeRender>(sr.edge_render, ar);
    }

    // Vertex
    {
        set_name(r, sr.vertex_render, "[VERTEX] Temporary Selection Render");
        r.emplace<MeshGeometry>(sr.vertex_render, geom_entity);
        r.emplace<MeshRender>(sr.vertex_render, mr);
        r.emplace<AttributeRender>(sr.vertex_render, ar);
    }


    r.emplace<MeshSelectionRender>(e, sr); // Link the render entity for reuse


    update_meshselection_render(r, e);


    return r.get<MeshSelectionRender>(e);
}


void update_selection_render(
    Registry& r,
    MeshSelectionRender& sel_render,
    const Entity& selected_mesh_entity,
    const StringID& current_element_type)
{
    // Update transform to match selection
    const auto global_object_transform = r.get<Transform>(selected_mesh_entity).global;
    r.get_or_emplace<Transform>(sel_render.facet_render).local = global_object_transform;
    r.get_or_emplace<Transform>(sel_render.edge_render).local = global_object_transform;
    r.get_or_emplace<Transform>(sel_render.vertex_render).local = global_object_transform;

    if (is_element_type<ElementFace>(current_element_type)) {
        // High opacity selected face
        ui::add_to_layer(r, sel_render.facet_render, DefaultLayers::Default);
        ui::set_material(r, sel_render.facet_render, sel_render.mat_faces_in_facet_mode);


        // Binary color edge, lower opacity
        ui::add_to_layer(r, sel_render.edge_render, DefaultLayers::Default);
        ui::set_material(r, sel_render.edge_render, sel_render.mat_edges_in_facet_mode);

        // No vertices
        ui::remove_from_layer(r, sel_render.vertex_render, DefaultLayers::Default);
    } else if (is_element_type<ElementEdge>(current_element_type)) {
        // Low opacity selected face
        ui::add_to_layer(r, sel_render.facet_render, DefaultLayers::Default);
        ui::set_material(r, sel_render.facet_render, sel_render.mat_faces_default);

        // High opacity binary color edge
        ui::add_to_layer(r, sel_render.edge_render, DefaultLayers::Default);
        ui::set_material(r, sel_render.edge_render, sel_render.mat_edges_in_edge_mode);

        // No vertices
        ui::remove_from_layer(r, sel_render.vertex_render, DefaultLayers::Default);
    } else if (is_element_type<ElementVertex>(current_element_type)) {
        // Low opacity selected faces
        ui::add_to_layer(r, sel_render.facet_render, DefaultLayers::Default);
        ui::set_material(r, sel_render.facet_render, sel_render.mat_faces_default);

        // Low opacity, interpolated color
        ui::add_to_layer(r, sel_render.edge_render, DefaultLayers::Default);
        ui::set_material(r, sel_render.edge_render, sel_render.mat_edges_in_vertex_mode);

        // Vertices ON
        ui::add_to_layer(r, sel_render.vertex_render, DefaultLayers::Default);
        ui::set_material(r, sel_render.vertex_render, sel_render.mat_vertices);
    }
}


void mark_selection_dirty(Registry& r, MeshSelectionRender& sel_render)
{
    r.get<AttributeRender>(sel_render.facet_render).dirty = true;
    // TODO: these share the attribute -> do not upload twice (GLVertexData should have
    // a link field)
    r.get<AttributeRender>(sel_render.edge_render).dirty = true;
    r.get<AttributeRender>(sel_render.vertex_render).dirty = true;
}

bool enable_accelerated_picking(Registry& r, Entity in_e)
{
    auto e = get_mesh_entity(r, in_e);
    if (!r.valid(e)) return false;

    const auto& md = r.get<MeshData>(e);

    AcceleratedPicking ap;
    ap.V = get_mesh_vertices(md);
    ap.F = get_mesh_facets(md);
    ap.root.init(ap.V, ap.F);

    r.emplace_or_replace<AcceleratedPicking>(e, std::move(ap));
    return true;
}

bool has_accelerated_picking(Registry& r, Entity e)
{
    return r.all_of<AcceleratedPicking>(get_mesh_entity(r, e));
}

std::optional<lagrange::ui::RayFacetHit>
intersect_ray(Registry& r, Entity in_e, const Eigen::Vector3f& origin, const Eigen::Vector3f& dir)
{
    auto e = get_mesh_entity(r, in_e);
    if (!r.valid(e)) return {};

    if (!has_accelerated_picking(r, e)) {
        const auto& md = r.get<MeshData>(e);
        return intersect_ray(md, origin, dir);
    }

    auto& ap = r.get<AcceleratedPicking>(e);

    igl::Hit hit;
    bool res =
        ap.root.intersect_ray(ap.V, ap.F, origin, dir, std::numeric_limits<float>::infinity(), hit);

    if (!res) return {};

    RayFacetHit rfhit;

    rfhit.facet_id = hit.id;
    rfhit.barycentric = Eigen::Vector3f(1.0f - hit.u - hit.v, hit.u, hit.v);
    rfhit.t = hit.t;

    return rfhit;
}

} // namespace ui
} // namespace lagrange
