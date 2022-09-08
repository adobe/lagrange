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
#include <lagrange/ui/components/MeshData.h>
#include <lagrange/ui/components/MeshGeometry.h>
#include <lagrange/ui/types/AABB.h>
#include <lagrange/ui/types/Frustum.h>
#include <lagrange/ui/types/RayFacetHit.h>
#include <lagrange/ui/utils/math.h>
#include <optional>

/// This header exposes functionality of previously registered mesh types
/// To register new mesh type, include <lagrange/ui/utils/mesh.impl.h>
/// and call register_mesh_type<MeshType>

namespace lagrange {
namespace ui {

struct GPUBuffer;
class Camera;

template <typename MeshType>
Entity register_mesh(Registry& r, std::shared_ptr<MeshType> mesh)
{
    auto e = r.create();
    MeshData d;
    d.mesh = mesh;
    d.type = entt::type_id<MeshType>();
    r.emplace<MeshData>(e, std::move(d));
    return e;
}

template <typename MeshType>
Entity register_mesh(Registry& r, std::unique_ptr<MeshType> mesh)
{
    return register_mesh(r, std::shared_ptr<MeshType>(mesh.release()));
}


//////////////////////////////////////////////////////////////////////////////////////
// Getters
//////////////////////////////////////////////////////////////////////////////////////


template <typename V, typename F>
lagrange::Mesh<V, F>& cast_mesh(MeshData& mesh_data);
template <typename V, typename F>
lagrange::Mesh<V, F>& cast_mesh(MeshData& mesh_data);
template <typename MeshType>
MeshType& get_mesh(Registry& r, Entity e);

MeshData& get_mesh_data(Registry& r, Entity e);
const MeshData& get_mesh_data(const Registry& r, Entity e);

bool has_mesh_component(const Registry& r, Entity e);

size_t get_num_vertices(const MeshData& d);
size_t get_num_facets(const MeshData& d);
size_t get_num_edges(const MeshData& d);
RowMajorMatrixXf get_mesh_vertices(const MeshData& d);
RowMajorMatrixXi get_mesh_facets(const MeshData& d);
RowMajorMatrixXf get_mesh_vertex_attribute(const MeshData& d, const std::string& name);
RowMajorMatrixXf get_mesh_corner_attribute(const MeshData& d, const std::string& name);
RowMajorMatrixXf get_mesh_facet_attribute(const MeshData& d, const std::string& name);
RowMajorMatrixXf get_mesh_edge_attribute(const MeshData& d, const std::string& name);
RowMajorMatrixXf get_mesh_attribute(const MeshData& d, IndexingMode mode, const std::string& name);
std::pair<Eigen::VectorXf, Eigen::VectorXf>
get_mesh_attribute_range(const MeshData& d, IndexingMode mode, const std::string& name);
AABB get_mesh_bounds(const MeshData& d);


//////////////////////////////////////////////////////////////////////////////////////
// Ensure existence of mesh attributes for rendering
//////////////////////////////////////////////////////////////////////////////////////

void ensure_uv(MeshData& d);
void ensure_normal(MeshData& d);
void ensure_tangent_bitangent(MeshData& d);
void ensure_is_selected_attribute(MeshData& d);
void map_indexed_attribute_to_corner_attribute(MeshData& d, const std::string& name);
void map_corner_attribute_to_vertex_attribute(MeshData& d, const std::string& name);

//////////////////////////////////////////////////////////////////////////////////////
// Mesh to GPU upload
//////////////////////////////////////////////////////////////////////////////////////

void upload_mesh_vertices(const MeshData& d, GPUBuffer& gpu);
void upload_mesh_triangles(const MeshData& d, GPUBuffer& gpu);

void upload_mesh_vertex_attribute(const MeshData& d, const RowMajorMatrixXf& data, GPUBuffer& gpu);
void upload_mesh_corner_attribute(const MeshData& d, const RowMajorMatrixXf& data, GPUBuffer& gpu);
void upload_mesh_facet_attribute(const MeshData& d, const RowMajorMatrixXf& data, GPUBuffer& gpu);
void upload_mesh_edge_attribute(const MeshData& d, const RowMajorMatrixXf& data, GPUBuffer& gpu);

std::unordered_map<entt::id_type, std::shared_ptr<GPUBuffer>> upload_submesh_indices(
    const MeshData& d,
    const std::string& facet_attrib_name);

//////////////////////////////////////////////////////////////////////////////////////
// Has attribute
//////////////////////////////////////////////////////////////////////////////////////
bool has_mesh_vertex_attribute(const MeshData& d, const std::string& name);
bool has_mesh_corner_attribute(const MeshData& d, const std::string& name);
bool has_mesh_facet_attribute(const MeshData& d, const std::string& name);
bool has_mesh_edge_attribute(const MeshData& d, const std::string& name);
bool has_mesh_indexed_attribute(const MeshData& d, const std::string& name);

//////////////////////////////////////////////////////////////////////////////////////
// Picking
//////////////////////////////////////////////////////////////////////////////////////

/// Intersect ray with MeshData
std::optional<RayFacetHit>
intersect_ray(const MeshData& d, const Eigen::Vector3f& origin, const Eigen::Vector3f& dir);
bool select_facets_in_frustum(MeshData& d, SelectionBehavior sel_behavior, const Frustum& frustum);
void select_vertices_in_frustum(
    MeshData& d,
    SelectionBehavior sel_behavior,
    const Frustum& frustum);
void select_edges_in_frustum(MeshData& d, SelectionBehavior sel_behavior, const Frustum& frustum);
void propagate_corner_selection(MeshData& d, const std::string& attrib_name);
void propagate_vertex_selection(MeshData& d, const std::string& attrib_name);
void propagate_facet_selection(MeshData& d, const std::string& attrib_name);
void combine_vertex_and_corner_selection(MeshData& d, const std::string& attrib_name);
void select_facets_by_color(
    MeshData& d,
    const std::string& attrib_name,
    SelectionBehavior sel_behavior,
    const unsigned char* color_bytes,
    size_t colors_byte_size);
void select_edges_by_color(
    MeshData& d,
    const std::string& attrib_name,
    SelectionBehavior sel_behavior,
    const unsigned char* color_bytes,
    size_t colors_byte_size);
void select_vertices_by_color(
    MeshData& d,
    const std::string& attrib_name,
    SelectionBehavior sel_behavior,
    const unsigned char* color_bytes,
    size_t colors_byte_size);
void select_facets(
    MeshData& d,
    SelectionBehavior sel_behavior,
    const std::vector<int>& facet_indices);

void filter_closest_vertex(
    MeshData& d,
    const std::string& attrib_name,
    SelectionBehavior sel_behavior,
    const Camera& camera,
    const Eigen::Vector2i& viewport_pos);

template <typename V, typename F>
lagrange::Mesh<V, F>& cast_mesh(const Registry& r, Entity e)
{
    return cast_mesh<V, F>(r.get<const MeshData>(e));
}


template <typename V, typename F>
lagrange::Mesh<V, F>& cast_mesh(MeshData& mesh_data)
{
    using MeshType = lagrange::Mesh<V, F>;
    la_runtime_assert(
        mesh_data.type == entt::type_id<MeshType>() && mesh_data.mesh,
        "Incorrect mesh type");
    return reinterpret_cast<MeshType&>(*mesh_data.mesh);
}

template <typename MeshType>
MeshType& get_mesh(Registry& r, Entity e)
{
    la_runtime_assert(r.all_of<MeshData>(e), "No MeshData component");
    return cast_mesh<typename MeshType::VertexArray, typename MeshType::FacetArray>(
        r.get<MeshData>(e));
}

inline Entity get_mesh_entity(Registry& r, Entity e)
{
    if (r.all_of<MeshData>(e)) return e;
    if (r.all_of<MeshGeometry>(e)) return r.get<MeshGeometry>(e).entity;
    return NullEntity;
}

} // namespace ui
} // namespace lagrange
