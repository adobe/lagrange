/*
 * Copyright 2024 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/thicken_and_close_mesh.h>

#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/compute_vertex_normal.h>
#include <lagrange/filter_attributes.h>
#include <lagrange/find_matching_attributes.h>
#include <lagrange/internal/visit_attribute.h>
#include <lagrange/utils/invalid.h>
#include <lagrange/utils/safe_cast.h>
#include <lagrange/utils/warning.h>
#include <lagrange/views.h>

#include <spdlog/fmt/ranges.h>

namespace lagrange {

namespace {

template <class... Ts>
struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

// Compute offset vertex
template <typename Scalar>
Eigen::Vector3<Scalar> compute_vertex(
    const Eigen::Vector3<Scalar>& vertex,
    const Eigen::Vector3<Scalar>& offset_vector,
    Scalar amount)
{
    Eigen::Vector3<Scalar> offset_vertex = vertex + amount * offset_vector;
    return offset_vertex;
};

// Compute offset vertex with plane mirroring
template <typename Scalar>
Eigen::Vector3<Scalar> compute_vertex(
    const Eigen::Vector3<Scalar>& vertex,
    const Eigen::Vector3<Scalar>& offset_vector,
    const Eigen::Vector3<Scalar>& mirror_vector,
    const Eigen::Vector3<Scalar>& offset_direction,
    Scalar amount)
{
    Eigen::Vector3<Scalar> offset_vertex = vertex + amount * offset_vector;
    offset_vertex -= (offset_vertex.dot(offset_direction) * amount) * mirror_vector;
    return offset_vertex;
};

// Offset wrt to a fixed direction, with plane mirroring
template <typename Scalar, typename Index>
void offset_vertices_fixed(
    Attribute<Scalar>& vertices_,
    Index num_input_vertices,
    Index num_segments,
    const std::vector<Index>& boundary_vertices,
    const Eigen::Vector3<Scalar>& offset_direction,
    Scalar offset_amount,
    Scalar mirror_ratio)
{
    using Vector3s = Eigen::Matrix<Scalar, 3, 1>;

    // Prepare ancillary data
    Vector3s offset_vector = offset_amount * offset_direction;
    Vector3s mirror_vector = (Scalar(1) - mirror_ratio) * offset_direction;

    // Offset vertices
    auto offset_vertices = matrix_ref(vertices_);
    for (Index v = 0; v < num_input_vertices; ++v) {
        const Vector3s vertex = offset_vertices.row(v).template head<3>();
        LA_IGNORE_ARRAY_BOUNDS_BEGIN
        offset_vertices.row(num_input_vertices + v) =
            compute_vertex(vertex, offset_vector, mirror_vector, offset_direction, Scalar(1.0));
        LA_IGNORE_ARRAY_BOUNDS_END
    }

    if (num_segments > 1) {
        // Add intermediate vertices to stitch boundary vertices
        const Scalar segment_increment = Scalar(1) / static_cast<Scalar>(num_segments);
        for (Index vi = 0; vi < boundary_vertices.size(); ++vi) {
            Index v = boundary_vertices[vi];
            const Vector3s vertex = offset_vertices.row(v).template head<3>();

            for (Index is = 1; is < num_segments; ++is) {
                Scalar offset_ratio = static_cast<Scalar>(is) * segment_increment;
                la_debug_assert(offset_ratio < 1.0);
                LA_IGNORE_ARRAY_BOUNDS_BEGIN
                offset_vertices.row(
                    num_input_vertices * 2 + // original + offset vertices
                    vi * (num_segments - 1) + // segment row
                    is - 1 // index in the boundary
                    ) =
                    compute_vertex(
                        vertex,
                        offset_vector,
                        mirror_vector,
                        offset_direction,
                        offset_ratio);
                LA_IGNORE_ARRAY_BOUNDS_END
            }
        }
    }
}

// Offset wrt to a variable normal direction
template <typename Scalar, typename Index>
void offset_vertices_normals(
    Attribute<Scalar>& vertices_,
    Index num_input_vertices,
    Index num_segments,
    const std::vector<Index>& boundary_vertices,
    const Attribute<Scalar>& normals_,
    Scalar offset_amount)
{
    using Vector3s = Eigen::Matrix<Scalar, 3, 1>;

    auto normals = matrix_view(normals_);

    // Offset vertices
    auto offset_vertices = matrix_ref(vertices_);
    for (Index v = 0; v < num_input_vertices; ++v) {
        const Vector3s vertex = offset_vertices.row(v).template head<3>();
        const Vector3s offset_vector = -normals.row(v).template head<3>() * offset_amount;
        LA_IGNORE_ARRAY_BOUNDS_BEGIN
        offset_vertices.row(num_input_vertices + v) =
            compute_vertex(vertex, offset_vector, Scalar(1.0));
        LA_IGNORE_ARRAY_BOUNDS_END
    }

    if (num_segments > 1) {
        // Add intermediate vertices to stitch boundary vertices
        const Scalar segment_increment = Scalar(1) / static_cast<Scalar>(num_segments);
        for (Index vi = 0; vi < boundary_vertices.size(); ++vi) {
            Index v = boundary_vertices[vi];
            const Vector3s vertex = offset_vertices.row(v).template head<3>();
            const Vector3s offset_vector = -normals.row(v).template head<3>() * offset_amount;

            for (Index is = 1; is < num_segments; ++is) {
                Scalar offset_ratio = static_cast<Scalar>(is) * segment_increment;
                la_debug_assert(offset_ratio < 1.0);
                LA_IGNORE_ARRAY_BOUNDS_BEGIN
                offset_vertices.row(
                    num_input_vertices * 2 + // original + offset vertices
                    vi * (num_segments - 1) + // segment row
                    is - 1 // index in the boundary
                    ) = compute_vertex(vertex, offset_vector, offset_ratio);
                LA_IGNORE_ARRAY_BOUNDS_END
            }
        }
    }
}

template <typename Index, typename Func1, typename Func2>
void offset_indices(
    Attribute<Index>& indices_,
    Index num_input_vertices,
    Index num_input_facets,
    Index num_input_corners,
    Index num_segments,
    const std::vector<Index>& boundary_edges_first_corner,
    const std::vector<Index>& vertex_to_boundary_vertex,
    Func1 facet_size,
    Func2 next_corner_around_facet)
{
    // 1. Write a flipped copy of each original facet.
    auto indices = indices_.ref_all();
    Index corner_index = 0;
    for (Index f = 0; f < num_input_facets; ++f) {
        const Index nv = facet_size(f);
        auto src = indices.subspan(corner_index, nv);
        auto dst = indices.subspan(corner_index + num_input_corners, nv);
        // Flip order of vertices in the duplicate facet
        std::transform(src.rbegin(), src.rend(), dst.begin(), [&](Index v) {
            return v + num_input_vertices;
        });
        corner_index += nv;
    }
    la_debug_assert(corner_index == num_input_corners);

    // 2. Stitch boundary edges with new facets
    Index vbstart = num_input_vertices * 2;
    corner_index = num_input_corners * 2;
    for (Index c0 : boundary_edges_first_corner) {
        Index v0 = indices[c0];
        Index v1 = indices[next_corner_around_facet(c0)];
        Index bv0 = vertex_to_boundary_vertex[v0];
        Index bv1 = vertex_to_boundary_vertex[v1];
        auto section = indices.subspan(corner_index, num_segments * 6);
        for (Index is = 0, ci = 0; is < num_segments; ++is) {
            bool is_first_segment = (is == 0);
            bool is_last_segment = (is == num_segments - 1);
            Index u0 = is_first_segment ? v0 : vbstart + bv0 * (num_segments - 1) + is - 1;
            Index u1 = is_first_segment ? v1 : vbstart + bv1 * (num_segments - 1) + is - 1;
            Index u2 =
                is_last_segment ? v0 + num_input_vertices : vbstart + bv0 * (num_segments - 1) + is;
            Index u3 =
                is_last_segment ? v1 + num_input_vertices : vbstart + bv1 * (num_segments - 1) + is;
            for (auto u : {u0, u2, u1, u1, u2, u3}) {
                section[ci++] = u;
            }
        }
        corner_index += num_segments * 6;
    }
    [[maybe_unused]] Index nbe = static_cast<Index>(boundary_edges_first_corner.size());
    la_debug_assert(corner_index == num_input_corners * 2 + nbe * num_segments * 6);
    la_debug_assert(corner_index == static_cast<Index>(indices.size()));
}

template <typename ValueType, typename Index, typename Func>
void offset_values(
    Attribute<ValueType>& values_,
    const Attribute<Index>& indices_,
    Index num_segments,
    const std::vector<Index>& boundary_edges_first_corner,
    std::vector<Index>& vertex_to_boundary_vertex,
    Func next_corner_around_facet)
{
    const Index num_input_values = static_cast<Index>(values_.get_num_elements());

    {
        // Duplicate input values twice
        values_.insert_elements(num_input_values);
        auto values = matrix_ref(values_);
        values.middleRows(num_input_values, num_input_values) = values.topRows(num_input_values);
    }

    if (num_segments > 1) {
        // Add intermediate vertices to stitch boundary values
        vertex_to_boundary_vertex.assign(num_input_values, invalid<Index>());
        auto indices = vector_view(indices_);
        const Index nc = static_cast<Index>(values_.get_num_channels());
        std::vector<ValueType> tmp_values((num_segments - 1) * nc);
        Index num_boundary_values = 0;
        for (Index c0 : boundary_edges_first_corner) {
            Index c1 = next_corner_around_facet(c0);
            for (Index ci : {c0, c1}) {
                Index vi = indices[ci];
                // Assign a new boundary vertex index
                if (vertex_to_boundary_vertex[vi] != invalid<Index>()) {
                    continue;
                }
                vertex_to_boundary_vertex[vi] = num_boundary_values++;
                auto src = values_.get_row(vi);
                for (Index is = 0; is + 1 < num_segments; ++is) {
                    std::copy(src.begin(), src.end(), tmp_values.data() + is * nc);
                }
                logger().debug("New values: {}", fmt::join(tmp_values, ", "));
                values_.insert_elements(tmp_values);
            }
        }
    }
}

} // namespace

template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> thicken_and_close_mesh(
    SurfaceMesh<Scalar, Index> mesh,
    const ThickenAndCloseOptions& options)
{
    using Vector3s = Eigen::Matrix<Scalar, 3, 1>;

    la_runtime_assert(mesh.get_dimension() == 3, "This function only supports 3D meshes.");

    // Input parameters
    const Index num_segments = std::max<Index>(1, safe_cast<Index>(options.num_segments));
    const Index num_input_vertices = mesh.get_num_vertices();
    const Index num_input_facets = mesh.get_num_facets();
    const Index num_input_corners = mesh.get_num_corners();

    // Count boundary edges and vertices in the input mesh
    std::vector<Index> vertex_to_boundary_vertex(num_input_vertices, invalid<Index>());
    std::vector<Index> boundary_edges_first_corner;
    std::vector<Index> boundary_vertices;
    {
        mesh.initialize_edges();
        for (Index e = 0; e < mesh.get_num_edges(); ++e) {
            if (mesh.is_boundary_edge(e)) {
                boundary_edges_first_corner.push_back(mesh.get_one_corner_around_edge(e));
                for (auto v : mesh.get_edge_vertices(e)) {
                    if (vertex_to_boundary_vertex[v] == invalid<Index>()) {
                        vertex_to_boundary_vertex[v] = static_cast<Index>(boundary_vertices.size());
                        boundary_vertices.push_back(v);
                    }
                }
            }
        }
        mesh.clear_edges();
    }
    const Index num_boundary_edges = static_cast<Index>(boundary_edges_first_corner.size());
    const Index num_boundary_vertices = static_cast<Index>(boundary_vertices.size());

    // Output vertices are packed as follows:
    // 1. original vertices (num_input_vertices)
    // 2. offset vertices (num_input_vertices)
    // 3. stitch vertices ((num_segments - 1) * num_boundary_vertices), packed by segment

    // First, maybe compute per-vertex normals
    AttributeId normals_id = invalid<AttributeId>();
    std::visit(
        overloaded{
            [&](std::string_view name) {
                if (options.mirror_ratio.has_value()) {
                    throw Error(
                        "Cannot use 'mirror_ratio' with varying offset direction. Use a "
                        "fixed offset direction instead.");
                }

                if (name.empty()) {
                    AttributeMatcher matcher;
                    matcher.usages = AttributeUsage::Normal;
                    matcher.element_types = AttributeElement::Vertex;
                    matcher.num_channels = 3;
                    auto id = find_matching_attribute(mesh, matcher);
                    if (!id.has_value()) {
                        logger().info(
                            "Could not find input vertex normals for offsetting. Recomputing.");
                        normals_id = compute_vertex_normal(mesh);
                    } else {
                        normals_id = id.value();
                    }
                } else {
                    normals_id = mesh.get_attribute_id(name);
                }
            },
            [&](std::array<double, 3>) {},
        },
        options.direction);

    // Allocate new vertices
    mesh.add_vertices(num_input_vertices + (num_segments - 1) * num_boundary_vertices);

    // Compute offset positions
    std::visit(
        overloaded{
            [&](std::string_view) {
                offset_vertices_normals(
                    mesh.ref_vertex_to_position(),
                    num_input_vertices,
                    num_segments,
                    boundary_vertices,
                    mesh.template get_attribute<Scalar>(normals_id),
                    static_cast<Scalar>(options.offset_amount));
            },
            [&](std::array<double, 3> dir) {
                Eigen::Vector3d direction_d(dir[0], dir[1], dir[2]);
                Vector3s direction_s = direction_d.stableNormalized().template cast<Scalar>();
                offset_vertices_fixed(
                    mesh.ref_vertex_to_position(),
                    num_input_vertices,
                    num_segments,
                    boundary_vertices,
                    direction_s,
                    static_cast<Scalar>(options.offset_amount),
                    static_cast<Scalar>(options.mirror_ratio.value_or(1.0)));
            },
        },
        options.direction);

    // Clear any vertex/corner/edge/face attribute
    {
        AttributeFilter value_filter;
        value_filter.included_element_types = AttributeElement::Value;

        AttributeFilter indexed_filter;
        indexed_filter.included_attributes.emplace();
        for (auto& name : options.indexed_attributes) {
            indexed_filter.included_attributes.value().push_back(name);
        }
        indexed_filter.included_element_types = AttributeElement::Indexed;

        AttributeFilter filter;
        filter.or_filters = {value_filter, indexed_filter};

        mesh = filter_attributes(std::move(mesh), filter);
    }

    // Compute offset facet connectivity
    mesh.add_hybrid(
        num_input_facets + num_boundary_edges * num_segments * 2,
        [&](Index f) {
            if (f < num_input_facets) {
                return mesh.get_facet_size(f);
            } else {
                return Index(3);
            }
        },
        [](Index, span<Index>) {});
    offset_indices(
        mesh.ref_corner_to_vertex(),
        num_input_vertices,
        num_input_facets,
        num_input_corners,
        num_segments,
        boundary_edges_first_corner,
        vertex_to_boundary_vertex,
        [&](Index f) { return mesh.get_facet_size(f); },
        [&](Index c) { return mesh.get_next_corner_around_facet(c); });

    // Extend UV values to offset vertices
    for (auto name : options.indexed_attributes) {
        internal::visit_attribute_write(mesh, mesh.get_attribute_id(name), [&](auto&& attr) {
            using AttributeType = std::decay_t<decltype(attr)>;
            if constexpr (AttributeType::IsIndexed) {
                Index num_input_values = static_cast<Index>(attr.values().get_num_elements());
                offset_values(
                    attr.values(),
                    attr.indices(),
                    num_segments,
                    boundary_edges_first_corner,
                    vertex_to_boundary_vertex,
                    [&](Index c) { return mesh.get_next_corner_around_facet(c); });
                offset_indices(
                    attr.indices(),
                    num_input_values,
                    num_input_facets,
                    num_input_corners,
                    num_segments,
                    boundary_edges_first_corner,
                    vertex_to_boundary_vertex,
                    [&](Index f) { return mesh.get_facet_size(f); },
                    [&](Index c) { return mesh.get_next_corner_around_facet(c); });
            } else {
                throw Error(fmt::format("Attribute '{}' is not indexed.", name));
            }
        });
    }

    return mesh;
}

#define LA_X_thicken_and_close_mesh(_, Scalar, Index)                       \
    template LA_CORE_API SurfaceMesh<Scalar, Index> thicken_and_close_mesh( \
        SurfaceMesh<Scalar, Index> mesh,                                    \
        const ThickenAndCloseOptions& options);
LA_SURFACE_MESH_X(thicken_and_close_mesh, 0)

} // namespace lagrange
