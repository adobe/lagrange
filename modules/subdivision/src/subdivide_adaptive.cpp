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
#include <lagrange/subdivision/api.h>
#include <lagrange/subdivision/mesh_subdivision.h>

#include "MeshConverter.h"

#include <lagrange/IndexedAttribute.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/foreach_attribute.h>
#include <lagrange/internal/attribute_string_utils.h>
#include <lagrange/internal/find_attribute_utils.h>
#include <lagrange/internal/visit_attribute.h>
#include <lagrange/utils/Error.h>
#include <lagrange/utils/SmallVector.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <opensubdiv/bfr/refinerSurfaceFactory.h>
#include <opensubdiv/bfr/surface.h>
#include <opensubdiv/bfr/tessellation.h>
#include <opensubdiv/far/topologyRefiner.h>
#include <opensubdiv/far/topologyRefinerFactory.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <variant>

//------------------------------------------------------------------------------

namespace lagrange::subdivision {

namespace {

//------------------------------------------------------------------------------

struct SharedVertex
{
    bool is_set() const { return point_index >= 0; }

    void set(int index, int lv)
    {
        point_index = index;
        local_vertex = lv;
    }

    int point_index = -1;
    int local_vertex = -1;
};

struct SharedEdge
{
    bool is_set() const { return point_index >= 0; }

    void set(int index, int n, int vtx, int lv)
    {
        point_index = index;
        num_points = n;
        first_vertex = vtx;
        local_vertex = lv;
    }

    int point_index = -1;
    int num_points = 0;
    int first_vertex = -1;
    int local_vertex = -1;
};

struct SharedEdges
{
    static constexpr size_t AvgEdgesPerVertex = 6;
    using VectorEdge = std::pair<int, SharedEdge>;

    std::vector<SmallVector<VectorEdge, AvgEdgesPerVertex>> edges;

    void set_num_vertices(int num_vertices) { edges.resize(num_vertices); }

    // Get a shared edge for the given pair of vertices. Edges are created on the fly if they do not
    // exist yet.
    SharedEdge& find_or_emplace_edge(int v0, int v1)
    {
        if (v0 > v1) std::swap(v0, v1);
        for (auto& e : edges[v0]) {
            if (e.first == v1) return e.second;
        }
        if (edges[v0].size() == AvgEdgesPerVertex) {
            logger().debug(
                "Vertex v{} has too many edges, allocating on heap for edge (v{}, v{})",
                v0,
                v0,
                v1);
        }
        edges[v0].emplace_back(v1, SharedEdge());
        return edges[v0].back().second;
    }

    const SharedEdge& find_edge(int v0, int v1) const
    {
        if (v0 > v1) std::swap(v0, v1);
        for (const auto& e : edges[v0]) {
            if (e.first == v1) return e.second;
        }
        throw std::runtime_error("Edge not found");
    }
};

template <typename Scalar>
Scalar edge_length(span<const Scalar> v0, span<const Scalar> v1)
{
    Scalar s = 0;
    for (size_t i = 0; i < v0.size(); ++i) {
        Scalar d = v0[i] - v1[i];
        s += d * d;
    }
    return std::sqrt(s);
}

template <typename Scalar>
std::tuple<float, float, float> find_min_max_avg_edges(
    const OpenSubdiv::Far::TopologyRefiner& mesh,
    span<const Scalar> vert_pos,
    int point_size)
{
    Scalar min_length = std::numeric_limits<Scalar>::max();
    Scalar max_length = 0;
    Scalar avg_length = 0;

    int num_edges = mesh.GetLevel(0).GetNumEdges();
    for (int i = 0; i < num_edges; ++i) {
        OpenSubdiv::Far::ConstIndexArray edge_verts = mesh.GetLevel(0).GetEdgeVertices(i);

        Scalar len = edge_length(
            vert_pos.subspan(edge_verts[0] * point_size, point_size),
            vert_pos.subspan(edge_verts[1] * point_size, point_size));

        max_length = std::max(max_length, len);
        min_length = std::min(min_length, len);
        avg_length += len;
    }
    avg_length /= static_cast<Scalar>(num_edges);

    return {
        static_cast<float>(min_length),
        static_cast<float>(max_length),
        static_cast<float>(avg_length)};
}

template <typename Scalar>
void get_edge_tess_rates(
    span<const Scalar> vert_pos,
    int point_size,
    Scalar tess_interval,
    int tess_rate_max,
    int* edge_rates)
{
    int num_edges = static_cast<int>(vert_pos.size()) / point_size;
    for (int i = 0; i < num_edges; ++i) {
        int j = (i + 1) % num_edges;

        Scalar len = edge_length(
            vert_pos.subspan(i * point_size, point_size),
            vert_pos.subspan(j * point_size, point_size));

        edge_rates[i] = 1 + static_cast<int>(len / tess_interval);
        edge_rates[i] = std::min(edge_rates[i], tess_rate_max);
    }
}

template <typename Index>
int eval_patch_indices(
    OpenSubdiv::Bfr::Tessellation& tess_pattern, // <- not const due to OpenSubdiv API issue
    const std::vector<int>& facet_tess_rates,
    span<const Index> patch_indices_in,
    std::vector<int>& patch_indices_out,
    std::vector<int>& tess_boundary_indices,
    std::vector<SharedVertex>& shared_verts,
    SharedEdges& shared_edges,
    int num_mesh_points_evaluated,
    bool preserve_shared_indices)
{
    //
    //  Evaluate the sample points of the Tessellation:
    //
    //  First traverse the boundary of the face to determine whether
    //  to evaluate or share points on vertices and edges of the face.
    //  Both pre-existing and new boundary points are identified by
    //  index in an array for later use. The interior points are all
    //  trivially computed after the boundary is dealt with.
    //
    //  Identify the boundary and interior coords and initialize the
    //  index array for the potentially shared boundary points:
    //
    int num_patch_coords = tess_pattern.GetNumCoords();
    int num_boundary_coords = tess_pattern.GetNumBoundaryCoords();
    int num_interior_coords = num_patch_coords - num_boundary_coords;

    tess_boundary_indices.resize(num_boundary_coords);

    //
    //  Walk around the face, inspecting each vertex and outgoing edge,
    //  and populating the index array of boundary points:
    //
    int boundary_index = 0;
    int num_face_points_evaluated = 0;
    for (int i = 0; i < static_cast<int>(patch_indices_in.size()); ++i) {
        int vert_index = static_cast<int>(patch_indices_in[i]);
        int vert_next = static_cast<int>(patch_indices_in[(i + 1) % patch_indices_in.size()]);
        int edge_rate = facet_tess_rates[i];

        //
        //  Evaluate/assign or retrieve the shared point for the vertex:
        //
        SharedVertex& shared_vertex = shared_verts[vert_index];
        if (!shared_vertex.is_set()) {
            //  Identify indices of the new shared point in the
            //  mesh and increment their inventory:
            int index_in_mesh = num_mesh_points_evaluated++;
            num_face_points_evaluated++;
            if (preserve_shared_indices) {
                shared_vertex.set(index_in_mesh, i);
            }
            tess_boundary_indices[boundary_index++] = index_in_mesh;
        } else {
            //  Assign shared vertex point index to boundary:
            tess_boundary_indices[boundary_index++] = shared_vertex.point_index;
        }

        //
        //  Evaluate/assign or retrieve all shared points for the edge:
        //
        //  To keep this simple, assume the edge is manifold. So the
        //  second face sharing the edge has that edge in the opposite
        //  direction in its boundary relative to the first face --
        //  making it necessary to reverse the order of shared points
        //  for the boundary of the second face.
        //
        //  To support a non-manifold edge, all subsequent faces that
        //  share the assigned shared edge must determine if their
        //  orientation of that edge is reversed relative to the first
        //  face for which the shared edge points were evaluated. So a
        //  little more book-keeping and/or inspection is required.
        //
        if (edge_rate > 1) {
            int points_per_edge = edge_rate - 1;

            SharedEdge& shared_edge = shared_edges.find_or_emplace_edge(vert_index, vert_next);
            if (!shared_edge.is_set()) {
                //  Identify indices of the new shared points in both the
                //  mesh and face and increment their inventory:
                int next_in_mesh = num_mesh_points_evaluated;
                num_face_points_evaluated += points_per_edge;
                num_mesh_points_evaluated += points_per_edge;

                if (preserve_shared_indices) {
                    shared_edge.set(next_in_mesh, points_per_edge, vert_index, i);
                }

                //  Evaluate shared points and assign indices to boundary:
                for (int j = 0; j < points_per_edge; ++j) {
                    tess_boundary_indices[boundary_index++] = next_in_mesh++;
                }
            } else {
                if (shared_edge.first_vertex == vert_index) {
                    //  Assign shared points to boundary in forward order:
                    int next_in_mesh = shared_edge.point_index;
                    for (int j = 0; j < points_per_edge; ++j) {
                        tess_boundary_indices[boundary_index++] = next_in_mesh++;
                    }
                } else {
                    //  Assign shared points to boundary in reverse order:
                    int next_in_mesh = shared_edge.point_index + points_per_edge - 1;
                    for (int j = 0; j < points_per_edge; ++j) {
                        tess_boundary_indices[boundary_index++] = next_in_mesh--;
                    }
                }
            }
        }
    }

    //
    //  Evaluate any interior points unique to this face -- appending
    //  them to those shared points computed above for the boundary:
    //
    if (num_interior_coords) {
        num_face_points_evaluated += num_interior_coords;
        num_mesh_points_evaluated += num_interior_coords;
    }

    //
    //  Identify the faces of the Tessellation:
    //
    //  Note that the coordinate indices used by the facets are local
    //  to the face (i.e. they range from [0..N-1], where N is the
    //  number of coordinates in the pattern) and so need to be offset
    //  when writing to Obj format.
    //
    //  For more advanced use, the coordinates associated with the
    //  boundary and interior of the pattern are distinguishable so
    //  that those on the boundary can be easily remapped to refer to
    //  shared edge or corner points, while those in the interior can
    //  be separately offset or similarly remapped.
    //
    //  So transform the indices of the facets here as needed using
    //  the indices of shared boundary points assembled above and a
    //  suitable offset for the new interior points added:
    //
    int tess_interior_offset = num_mesh_points_evaluated - num_patch_coords;

    int num_facets = tess_pattern.GetNumFacets();
    patch_indices_out.resize(num_facets * tess_pattern.GetFacetSize());
    tess_pattern.GetFacets(patch_indices_out.data());

    tess_pattern.TransformFacetCoordIndices(
        patch_indices_out.data(),
        tess_boundary_indices.data(),
        tess_interior_offset);

    return num_face_points_evaluated;
}

template <typename ValueType, typename Index>
int eval_patch_values(
    OpenSubdiv::Bfr::Surface<ValueType>& facet_surface,
    const OpenSubdiv::Bfr::Tessellation& tess_pattern,
    int num_channels,
    span<const ValueType> attr_values_in,
    const std::vector<int>& facet_tess_rates,
    std::vector<ValueType>& patch_coords,
    std::vector<ValueType>& patch_values_in,
    span<ValueType> patch_values_out,
    span<const Index> patch_indices_in,
    const std::vector<SharedVertex>& shared_verts,
    const SharedEdges& shared_edges,
    int num_mesh_points_before,
    bool preserve_shared_indices)
{
    //
    //  Prepare the Surface patch points first as it may be evaluated
    //  to determine suitable edge-rates for Tessellation:
    //
    patch_values_in.resize(facet_surface.GetNumPatchPoints() * num_channels);
    facet_surface.PreparePatchPoints(
        attr_values_in.data(),
        num_channels,
        patch_values_in.data(),
        num_channels);

    int num_patch_coords = tess_pattern.GetNumCoords();
    patch_coords.resize(num_patch_coords * 2);
    tess_pattern.GetCoords(patch_coords.data());

    //
    //  Evaluate the sample points of the Tessellation:
    //
    //  First traverse the boundary of the face to determine whether
    //  to evaluate or share points on vertices and edges of the face.
    //  Both pre-existing and new boundary points are identified by
    //  index in an array for later use. The interior points are all
    //  trivially computed after the boundary is dealt with.
    //
    //  Identify the boundary and interior coords and initialize the
    //  index array for the potentially shared boundary points:
    //
    int num_boundary_coords = tess_pattern.GetNumBoundaryCoords();
    int num_interior_coords = num_patch_coords - num_boundary_coords;

    auto patch_coords_span = span<const ValueType>(patch_coords);
    span<const ValueType> tess_boundary_uvs = patch_coords_span.subspan(0, num_boundary_coords * 2);
    span<const ValueType> tess_interior_uvs =
        patch_coords_span.subspan(num_boundary_coords * 2, num_interior_coords * 2);

    //
    //  Walk around the face, inspecting each vertex and outgoing edge,
    //  and populating the index array of boundary points:
    //
    int boundary_index = 0;
    int num_face_points_evaluated = 0;
    for (int i = 0; i < static_cast<int>(patch_indices_in.size()); ++i) {
        int vert_index = static_cast<int>(patch_indices_in[i]);
        int vert_next = static_cast<int>(patch_indices_in[(i + 1) % patch_indices_in.size()]);
        int edge_rate = facet_tess_rates[i];

        //
        //  Evaluate/assign or retrieve the shared point for the vertex:
        //
        const SharedVertex& shared_vertex = shared_verts[vert_index];
        if (!preserve_shared_indices || (shared_vertex.point_index >= num_mesh_points_before &&
                                         shared_vertex.local_vertex == i)) {
            //  Shared vertex has been assigned an index by this facet, interpolate
            int index_in_face = num_face_points_evaluated++;

            //  Evaluate new shared point and assign index to boundary:
            span<const ValueType> uv = tess_boundary_uvs.subspan(boundary_index * 2, 2);

            int p_index = index_in_face * num_channels;
            facet_surface.Evaluate(
                uv.data(),
                patch_values_in.data(),
                num_channels,
                &patch_values_out[p_index]);
        }
        ++boundary_index;

        //
        //  Evaluate/assign or retrieve all shared points for the edge:
        //
        //  To keep this simple, assume the edge is manifold. So the
        //  second face sharing the edge has that edge in the opposite
        //  direction in its boundary relative to the first face --
        //  making it necessary to reverse the order of shared points
        //  for the boundary of the second face.
        //
        //  To support a non-manifold edge, all subsequent faces that
        //  share the assigned shared edge must determine if their
        //  orientation of that edge is reversed relative to the first
        //  face for which the shared edge points were evaluated. So a
        //  little more book-keeping and/or inspection is required.
        //
        if (edge_rate > 1) {
            int points_per_edge = edge_rate - 1;

            const SharedEdge& shared_edge = shared_edges.find_edge(vert_index, vert_next);
            if (!preserve_shared_indices || (shared_edge.point_index >= num_mesh_points_before &&
                                             shared_edge.local_vertex == i)) {
                //  Identify indices of the new shared points in both the
                //  mesh and face and increment their inventory:
                int next_in_face = num_face_points_evaluated;
                num_face_points_evaluated += points_per_edge;

                //  Evaluate shared points and assign indices to boundary:
                span<const ValueType> uvs =
                    tess_boundary_uvs.subspan(boundary_index * 2, points_per_edge * 2);

                for (int j = 0; j < points_per_edge; ++j) {
                    int p_index = (next_in_face++) * num_channels;
                    facet_surface.Evaluate(
                        uvs.subspan(j * 2, 2).data(),
                        patch_values_in.data(),
                        num_channels,
                        &patch_values_out[p_index]);
                }
            }
            boundary_index += points_per_edge;
        }
    }

    //
    //  Evaluate any interior points unique to this face -- appending
    //  them to those shared points computed above for the boundary:
    //
    if (num_interior_coords) {
        span<const ValueType> uvs = tess_interior_uvs;

        int i_last = num_face_points_evaluated + num_interior_coords;
        for (int i = num_face_points_evaluated, k = 0; i < i_last; ++i, ++k) {
            int p_index = i * num_channels;
            facet_surface.Evaluate(
                uvs.subspan(k * 2, 2).data(),
                patch_values_in.data(),
                num_channels,
                &patch_values_out[p_index]);
        }
        num_face_points_evaluated += num_interior_coords;
    }

    return num_face_points_evaluated;
}

template <typename ValueType, typename Index>
void eval_patch_btn(
    OpenSubdiv::Bfr::Surface<ValueType>& facet_surface,
    const OpenSubdiv::Bfr::Tessellation& tess_pattern,
    int num_channels,
    span<const ValueType> patch_coords,
    span<const ValueType> patch_values_in,
    span<ValueType> patch_pos,
    span<ValueType> patch_du,
    span<ValueType> patch_dv,
    IndexedAttribute<ValueType, Index>* normals_out,
    IndexedAttribute<ValueType, Index>* tangents_out,
    IndexedAttribute<ValueType, Index>* bitangents_out,
    span<int> patch_indices_out,
    int first_corner,
    int patch_num_corners)
{
    using Vector3s = Eigen::Vector3<ValueType>;
    la_runtime_assert(
        num_channels == 3,
        "Limit normal/tangent/bitangent only available for meshes in dimension 3");

    //
    //  Evaluate the sample points of the Tessellation:
    //
    int num_coords = tess_pattern.GetNumCoords();
    for (int i = 0; i < num_coords; ++i) {
        int p_index = i * num_channels;
        facet_surface.Evaluate(
            patch_coords.subspan(i * 2, 2).data(),
            patch_values_in.data(),
            num_channels,
            &patch_pos[p_index],
            &patch_du[p_index],
            &patch_dv[p_index]);
        if (normals_out) {
            Vector3s du(patch_du[p_index], patch_du[p_index + 1], patch_du[p_index + 2]);
            Vector3s dv(patch_dv[p_index], patch_dv[p_index + 1], patch_dv[p_index + 2]);
            Vector3s normal = du.cross(dv).stableNormalized();
            std::copy_n(normal.data(), 3, patch_pos.begin() + p_index);
        }
    }
    // Evaluate corner indices
    auto pairs = {
        std::make_pair(normals_out, patch_pos),
        std::make_pair(tangents_out, patch_du),
        std::make_pair(bitangents_out, patch_dv),
    };
    tess_pattern.GetFacets(patch_indices_out.data());
    for (auto [attr, patch_values_out] : pairs) {
        if (attr) {
            // Insert values to attr
            auto& values = attr->values();
            size_t next_value = values.get_all().size();
            Index offset = values.get_num_elements();
            values.insert_elements(num_coords * num_channels);
            std::copy(
                patch_values_out.begin(),
                patch_values_out.end(),
                values.ref_all().begin() + next_value);

            // Insert indices to attr
            auto indices = attr->indices().ref_all().subspan(first_corner, patch_num_corners);
            auto nvpf = tess_pattern.GetFacetSize();
            for (int lf = 0, lc = 0; lf < tess_pattern.GetNumFacets(); ++lf) {
                for (int lv = 0; lv < nvpf; ++lv) {
                    if (nvpf == 4 && lv == 3 && patch_indices_out[lf * nvpf + lv] < 0) {
                        continue; // Skip last index
                    } else {
                        indices[lc++] =
                            static_cast<Index>(offset + patch_indices_out[lf * nvpf + lv]);
                    }
                }
            }
        }
    }
}

template <typename Scalar>
void compute_facet_tess_rates(
    const OpenSubdiv::Far::TopologyRefiner& mesh_topology,
    int face_index,
    OpenSubdiv::Bfr::Surface<Scalar>& facet_surface,
    span<const Scalar> mesh_vertex_positions,
    int dimension,
    std::vector<Scalar>& patch_values_in,
    std::vector<Scalar>& patch_values_out,
    bool use_limit_positions,
    Scalar tess_interval,
    int tess_rate_max,
    std::vector<int>& facet_tess_rates)
{
    //
    //  Prepare the Surface patch points first as it may be evaluated
    //  to determine suitable edge-rates for Tessellation:
    //
    patch_values_in.resize(facet_surface.GetNumPatchPoints() * dimension);
    facet_surface.PreparePatchPoints(
        mesh_vertex_positions.data(),
        dimension,
        patch_values_in.data(),
        dimension);

    //
    //  For each of the N edges of the face, a tessellation rate is
    //  determined to initialize a non-uniform Tessellation pattern.
    //
    //  Many metrics are possible -- some based on the geometry itself
    //  (size, curvature), others dependent on viewpoint (screen space
    //  size, center of view, etc.) and many more. Simple techniques
    //  are chosen here for illustration and can easily be replaced.
    //
    //  Here two methods are shown using lengths between the corners of
    //  the face -- the first using the vertex positions of the face and
    //  the second using points evaluated at the corners of its limit
    //  surface. Use of the control hull is more efficient (avoiding the
    //  evaluation) but may prove less effective in some cases (though
    //  both estimates have their limitations).
    //
    int N = facet_surface.GetFaceSize();

    patch_values_out.resize(N * dimension);

    if (!use_limit_positions) {
        OpenSubdiv::Far::ConstIndexArray verts =
            mesh_topology.GetLevel(0).GetFaceVertices(face_index);

        for (int i = 0, j = 0; i < N; ++i, j += dimension) {
            const Scalar* v_pos = &mesh_vertex_positions[verts[i] * dimension];
            patch_values_out[j] = v_pos[0];
            patch_values_out[j + 1] = v_pos[1];
            patch_values_out[j + 2] = v_pos[2];
        }
    } else {
        OpenSubdiv::Bfr::Parameterization face_param = facet_surface.GetParameterization();

        for (int i = 0, j = 0; i < N; ++i, j += dimension) {
            Scalar uv[2];
            face_param.GetVertexCoord(i, uv);
            facet_surface.Evaluate(uv, patch_values_in.data(), dimension, &patch_values_out[j]);
        }
    }

    facet_tess_rates.resize(N);
    get_edge_tess_rates<Scalar>(
        patch_values_out,
        dimension,
        tess_interval,
        tess_rate_max,
        facet_tess_rates.data());
}

using FVarId = OpenSubdiv::Bfr::SurfaceFactory::FVarID;

template <typename T>
using Surface = OpenSubdiv::Bfr::Surface<T>;

template <typename ValueType_, typename Index>
struct AttributeInfo
{
    using ValueType = ValueType_;
    span<const ValueType> values_in;
    Attribute<ValueType>& values_out;
    span<const Index> indices_in;
    Attribute<Index>* indices_out;
    std::string_view name;
    int num_channels;
    bool preserve_shared_indices;
};

template <typename ValueType, typename Index>
struct Surfaces
{
    // vertex data (per-vertex smoothly interpolated attributes)
    std::optional<Surface<ValueType>> vertex;

    // varying data (per-vertex linearly interpolated attributes)
    std::optional<Surface<ValueType>> varying;

    // face-varying data (indexed attributes)
    std::vector<Surface<ValueType>> face_varying;

    // face-varying ids
    std::vector<FVarId> fvar_ids;
};

template <typename ValueType, typename Index>
struct PatchCacheData
{
    std::vector<ValueType> patch_coords;
    std::vector<ValueType> patch_values_in;
    std::vector<ValueType> pos;
    std::vector<ValueType> du;
    std::vector<ValueType> dv;
};

template <typename ValueType, typename Index>
struct AttributeSurface
{
    AttributeInfo<ValueType, Index> attr;
    Surface<ValueType>* surface;
    std::vector<SharedVertex>* shared_verts;
    SharedEdges* shared_edges;
};

template <template <typename T, typename I> class Container, typename Index>
struct Selector
{
    Container<float, Index> f;
    Container<double, Index> d;

    template <typename ValueType>
    Container<ValueType, Index>& get()
    {
        if constexpr (std::is_same_v<ValueType, float>) {
            return f;
        } else {
            return d;
        }
    }
};

template <typename Scalar, typename Index>
void interpolate_attributes(
    OpenSubdiv::Bfr::RefinerSurfaceFactory<>& mesh_surface_factory,
    const OpenSubdiv::Bfr::Tessellation::Options& tess_options,
    const InterpolatedAttributeIds& interpolated_attr,
    const SurfaceMesh<Scalar, Index>& input_mesh,
    SurfaceMesh<Scalar, Index>& output_mesh,
    IndexedAttribute<Scalar, Index>* output_limit_normals,
    IndexedAttribute<Scalar, Index>* output_limit_tangents,
    IndexedAttribute<Scalar, Index>* output_limit_bitangents,
    bool use_limit_positions,
    Scalar tess_interval,
    int tess_rate_max,
    bool preserve_shared_indices)
{
    const bool need_limit_btn =
        (output_limit_normals || output_limit_tangents || output_limit_bitangents);

    // Surfaces parameterizing each attribute
    size_t num_indexed_attrs = interpolated_attr.face_varying_attributes.size();
    Selector<Surfaces, Index> surfaces;
    // Pre-allocate face_varying vector in advance to guarantee that we can take stable pointers to
    // its content
    surfaces.template get<float>().face_varying.resize(num_indexed_attrs);
    surfaces.template get<double>().face_varying.resize(num_indexed_attrs);

    //  Declare vectors to identify shared tessellation points at vertices
    //  and edges and their indices around the boundary of a face:
    std::vector<std::vector<SharedVertex>> all_shared_verts(num_indexed_attrs + 1);
    std::vector<SharedEdges> all_shared_edges(num_indexed_attrs + 1);

    all_shared_verts[0].resize(input_mesh.get_num_vertices());
    all_shared_edges[0].set_num_vertices(input_mesh.get_num_vertices());

    // Chain surfaces and attributes to interpolate in the correct order
    using AttributeSurfaceV =
        std::variant<AttributeSurface<float, Index>, AttributeSurface<double, Index>>;
    std::vector<AttributeSurfaceV> attributes_and_surfaces;

    // Prepare per-vertex attributes to interpolate
    auto prepare_vertex_attribute = [&](AttributeId id, bool smooth) {
        lagrange::internal::visit_attribute_read(input_mesh, id, [&](auto&& attr) {
            using AttributeType = std::decay_t<decltype(attr)>;
            using ValueType = typename AttributeType::ValueType;
            using AttributeSurface = AttributeSurface<ValueType, Index>;
            if constexpr (std::is_same_v<ValueType, float> || std::is_same_v<ValueType, double>) {
                la_debug_assert(attr.get_element_type() == AttributeElement::Vertex);
                if constexpr (!AttributeType::IsIndexed) {
                    AttributeId out_id = lagrange::internal::find_or_create_attribute<ValueType>(
                        output_mesh,
                        input_mesh.get_attribute_name(id),
                        AttributeElement::Vertex,
                        attr.get_usage(),
                        attr.get_num_channels(),
                        lagrange::internal::ResetToDefault::No);
                    AttributeType& out_attr = output_mesh.template ref_attribute<ValueType>(out_id);

                    AttributeInfo<ValueType, Index> info = {
                        attr.get_all(),
                        out_attr,
                        input_mesh.get_corner_to_vertex().get_all(),
                        nullptr,
                        input_mesh.get_attribute_name(id),
                        static_cast<int>(attr.get_num_channels()),
                        true};
                    if (id == input_mesh.attr_id_vertex_to_position()) {
                        info.indices_out = &output_mesh.ref_corner_to_vertex();
                    }

                    auto& sfc = surfaces.template get<ValueType>();
                    if (smooth) {
                        if (!sfc.vertex.has_value()) {
                            sfc.vertex.emplace();
                        }
                        attributes_and_surfaces.push_back(AttributeSurface{
                            info,
                            &sfc.vertex.value(),
                            &all_shared_verts[0],
                            &all_shared_edges[0]});
                    } else {
                        if (!sfc.varying.has_value()) {
                            sfc.varying.emplace();
                        }
                        attributes_and_surfaces.push_back(AttributeSurface{
                            info,
                            &sfc.varying.value(),
                            &all_shared_verts[0],
                            &all_shared_edges[0]});
                    }
                }
            }
        });
    };
    for (auto id : interpolated_attr.smooth_vertex_attributes) {
        prepare_vertex_attribute(id, true);
    }
    for (auto id : interpolated_attr.linear_vertex_attributes) {
        prepare_vertex_attribute(id, false);
    }

    // Prepare indexed attributes to interpolate (e.g. UVs)
    int fvar_index = 0;
    for (auto id : interpolated_attr.face_varying_attributes) {
        lagrange::internal::visit_attribute_read(input_mesh, id, [&](auto&& attr) {
            using AttributeType = std::decay_t<decltype(attr)>;
            using ValueType = typename AttributeType::ValueType;
            using AttributeSurface = AttributeSurface<ValueType, Index>;
            if constexpr (
                (std::is_same_v<ValueType, float> || std::is_same_v<ValueType, double>) &&
                AttributeType::IsIndexed) {
                AttributeId out_id = lagrange::internal::find_or_create_attribute<ValueType>(
                    output_mesh,
                    input_mesh.get_attribute_name(id),
                    AttributeElement::Indexed,
                    attr.get_usage(),
                    attr.get_num_channels(),
                    lagrange::internal::ResetToDefault::No);
                AttributeType& out_attr =
                    output_mesh.template ref_indexed_attribute<ValueType>(out_id);

                AttributeInfo<ValueType, Index> info = {
                    attr.values().get_all(),
                    out_attr.values(),
                    attr.indices().get_all(),
                    &out_attr.indices(),
                    input_mesh.get_attribute_name(id),
                    static_cast<int>(attr.get_num_channels()),
                    preserve_shared_indices};

                auto& sfc = surfaces.template get<ValueType>();

                size_t idx = sfc.fvar_ids.size();
                sfc.fvar_ids.push_back(static_cast<FVarId>(fvar_index++));
                logger().trace("FVar ID for attribute {}: {}", id, sfc.fvar_ids.back());
                all_shared_verts[fvar_index].resize(attr.values().get_num_elements());
                all_shared_edges[fvar_index].set_num_vertices(attr.values().get_num_elements());
                attributes_and_surfaces.push_back(AttributeSurface{
                    info,
                    &sfc.face_varying[idx],
                    &all_shared_verts[fvar_index],
                    &all_shared_edges[fvar_index]});
            }
        });
    }

    Selector<PatchCacheData, Index> patch_cache;
    struct
    {
        std::vector<int> facet_tess_rates;
        std::vector<int> tess_boundary_indices;
        std::vector<int> patch_indices_out;
    } tmp;

    auto eval_attribute = [&](auto& attr_surface,
                              int face_index,
                              int corner_index,
                              int old_num_values,
                              std::optional<int>& patch_nv,
                              std::optional<int>& patch_nf,
                              std::optional<int>& patch_nc) {
        auto& attr = attr_surface.attr;
        auto& surface = *attr_surface.surface;
        auto& shared_verts = *attr_surface.shared_verts;
        auto& shared_edges = *attr_surface.shared_edges;
        using ValueType = typename std::decay_t<decltype(attr)>::ValueType;
        auto& patch = patch_cache.template get<ValueType>();

        OpenSubdiv::Bfr::Tessellation tess_pattern(
            surface.GetParameterization(),
            surface.GetFaceSize(),
            tmp.facet_tess_rates.data(),
            tess_options);

        auto patch_indices_in = attr.indices_in.subspan(
            input_mesh.get_facet_corner_begin(face_index),
            input_mesh.get_facet_size(face_index));

        // Evaluate indices
        bool is_first = false;
        if (attr.indices_out == nullptr) {
            la_debug_assert(patch_nv.has_value());
        } else {
            if (!patch_nf.has_value()) {
                // Must be a vertex attribute, use old_num_values provided as argument
                is_first = true;
            } else {
                // Not a vertex attribute, retrieve prev num values from value attribute directly
                old_num_values = attr.values_out.get_num_elements();
            }

            patch_nv = eval_patch_indices(
                tess_pattern,
                tmp.facet_tess_rates,
                patch_indices_in,
                tmp.patch_indices_out,
                tmp.tess_boundary_indices,
                shared_verts,
                shared_edges,
                old_num_values,
                attr.preserve_shared_indices);

            auto nvpf = tess_pattern.GetFacetSize();
            if (!patch_nf.has_value()) {
                is_first = true;
                Index nc = output_mesh.get_num_corners();
                output_mesh.add_hybrid(
                    tess_pattern.GetNumFacets(),
                    [&](Index f) {
                        if (nvpf == 3) {
                            // Everything is a triangle
                            return 3;
                        } else {
                            // Maybe triangle or quad, check last index of the tessellated face
                            return tmp.patch_indices_out[f * nvpf + 3] < 0 ? 3 : 4;
                        }
                    },
                    [&](Index, span<Index>) {});
                patch_nc = static_cast<int>(output_mesh.get_num_corners() - nc);
            }
            // We may need to ignore the 4-th item in the list of indices when copying to our attr
            auto f_out = attr.indices_out->ref_all().subspan(corner_index, patch_nc.value());
            for (int lf = 0, lc = 0; lf < tess_pattern.GetNumFacets(); ++lf) {
                for (int lv = 0; lv < nvpf; ++lv) {
                    if (nvpf == 4 && lv == 3 && tmp.patch_indices_out[lf * nvpf + lv] < 0) {
                        continue; // Skip last index
                    } else {
                        f_out[lc++] = static_cast<Index>(tmp.patch_indices_out[lf * nvpf + lv]);
                    }
                }
            }
        }

        // Evaluate values
        if (is_first) {
            // Allocate new mesh vertices and resize all vertex attributes
            la_debug_assert(output_mesh.get_num_vertices() == static_cast<Index>(old_num_values));
            output_mesh.add_vertices(patch_nv.value());
        } else if (attr.indices_out) {
            // Insert new rows into the values of our indexed attribute
            attr.values_out.insert_elements(patch_nv.value());
        }
        span<ValueType> patch_values_out = attr.values_out.ref_all().subspan(
            old_num_values * attr.num_channels,
            patch_nv.value() * attr.num_channels);

        [[maybe_unused]] int nv = eval_patch_values(
            surface,
            tess_pattern,
            attr.num_channels,
            attr.values_in,
            tmp.facet_tess_rates,
            patch.patch_coords,
            patch.patch_values_in,
            patch_values_out,
            patch_indices_in,
            shared_verts,
            shared_edges,
            old_num_values,
            attr.preserve_shared_indices);

        if (is_first && need_limit_btn) {
            if constexpr (std::is_same_v<ValueType, Scalar>) {
                patch.pos.resize(tess_pattern.GetNumCoords() * attr.num_channels);
                patch.du.resize(tess_pattern.GetNumCoords() * attr.num_channels);
                patch.dv.resize(tess_pattern.GetNumCoords() * attr.num_channels);
                eval_patch_btn<ValueType, Index>(
                    surface,
                    tess_pattern,
                    attr.num_channels,
                    patch.patch_coords,
                    patch.patch_values_in,
                    patch.pos,
                    patch.du,
                    patch.dv,
                    output_limit_normals,
                    output_limit_tangents,
                    output_limit_bitangents,
                    tmp.patch_indices_out,
                    corner_index,
                    patch_nc.value());
            }
        }

        la_debug_assert(nv == patch_nv.value());

        if (!patch_nf.has_value()) {
            patch_nf = tess_pattern.GetNumFacets();
        } else {
            la_debug_assert(
                patch_nf.value() == tess_pattern.GetNumFacets(),
                "Inconsistent number of facets");
        }
    };

    int num_tess_vertices = 0;
    [[maybe_unused]] int num_tess_facets = 0;
    int num_tess_corners = 0;
    for (int face_index = 0; face_index < mesh_surface_factory.GetNumFaces(); ++face_index) {
        //
        //  Initialize the surfaces for this face -- if valid (skipping
        //  holes and boundary faces in some rare cases):
        //
        using Other = std::conditional_t<std::is_same_v<Scalar, float>, double, float>;
        auto init = [&](auto&& s) {
            using ValueType = std::decay_t<decltype(s)>;
            auto& sfc = surfaces.template get<ValueType>();
            return mesh_surface_factory.InitSurfaces(
                face_index,
                sfc.vertex.has_value() ? &sfc.vertex.value() : nullptr,
                sfc.fvar_ids.empty() ? nullptr : sfc.face_varying.data(),
                sfc.fvar_ids.data(),
                static_cast<int>(sfc.fvar_ids.size()),
                sfc.varying.has_value() ? &sfc.varying.value() : nullptr);
        };
        if (!init(Scalar(0))) {
            la_debug_assert(!init(Other(0)));
            continue;
        }
        init(Other(0));

        // Compute tessellation rates for the face edges
        tmp.facet_tess_rates.clear();
        compute_facet_tess_rates<Scalar>(
            mesh_surface_factory.GetMesh(),
            face_index,
            surfaces.template get<Scalar>().vertex.value(),
            input_mesh.get_vertex_to_position().get_all(),
            input_mesh.get_dimension(),
            patch_cache.template get<Scalar>().patch_values_in,
            patch_cache.template get<Scalar>().patch_coords,
            use_limit_positions,
            tess_interval,
            tess_rate_max,
            tmp.facet_tess_rates);

        // Interpolate all attributes. The first attribute in this list is the vertex position, and
        // it determines the edge tessellation rate for the facet.
        std::optional<int> patch_nv;
        std::optional<int> patch_nf;
        std::optional<int> patch_nc;
        bool first = true;
        int num_patch_vertices = 0;
        for (auto& var : attributes_and_surfaces) {
            std::visit(
                [&](auto& attr_surface) {
                    eval_attribute(
                        attr_surface,
                        face_index,
                        num_tess_corners,
                        num_tess_vertices,
                        patch_nv,
                        patch_nf,
                        patch_nc);
                },
                var);

            if (first) {
                num_patch_vertices += patch_nv.value();
                first = false;
            }
        }
        num_tess_vertices += num_patch_vertices;
        num_tess_facets += patch_nf.value();
        num_tess_corners += patch_nc.value();
    }

    la_debug_assert(
        output_mesh.get_vertex_to_position().get_num_elements() ==
        static_cast<Index>(num_tess_vertices));
    la_debug_assert(output_mesh.get_num_vertices() == static_cast<Index>(num_tess_vertices));
    la_debug_assert(output_mesh.get_num_facets() == static_cast<Index>(num_tess_facets));
    la_debug_assert(output_mesh.get_num_corners() == static_cast<Index>(num_tess_corners));

    output_mesh.shrink_to_fit();
}

//
//  The main tessellation function: given a mesh and vertex positions,
//  tessellate each face.
//
//  This tessellation function differs from earlier tutorials in that it
//  computes and reuses shared points at vertices and edges of the mesh.
//  There are several ways to compute these shared points, and which is
//  best depends on context.
//
//  Dealing with shared data poses complications for threading in general,
//  so computing all points for the vertices and edges up front may be
//  preferred -- despite the fact that faces will be visited more than once
//  (first when generating potentially shared vertex or edge points, and
//  later when generating any interior points). The loops for vertices and
//  edges can be threaded and the indexing of the shared points is simpler.
//
//  For the single-threaded case here, the faces are each processed in
//  order and any shared points will be computed and used as needed. So
//  each face is visited once (and so each Surface initialized once) but
//  the bookkeeping to deal with indices of shared points becomes more
//  complicated.
//
template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> extract_adaptive_mesh_topology(
    const SurfaceMesh<Scalar, Index>& input_mesh,
    const OpenSubdiv::Far::TopologyRefiner& mesh_topology,
    const InterpolatedAttributeIds& interpolated_attr,
    const SubdivisionOptions& options,
    Index dimension,
    bool output_quads,
    bool use_limit_positions,
    Scalar tess_interval,
    int tess_rate_max,
    bool preserve_shared_indices)
{
    //
    //  Initialize the SurfaceFactory for the given base mesh (very low
    //  cost in terms of both time and space) and tessellate each face
    //  independently (i.e. no shared vertices):
    //
    //  Note that the SurfaceFactory is not thread-safe by default due to
    //  use of an internal cache. Creating a separate instance of the
    //  SurfaceFactory for each thread is one way to safely parallelize
    //  this loop. Another (preferred) is to assign a thread-safe cache
    //  to the single instance.
    //
    //  First declare any evaluation options when initializing (though
    //  none are used in this simple case):
    //
    OpenSubdiv::Bfr::RefinerSurfaceFactory<> mesh_surface_factory(mesh_topology, {});

    //
    //  Assign Tessellation Options applied for all faces.  Tessellations
    //  allow the creating of either 3- or 4-sided faces -- both of which
    //  are supported here via a command line option:
    //
    const int tess_facet_size = 3 + (output_quads ? 1 : 0);

    OpenSubdiv::Bfr::Tessellation::Options tess_options;
    tess_options.SetFacetSize(tess_facet_size);
    tess_options.PreserveQuads(output_quads);

    //
    //  Process each face sequentially, computing interpolated attributes one at a time
    //
    SurfaceMesh<Scalar, Index> tessellated_mesh(dimension);

    // Prepare output BTN attributes
    IndexedAttribute<Scalar, Index>* output_limit_normals = nullptr;
    IndexedAttribute<Scalar, Index>* output_limit_tangents = nullptr;
    IndexedAttribute<Scalar, Index>* output_limit_bitangents = nullptr;
    if (!options.output_limit_normals.empty()) {
        AttributeId out_id = lagrange::internal::find_or_create_attribute<Scalar>(
            tessellated_mesh,
            options.output_limit_normals,
            AttributeElement::Indexed,
            AttributeUsage::Normal,
            3,
            lagrange::internal::ResetToDefault::No);
        output_limit_normals = &tessellated_mesh.template ref_indexed_attribute<Scalar>(out_id);
    }
    if (!options.output_limit_tangents.empty()) {
        AttributeId out_id = lagrange::internal::find_or_create_attribute<Scalar>(
            tessellated_mesh,
            options.output_limit_tangents,
            AttributeElement::Indexed,
            AttributeUsage::Tangent,
            3,
            lagrange::internal::ResetToDefault::No);
        output_limit_tangents = &tessellated_mesh.template ref_indexed_attribute<Scalar>(out_id);
    }
    if (!options.output_limit_bitangents.empty()) {
        AttributeId out_id = lagrange::internal::find_or_create_attribute<Scalar>(
            tessellated_mesh,
            options.output_limit_bitangents,
            AttributeElement::Indexed,
            AttributeUsage::Bitangent,
            3,
            lagrange::internal::ResetToDefault::No);
        output_limit_bitangents = &tessellated_mesh.template ref_indexed_attribute<Scalar>(out_id);
    }

    interpolate_attributes<Scalar>(
        mesh_surface_factory,
        tess_options,
        interpolated_attr,
        input_mesh,
        tessellated_mesh,
        output_limit_normals,
        output_limit_tangents,
        output_limit_bitangents,
        use_limit_positions,
        tess_interval,
        tess_rate_max,
        preserve_shared_indices);

    return tessellated_mesh;
}

} // namespace

//------------------------------------------------------------------------------

template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> subdivide_edge_adaptive(
    const SurfaceMesh<Scalar, Index>& input_mesh,
    OpenSubdiv::Far::TopologyRefiner& topology_refiner,
    const InterpolatedAttributeIds& interpolated_attr,
    const SubdivisionOptions& options)
{
    if (!options.use_limit_surface) {
        logger().warn(
            "Adaptive subdivision always interpolates to the limit surface. To ignore this "
            "warning, please set 'use_limit_surface' to 'true' in your subdivision options.");
    }

    // Extract mesh facet topology
    bool output_quads = !input_mesh.is_triangle_mesh();
    bool use_limit_positions = true;
    Scalar tess_interval;

    // Only limit max edge tessellation if not target edge length is specified
    int tess_rate_max =
        (options.max_edge_length.has_value() ? std::numeric_limits<int>::max()
                                             : std::max(1u, options.num_levels));

    logger().debug("Output quads? {}", output_quads);

    if (options.max_edge_length.has_value()) {
        tess_interval = options.max_edge_length.value();
    } else {
        auto [min_len, max_len, avg_len] = find_min_max_avg_edges(
            topology_refiner,
            input_mesh.get_vertex_to_position().get_all(),
            input_mesh.get_dimension());
        tess_interval = max_len / static_cast<float>(tess_rate_max);
        logger().info(
            "Adaptive tessellation.\n\t- Max edge len: {},\n\t- Min edge len: {},\n\t- Avg edge "
            "len: {},\n\t- Max rate: {},\n\t- Tess interval: {}",
            max_len,
            min_len,
            avg_len,
            tess_rate_max,
            tess_interval);
    }

    SurfaceMesh<Scalar, Index> output_mesh = extract_adaptive_mesh_topology(
        input_mesh,
        topology_refiner,
        interpolated_attr,
        options,
        input_mesh.get_dimension(),
        output_quads,
        use_limit_positions,
        tess_interval,
        tess_rate_max,
        options.preserve_shared_indices);

    return output_mesh;
}

#define LA_X_subdivide_edge_adaptive(_, Scalar, Index)                              \
    template LA_SUBDIVISION_API SurfaceMesh<Scalar, Index> subdivide_edge_adaptive( \
        const SurfaceMesh<Scalar, Index>& input_mesh,                               \
        OpenSubdiv::Far::TopologyRefiner& topology_refiner,                         \
        const InterpolatedAttributeIds& interpolated_attr,                          \
        const SubdivisionOptions& options);
LA_SURFACE_MESH_X(subdivide_edge_adaptive, 0)

} // namespace lagrange::subdivision
