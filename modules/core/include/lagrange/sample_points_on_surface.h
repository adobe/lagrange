/*
 * Copyright 2019 Adobe. All rights reserved.
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

#include <unordered_map>

#include <lagrange/common.h>
#include <lagrange/compute_facet_area.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/range.h>

#include <list>

namespace lagrange {

// sample_points_on_surface()
//
// Samples points on a mesh as uniformly as possible by splitting edges
// until they are smaller than a certain value and also not sampling more
// than one point inside a grid.
//
// This is a Lagrangified version of segmentation/PointSample::SampleViaGrid()

// Output values
template <typename MeshType>
struct SamplePointsOnSurfaceOutput
{
public:
    using Index = typename MeshType::Index;
    using IndexList = typename MeshType::IndexList;
    using VertexArray = typename MeshType::VertexArray;
    // Same as vertex array, but must have 3 cols rather than whatever vertex array has
    using BarycentricArray = Eigen::Matrix<
        typename VertexArray::Scalar,
        VertexArray::RowsAtCompileTime,
        3, // VertexArray::ColsAtCompileTime,
        VertexArray::Options>;

    // Number of samples points
    Index num_samples = 0;
    // The facet id of the sampled points
    IndexList facet_ids = IndexList(0);
    // Coordinates of the samples points
    VertexArray positions = VertexArray();
    // Barycentric coordinates of the samples points
    BarycentricArray barycentrics = BarycentricArray();
};

//
// A few version follow with different ways of initializing the
// list of active facts.
//

// You can choose which facets to sample from by specifying an array of indices.
// Inputs
// - The mesh
// - Approximate number of points to be sampled
// - Index of the faces to be sampled from, an empty list of indices means all faces.
template <typename MeshType>
SamplePointsOnSurfaceOutput<MeshType> sample_points_on_surface(
    MeshType& mesh,
    const typename MeshType::Index approx_num_points,
    const typename MeshType::IndexList& active_facets)
{
    // Helper types
    using Index = typename MeshType::Index;
    using Scalar = typename MeshType::Scalar;
    using VertexType = typename MeshType::VertexType;
    //
    using Output = SamplePointsOnSurfaceOutput<MeshType>;
    //
    using RowVectorS = Eigen::Matrix<Scalar, 1, Eigen::Dynamic>;
    using RowVector3S = Eigen::Matrix<Scalar, 1, 3>;
    using RowVectorI = Eigen::Matrix<Index, 1, Eigen::Dynamic>;
    using BoundingBox = Eigen::AlignedBox<Scalar, Eigen::Dynamic>;

    // Check conditions to be met by input
    la_runtime_assert(mesh.get_vertex_per_facet() == 3, "only works for triangle meshes");
    la_runtime_assert(approx_num_points != invalid<Index>());
    la_runtime_assert(active_facets.size() <= safe_cast<size_t>(mesh.get_num_facets()));
    for (const auto facet_id : active_facets) {
        la_runtime_assert(facet_id < mesh.get_num_facets());
    }


    // Information regarding the mesh that we need
    if (!mesh.has_facet_attribute("area")) {
        compute_facet_area(mesh);
    }
    const auto& facet_area = mesh.get_facet_attribute("area");
    const auto& facets = mesh.get_facets();
    const auto& vertices = mesh.get_vertices();

    // Get space dimensions
    const Index n_dims = mesh.get_dim();

    // Compute the total area of active part of the mesh
    // Otherwise we could have just used total_mesh_area = facet_area.sum().
    // In the new version of eigen, we can also use facet_area( active_facets ).sum()
    Scalar total_mesh_area = 0;
    for (auto facet_id : range_sparse(mesh.get_num_facets(), active_facets)) {
        total_mesh_area += facet_area(facet_id);
    }

    // total area of zero is bad
    la_runtime_assert(total_mesh_area != 0.0);

    // Estimate the sampling length
    // If each point is a disk, what should be the radius of disks to cover  the surface
    // this radius would be the sampling length
    // 1.5 is probably a safety factor
    const Scalar sampling_length = 1.5 * sqrt((total_mesh_area / (approx_num_points * M_PI)));

    // Find the bounding box of the active facets on the mesh
    BoundingBox bounding_box(n_dims);
    for (auto facet_id : range_sparse(mesh.get_num_facets(), active_facets)) {
        for (auto vertex_offset : range(mesh.get_vertex_per_facet())) {
            const Index vertex_id = facets(facet_id, vertex_offset);
            // bbox needs a column vector . Hence the transpose(). Arg...
            bounding_box.extend(vertices.row(vertex_id).transpose());
        }
    }

    // Find the side lengths and dimensions of the grid used for sampling
    const RowVectorS grid_lens = bounding_box.sizes();
    const RowVectorI grid_dims =
        (grid_lens / sampling_length).array().ceil().template cast<Index>();

    // Number of cells in the grid
    const Index num_grid_cells = grid_dims.prod();

    // Given a 2(3)-D index gives the flattened index of a cell in the grid
    auto get_flattend_index = [grid_dims, n_dims](const RowVectorI& index_nd) -> Index {
        // Some debug time assertions
        assert(index_nd.minCoeff() >= 0);
        assert((index_nd.array() < grid_dims.array()).all());
        //
        Index answer = invalid<Index>();
        //
        if (n_dims == 2) {
            answer = index_nd(1) * grid_dims(0) + index_nd(0);
        } else if (n_dims == 3) {
            LA_IGNORE_ARRAY_BOUNDS_BEGIN
            answer = index_nd(2) * grid_dims(1) * grid_dims(0) + index_nd(1) * grid_dims(0) +
                     index_nd(0);
            LA_IGNORE_ARRAY_BOUNDS_END
        } else {
            la_runtime_assert(0, "This dimension is not supported");
        }
        //
        assert(answer >= 0 && answer < grid_dims.prod());
        //
        return answer;
    };

    // A data structure to indicate if we have already sampled from a cell of the grid
    // A vector, this can be too memory consuming
    // std::vector<bool> is_grid_cell_marked_backend(num_grid_cells, false);
    // Hash map
    std::unordered_set<Index> is_grid_cell_marked_backend(num_grid_cells);
    //
    // A labmda to see if a grid cell is already marked.
    auto is_grid_cell_marked = [&is_grid_cell_marked_backend](const Index cell_index) {
        // for a vector
        // return is_grid_cell_marked_backend[cell_index];
        // for the hashmap
        return is_grid_cell_marked_backend.count(cell_index) != 0;
    };
    //
    // A lambda to mark a grid cell
    auto mark_grid_cell = [&is_grid_cell_marked_backend](const Index cell_index) {
        // for a vector
        // is_grid_cell_marked_backend[cell_index] = true;
        // for hashmap
        is_grid_cell_marked_backend.insert(cell_index);
    };


    // Hold the info about the sample points in a list
    // once the process is finished move them to the output struct
    std::list<VertexType> sample_positions;
    std::list<Index> sample_facet_ids;
    std::list<RowVector3S> sample_barycentrics;
    Index num_samples = 0;

    //
    // A queue holding the triangles generated by splitting.
    // Each triangle is represented by a 3x6 vector.
    // Each row represents a vertex.
    // The first three(two) columns represent the positions in the xy(z) space.
    // The second three columns are the barycentric coordinates of the vertices w.r.t to the
    // mother triangle (the non splitted one).
    //
    const Index num_triangle_cols = 3 + n_dims;
    using Triangle = Eigen::Matrix<Scalar, 3, Eigen::Dynamic>;
    auto get_triangle_pos = [&n_dims](const Triangle& tri, const Index vert_offset) -> RowVectorS {
        return tri.row(vert_offset).head(n_dims);
    };
    auto get_triangle_bary = [](const Triangle& tri, const Index vert_offset) -> RowVectorS {
        return tri.row(vert_offset).tail(3);
    };
    auto set_triangle_pos =
        [&n_dims](Triangle& tri, const Index vert_offset, const RowVectorS& v) -> void {
        tri.row(vert_offset).head(n_dims) = v;
    };
    auto set_triangle_bary = [](Triangle& tri,
                                const Index vert_offset,
                                const RowVectorS& v) -> void { tri.row(vert_offset).tail(3) = v; };
    std::queue<Triangle> sub_triangles;

    //
    // Loop over facets
    // subdivide each facet to the point that you get a triangle smaller
    // than sampling length. Then get the midpoint of that triangle if
    // its corresponding grid cell is not marked already.
    //
    for (auto facet_id : range_sparse(mesh.get_num_facets(), active_facets)) {
        //
        // Splitting code follows.
        // This is done rather inefficiently using a queue. There is no need for that.
        // If a samaritan with free time was passing by, they can  make it better :).
        //

        // Empty out the queue
        sub_triangles = std::queue<Triangle>();

        // Create the mother triangle
        Triangle mother_facet = Triangle::Zero(3, num_triangle_cols);
        set_triangle_pos(mother_facet, 0, vertices.row(facets(facet_id, 0)));
        set_triangle_pos(mother_facet, 1, vertices.row(facets(facet_id, 1)));
        set_triangle_pos(mother_facet, 2, vertices.row(facets(facet_id, 2)));
        set_triangle_bary(mother_facet, 0, RowVector3S(1, 0, 0));
        set_triangle_bary(mother_facet, 1, RowVector3S(0, 1, 0));
        set_triangle_bary(mother_facet, 2, RowVector3S(0, 0, 1));

        // Push it to the queue
        sub_triangles.push(mother_facet);

        // Now process the triagles in the queue
        while (!sub_triangles.empty()) {
            const Triangle current_facet = sub_triangles.front();
            sub_triangles.pop();

            // Find the longest edge in the triangle
            Scalar longest_edge_length = -std::numeric_limits<Scalar>::max();
            Index longest_edge_offset = invalid<Index>();
            for (auto edge_offset : range(3)) {
                const auto edge_offset_p1 = (edge_offset + 1) % 3;
                const Scalar edge_length = (get_triangle_pos(current_facet, edge_offset) //
                                            - get_triangle_pos(current_facet, edge_offset_p1))
                                               .norm();
                if (edge_length > longest_edge_length) {
                    longest_edge_length = edge_length;
                    longest_edge_offset = edge_offset;
                }
            }

            // If too big, subdivide
            if (longest_edge_length > 2 * sampling_length) {
                Triangle f1 = Triangle::Zero(3, num_triangle_cols);
                Triangle f2 = Triangle::Zero(3, num_triangle_cols);
                //
                f1.row(0) = (current_facet.row(longest_edge_offset) +
                             current_facet.row((longest_edge_offset + 1) % 3)) /
                            2;
                f1.row(1) = current_facet.row(longest_edge_offset);
                f1.row(2) = current_facet.row((longest_edge_offset + 2) % 3);
                //
                f2.row(0) = f1.row(0);
                f2.row(1) = current_facet.row((longest_edge_offset + 1) % 3);
                f2.row(2) = current_facet.row((longest_edge_offset + 2) % 3);
                //
                sub_triangles.push(f1);
                sub_triangles.push(f2);
            } else { // Otherwise, sample the centroid
                const RowVector3S point_barycentric =
                    (get_triangle_bary(current_facet, 0) + get_triangle_bary(current_facet, 1) +
                     get_triangle_bary(current_facet, 2)) /
                    3.;
                const VertexType point_position =
                    (get_triangle_pos(current_facet, 0) + get_triangle_pos(current_facet, 1) +
                     get_triangle_pos(current_facet, 2)) /
                    3.;


                // Get the corresponding cell in the grid
                // min() returns column vector, hence the tranpose(), arg...
                const RowVectorI grid_cell =
                    ((point_position - bounding_box.min().transpose()) / sampling_length)
                        .array()
                        .floor()
                        .template cast<Index>();
                const Index grid_cell_id = get_flattend_index(grid_cell);

                if (!is_grid_cell_marked(grid_cell_id)) {
                    mark_grid_cell(grid_cell_id);
                    // record sample facet
                    sample_facet_ids.push_back(facet_id);
                    // record barycetric (in mother facet)
                    sample_barycentrics.push_back(point_barycentric);
                    // record the position
                    sample_positions.push_back(point_position);
                    //
                    ++num_samples;
                } // end of grid check
            } // end of size check
        } //  end of loop over queue
    } // end of loop over faces


    // Now copy the result into the output struct
    Output output;
    {
        output.num_samples = num_samples;
        //
        output.barycentrics.resize(num_samples, 3);
        output.facet_ids.resize(num_samples);
        output.positions.resize(num_samples, n_dims);
        //
        auto it_barycentric = sample_barycentrics.begin();
        auto it_facet_id = sample_facet_ids.begin();
        auto it_position = sample_positions.begin();
        //
        for (Index i : range(num_samples)) {
            output.barycentrics.row(i) = *it_barycentric;
            output.facet_ids[i] = *it_facet_id;
            output.positions.row(i) = *it_position;
            ++it_barycentric;
            ++it_facet_id;
            ++it_position;
        }
    }

    return output;
}

// Active facets are specified by an array of bools
// Inputs
// - The mesh
// - Approximate number of points to be sampled
// - Array of bools saying whether a facet should be sampled from or not.
template <typename MeshType>
SamplePointsOnSurfaceOutput<MeshType> sample_points_on_surface(
    MeshType& mesh,
    const typename MeshType::Index approx_num_points,
    const std::vector<bool>& is_facet_active)
{
    typename MeshType::IndexList active_facets;
    for (auto i : range(safe_cast<typename MeshType::Index>(is_facet_active.size()))) {
        if (is_facet_active[i]) {
            active_facets.push_back(i);
        }
    }
    return sample_points_on_surface(mesh, approx_num_points, active_facets);
}

// All the facets will be sampled
// Inputs
// - The mesh
// - Approximate number of points to be sampled
template <typename MeshType>
SamplePointsOnSurfaceOutput<MeshType> sample_points_on_surface(
    MeshType& mesh,
    const typename MeshType::Index approx_num_points)
{
    return sample_points_on_surface(mesh, approx_num_points, typename MeshType::IndexList());
}

} // namespace lagrange
