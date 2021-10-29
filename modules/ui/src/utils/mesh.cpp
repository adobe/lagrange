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
#include <lagrange/ui/utils/mesh.h>
#include <lagrange/ui/types/Camera.h>


namespace lagrange {
namespace ui {

MeshData& get_mesh_data(Registry& r, Entity e)
{
    if (r.has<MeshData>(e)) return r.get<MeshData>(e);
    return r.get<MeshData>(r.get<MeshGeometry>(e).entity);
}

const MeshData& get_mesh_data(const Registry& r, Entity e)
{
    if (r.has<MeshData>(e)) return r.get<MeshData>(e);
    return r.get<MeshData>(r.get<MeshGeometry>(e).entity);
}

size_t get_num_vertices(const MeshData& d)
{
    return entt::resolve(d.type)
        .func(entt::hashed_string{"get_num_vertices"})
        .invoke({}, (const MeshBase*)(d.mesh.get()))
        .cast<size_t>();
}

size_t get_num_facets(const MeshData& d)
{
    return entt::resolve(d.type)
        .func(entt::hashed_string{"get_num_facets"})
        .invoke({}, (const MeshBase*)(d.mesh.get()))
        .cast<size_t>();
}

size_t get_num_edges(const MeshData& d)
{
    return entt::resolve(d.type)
        .func(entt::hashed_string{"get_num_edges"})
        .invoke({}, (const MeshBase*)(d.mesh.get()))
        .cast<size_t>();
}

RowMajorMatrixXf get_mesh_vertices(const MeshData& d)
{
    const MeshBase* base = d.mesh.get();
    auto fn = entt::resolve(d.type).func(entt::hashed_string{"get_mesh_vertices"});
    auto result = fn.invoke({}, base);
    auto conversion_result = entt::resolve<RowMajorMatrixXf>().construct(result);
    return conversion_result.cast<RowMajorMatrixXf>();
}

RowMajorMatrixXi get_mesh_facets(const MeshData& d)
{
    return entt::resolve(d.type)
        .func(entt::hashed_string{"get_mesh_facets"})
        .invoke({}, (const MeshBase*)(d.mesh.get()))
        .cast<RowMajorMatrixXi>();
}

RowMajorMatrixXf get_mesh_vertex_attribute(const MeshData& d, const std::string& name)
{
    return entt::resolve(d.type)
        .func(entt::hashed_string{"get_mesh_vertex_attribute"})
        .invoke({}, (const MeshBase*)(d.mesh.get()), name)
        .cast<RowMajorMatrixXf>();
}

RowMajorMatrixXf get_mesh_corner_attribute(const MeshData& d, const std::string& name)
{
    return entt::resolve(d.type)
        .func(entt::hashed_string{"get_mesh_corner_attribute"})
        .invoke({}, (const MeshBase*)(d.mesh.get()), name)
        .cast<RowMajorMatrixXf>();
}

RowMajorMatrixXf get_mesh_facet_attribute(const MeshData& d, const std::string& name)
{
    return entt::resolve(d.type)
        .func(entt::hashed_string{"get_mesh_facet_attribute"})
        .invoke({}, (const MeshBase*)(d.mesh.get()), name)
        .cast<RowMajorMatrixXf>();
}

RowMajorMatrixXf get_mesh_edge_attribute(const MeshData& d, const std::string& name)
{
    return entt::resolve(d.type)
        .func(entt::hashed_string{"get_mesh_edge_attribute"})
        .invoke({}, (const MeshBase*)(d.mesh.get()), name)
        .cast<RowMajorMatrixXf>();
}

RowMajorMatrixXf get_mesh_attribute(const MeshData& d, IndexingMode mode, const std::string& name)
{
    return entt::resolve(d.type)
        .func(entt::hashed_string{"get_mesh_attribute"})
        .invoke({}, (const MeshBase*)(d.mesh.get()), mode, name)
        .cast<RowMajorMatrixXf>();
}
std::pair<Eigen::VectorXf, Eigen::VectorXf>
get_mesh_attribute_range(const MeshData& d, IndexingMode mode, const std::string& name)
{
    return entt::resolve(d.type)
        .func(entt::hashed_string{"get_mesh_attribute_range"})
        .invoke({}, (const MeshBase*)(d.mesh.get()), mode, name)
        .cast<std::pair<Eigen::VectorXf, Eigen::VectorXf>>();
}

ui::AABB get_mesh_bounds(const MeshData& d) {
    return entt::resolve(d.type)
        .func(entt::hashed_string{"get_mesh_bounds"})
        .invoke({}, (const MeshBase*)(d.mesh.get()))
        .cast<ui::AABB>();
}

void ensure_uv(MeshData& d)
{
    entt::resolve(d.type)
        .func(entt::hashed_string{"ensure_uv"})
        .invoke({}, (MeshBase*)(d.mesh.get()));
}

void ensure_normal(MeshData& d)
{
    entt::resolve(d.type)
        .func(entt::hashed_string{"ensure_normal"})
        .invoke({}, (MeshBase*)(d.mesh.get()));
}

void ensure_tangent_bitangent(MeshData& d)
{
    entt::resolve(d.type)
        .func(entt::hashed_string{"ensure_tangent_bitangent"})
        .invoke({}, (MeshBase*)(d.mesh.get()));
}

void ensure_is_selected_attribute(MeshData& d) {
    entt::resolve(d.type)
        .func(entt::hashed_string{"ensure_is_selected_attribute"})
        .invoke({}, (MeshBase*)(d.mesh.get()));
}

void map_indexed_attribute_to_corner_attribute(MeshData& d, const std::string& name) {
    entt::resolve(d.type)
        .func(entt::hashed_string{"map_indexed_attribute_to_corner_attribute"})
        .invoke({}, (MeshBase*)(d.mesh.get()), name);
}

void map_corner_attribute_to_vertex_attribute(MeshData& d, const std::string& name) {
    entt::resolve(d.type)
        .func(entt::hashed_string{"map_corner_attribute_to_vertex_attribute"})
        .invoke({}, (MeshBase*)(d.mesh.get()), name);
}

bool has_mesh_vertex_attribute(const MeshData& d, const std::string& name)
{
    return entt::resolve(d.type)
        .func(entt::hashed_string{"has_mesh_vertex_attribute"})
        .invoke({}, (const MeshBase*)(d.mesh.get()), name)
        .cast<bool>();
}

bool has_mesh_corner_attribute(const MeshData& d, const std::string& name)
{
    return entt::resolve(d.type)
        .func(entt::hashed_string{"has_mesh_corner_attribute"})
        .invoke({}, (const MeshBase*)(d.mesh.get()), name)
        .cast<bool>();
}

bool has_mesh_facet_attribute(const MeshData& d, const std::string& name)
{
    return entt::resolve(d.type)
        .func(entt::hashed_string{"has_mesh_facet_attribute"})
        .invoke({}, (const MeshBase*)(d.mesh.get()), name)
        .cast<bool>();
}

bool has_mesh_edge_attribute(const MeshData& d, const std::string& name)
{
    return entt::resolve(d.type)
        .func(entt::hashed_string{"has_mesh_edge_attribute"})
        .invoke({}, (const MeshBase*)(d.mesh.get()), name)
        .cast<bool>();
}


bool has_mesh_indexed_attribute(const MeshData& d, const std::string& name) {
    return entt::resolve(d.type)
        .func(entt::hashed_string{"has_mesh_indexed_attribute"})
        .invoke({}, (const MeshBase*)(d.mesh.get()), name)
        .cast<bool>();
}

void upload_mesh_vertices(const MeshData& d, GPUBuffer& gpu)
{
    entt::resolve(d.type)
        .func(entt::hashed_string{"upload_mesh_vertices"})
        .invoke({}, (const MeshBase*)(d.mesh.get()), &gpu);
}

void upload_mesh_triangles(const MeshData& d, GPUBuffer& gpu)
{
    entt::resolve(d.type)
        .func(entt::hashed_string{"upload_mesh_triangles"})
        .invoke({}, (const MeshBase*)(d.mesh.get()), &gpu);
}

void upload_mesh_vertex_attribute(const MeshData& d, const RowMajorMatrixXf& data, GPUBuffer& gpu)
{
    entt::resolve(d.type)
        .func(entt::hashed_string{"upload_mesh_vertex_attribute"})
        .invoke({}, (const MeshBase*)(d.mesh.get()), &data, &gpu);
}

void upload_mesh_corner_attribute(const MeshData& d, const RowMajorMatrixXf& data, GPUBuffer& gpu)
{
    entt::resolve(d.type)
        .func(entt::hashed_string{"upload_mesh_corner_attribute"})
        .invoke({}, (const MeshBase*)(d.mesh.get()), &data, &gpu);
}

void upload_mesh_facet_attribute(const MeshData& d, const RowMajorMatrixXf& data, GPUBuffer& gpu)
{
    entt::resolve(d.type)
        .func(entt::hashed_string{"upload_mesh_facet_attribute"})
        .invoke({}, (const MeshBase*)(d.mesh.get()), &data, &gpu);
}

void upload_mesh_edge_attribute(const MeshData& d, const RowMajorMatrixXf& data, GPUBuffer& gpu)
{
    entt::resolve(d.type)
        .func(entt::hashed_string{"upload_mesh_edge_attribute"})
        .invoke({}, (const MeshBase*)(d.mesh.get()), &data, &gpu);
}


std::unordered_map<entt::id_type, std::shared_ptr<lagrange::ui::GPUBuffer>> upload_submesh_indices(
    const MeshData& d,
    const std::string& facet_attrib_name)
{
    auto res = entt::resolve(d.type)
        .func(entt::hashed_string{"upload_submesh_indices"})
        .invoke({}, (const MeshBase*)(d.mesh.get()), facet_attrib_name);

    return res.cast<std::unordered_map<entt::id_type, std::shared_ptr<GPUBuffer>>>();
}

//////////////////////////////////////////////////////////////////////////////////////
// Picking
//////////////////////////////////////////////////////////////////////////////////////

std::optional<RayFacetHit>
intersect_ray(const MeshData& d, const Eigen::Vector3f& origin, const Eigen::Vector3f& dir)
{
    RayFacetHit rfhit;

    bool result = entt::resolve(d.type)
                      .func(entt::hashed_string{"intersect_ray"})
                      .invoke({}, (const MeshBase*)(d.mesh.get()), origin, dir, &rfhit)
                      .cast<bool>();

    if (!result) return {};
    return rfhit;
}

bool select_facets_in_frustum(MeshData& d, SelectionBehavior sel_behavior, const Frustum& frustum)
{
    auto result = entt::resolve(d.type)
                      .func(entt::hashed_string{"select_facets_in_frustum"})
                      .invoke({}, (MeshBase*)(d.mesh.get()), sel_behavior, & frustum);

    if (!result) {
        LA_ASSERT(false, "select_facets_in_frustum failed");
    }
    return result.cast<bool>();
}

void select_vertices_in_frustum(MeshData& d, SelectionBehavior sel_behavior, const Frustum& frustum)
{
    if (!entt::resolve(d.type)
             .func(entt::hashed_string{"select_vertices_in_frustum"})
             .invoke({}, (MeshBase*)(d.mesh.get()), sel_behavior, & frustum)) {
        LA_ASSERT(false, "select_vertices_in_frustum failed");
    }
}

void select_edges_in_frustum(MeshData& d, SelectionBehavior sel_behavior, const Frustum& frustum)
{
    if (!entt::resolve(d.type)
             .func(entt::hashed_string{"select_edges_in_frustum"})
             .invoke({}, (MeshBase*)(d.mesh.get()), sel_behavior, & frustum)) {
        LA_ASSERT(false, "select_edges_in_frustum failed");
    }
}


void propagate_corner_selection(MeshData& d, const std::string& attrib_name)
{
    if (!entt::resolve(d.type)
             .func(entt::hashed_string{"propagate_corner_selection"})
             .invoke({}, (MeshBase*)(d.mesh.get()), attrib_name)) {
        LA_ASSERT(false, "propagate_corner_selection failed");
    }
}

void propagate_vertex_selection(MeshData& d, const std::string& attrib_name)
{
    if (!entt::resolve(d.type)
             .func(entt::hashed_string{"propagate_vertex_selection"})
             .invoke({}, (MeshBase*)(d.mesh.get()), attrib_name)) {
        LA_ASSERT(false, "propagate_vertex_selection failed");
    }
}

void propagate_facet_selection(MeshData& d, const std::string& attrib_name)
{
    if (!entt::resolve(d.type)
             .func(entt::hashed_string{"propagate_facet_selection"})
             .invoke({}, (MeshBase*)(d.mesh.get()), attrib_name)) {
        LA_ASSERT(false, "propagate_facet_selection failed");
    }
}

void combine_vertex_and_corner_selection(MeshData& d, const std::string& attrib_name)
{
    if (!entt::resolve(d.type)
             .func(entt::hashed_string{"combine_vertex_and_corner_selection"})
             .invoke({}, (MeshBase*)(d.mesh.get()), attrib_name)) {
        LA_ASSERT(false, "combine_vertex_and_corner_selection failed");
    }
}

void select_facets_by_color(
    MeshData& d,
    const std::string& attrib_name,
    SelectionBehavior sel_behavior, 
    const unsigned char* color_bytes,
    size_t colors_byte_size)
{
    if (!entt::resolve(d.type)
             .func(entt::hashed_string{"select_facets_by_color"})
             .invoke(
                 {},
                 (MeshBase*)(d.mesh.get()),
                 attrib_name,
                 sel_behavior,
                 color_bytes,
                 colors_byte_size)) {
        LA_ASSERT(false, "select_facets_by_color failed");
    }
}

void select_edges_by_color(
    MeshData& d,
    const std::string& attrib_name,
    SelectionBehavior sel_behavior, 
    const unsigned char* color_bytes,
    size_t colors_byte_size)
{
    if (!entt::resolve(d.type)
             .func(entt::hashed_string{"select_edges_by_color"})
             .invoke(
                 {},
                 (MeshBase*)(d.mesh.get()),
                 attrib_name,
                 sel_behavior,
                 color_bytes,
                 colors_byte_size)) {
        LA_ASSERT(false, "select_edges_by_color failed");
    }
}

void select_vertices_by_color(
    MeshData& d,
    const std::string& attrib_name,
    SelectionBehavior sel_behavior, 
    const unsigned char* color_bytes,
    size_t colors_byte_size)
{
    if (!entt::resolve(d.type)
             .func(entt::hashed_string{"select_vertices_by_color"})
             .invoke(
                 {},
                 (MeshBase*)(d.mesh.get()),
                 attrib_name,
                 sel_behavior,
                 color_bytes,
                 colors_byte_size)) {
        LA_ASSERT(false, "select_vertices_by_color failed");
    }
}


void select_facets(MeshData& d, SelectionBehavior sel_behavior, const std::vector<int> & facet_indices) {
    if (!entt::resolve(d.type)
             .func(entt::hashed_string{"select_facets"})
             .invoke(
                 {},
                 (MeshBase*)(d.mesh.get()),
                 sel_behavior,
                 &facet_indices)) {
        LA_ASSERT(false, "select_facets failed");
    }
}

void filter_closest_vertex(
    MeshData& d,
    const std::string& attrib_name,
    SelectionBehavior sel_behavior,
    const Camera& camera,
    const Eigen::Vector2i& viewport_pos)
{
    if (!entt::resolve(d.type)
             .func(entt::hashed_string{"filter_closest_vertex"})
             .invoke({}, (MeshBase*)(d.mesh.get()), attrib_name, sel_behavior, camera, viewport_pos)) {
        LA_ASSERT(false, "filter_closest_vertex failed");
    }
}

} // namespace ui
} // namespace lagrange