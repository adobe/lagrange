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
#pragma once

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <Eigen/Core>

#include <lagrange/Edge.h>
#include <lagrange/MeshTrait.h>
#include <lagrange/utils/la_assert.h>
#include <lagrange/utils/range.h>

namespace lagrange {

template <typename MeshType_>
struct MarchingTrianglesOutput
{
    using MeshType = MeshType_;
    using Index = typename MeshType::Index;
    using IndexList = typename MeshType::IndexList;
    using Scalar = typename MeshType::Scalar;
    using VertexArray = typename MeshType::VertexArray;
    using EdgeArray = Eigen::Matrix<Index, Eigen::Dynamic, 2, Eigen::RowMajor>;

    // The extracted vertices of the contour
    VertexArray vertices;
    // Note that the direction in the edges bears no particular meaning
    EdgeArray edges;
    // The edge parent that gives birth to a marching triangle vertex
    IndexList vertices_parent_edge;
    // The param ( 0 =< t =< 1 ) that gives birth to each vertex.
    // NOTE: t is defined such that position of vertex is (1-t)*v0 + t*v1,
    // i.e., t=0 means the vertex is on v0 and t=1 means that it is on v1.
    std::vector<Scalar> vertices_parent_param;
};

// Perform marching triangles to extract the isocontours of a field
// defined by the linear interpolation of a vertex attribute.
//
// Adapted from https://www.cs.ubc.ca/~rbridson/download/common_2008_nov_12.tar.gz
// (code released to the __public domain__ by Robert Bridson)
//
// Input:
//    mesh_ref, the mesh to run marching cubes on. Can be 2D or 3D,
//              but must be triangular.
//    isovalue, the isovalue to be extracted.
//    vertex_attribute_name, the name of the vertex attribute
//    attribute_col_index, which column of the vertex attribute should be used?
//
template <typename MeshType>
MarchingTrianglesOutput<MeshType> marching_triangles(
    MeshType& mesh_ref,
    const typename MeshType::Scalar isovalue,
    const std::string vertex_attribute_name,
    const typename MeshType::Index attribute_col_index = 0)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");
    using Index = typename MeshType::Index;
    using Scalar = typename MeshType::Scalar;
    using VertexType = typename MeshType::VertexType;
    using MarchingTrianglesOutput = ::lagrange::MarchingTrianglesOutput<MeshType>;
    using Edge = typename MeshType::Edge;

    LA_ASSERT(
        mesh_ref.has_vertex_attribute(vertex_attribute_name),
        "attribute does not exist in the mesh");
    LA_ASSERT(mesh_ref.get_vertex_per_facet() == 3, "only works for triangle meshes");
    if (!mesh_ref.is_edge_data_initialized_new()) {
        mesh_ref.initialize_edge_data_new();
    }

    const auto& attribute = mesh_ref.get_vertex_attribute(vertex_attribute_name);
    LA_ASSERT(attribute_col_index < safe_cast<Index>(attribute.cols()), "col_index is invalid");
    const auto& facets = mesh_ref.get_facets();
    const auto& vertices = mesh_ref.get_vertices();

    std::vector<Edge> extracted_edges;
    std::vector<VertexType> extracted_vertices;
    std::vector<Index> extracted_vertices_parent_edge;
    std::vector<Scalar> extracted_vertices_parent_param;
    std::vector<Index> parent_edge_to_extracted_vertex(
        mesh_ref.get_num_edges_new(),
        INVALID<Index>());

    //
    // Find the point that attains a zero value on an edge
    // (if it does not exists, creates it)
    // Returns the index of this vertex
    //
    auto find_zero = [&](Index parent_edge_id, Index v0, Index v1, Scalar p0, Scalar p1) -> Index {
        if (parent_edge_to_extracted_vertex[parent_edge_id] != INVALID<Index>()) {
            return parent_edge_to_extracted_vertex[parent_edge_id];
        } else {
            // Get the edge and see if the order of v0 v1 and the edge values are the same.
            const auto parent_edge = mesh_ref.get_edge_vertices_new(parent_edge_id);
            if ((v0 == parent_edge[1]) && (v1 == parent_edge[0])) {
                std::swap(p0, p1);
                std::swap(v0, v1);
            }
            assert((v1 == parent_edge[1]) && (v0 == parent_edge[0]));
            // Now construct the vertex
            const Scalar a = p1 / (p1 - p0);
            const Scalar b = 1 - a;
            assert(a >= 0);
            assert(a <= 1);
            assert(b >= 0);
            assert(b <= 1);
            LA_ASSERT(!std::isnan(a));
            LA_ASSERT(!std::isnan(b));
            const Index vertex_index = safe_cast<Index>(extracted_vertices.size());
            const VertexType pos0 = vertices.row(v0);
            const VertexType pos1 = vertices.row(v1);
            // And push it into the data structures
            extracted_vertices.emplace_back(a * pos0 + b * pos1);
            extracted_vertices_parent_edge.emplace_back(parent_edge_id);
            extracted_vertices_parent_param.emplace_back(b);
            parent_edge_to_extracted_vertex[parent_edge_id] = vertex_index;
            return vertex_index;
        }
    };

    //
    // Find the contour (if exists) in a triangle
    //
    auto contour_triangle = [&](const Index tri_id) {
        const Index v0 = facets(tri_id, 0);
        const Index v1 = facets(tri_id, 1);
        const Index v2 = facets(tri_id, 2);
        Scalar p0 = attribute(v0, attribute_col_index) - isovalue;
        Scalar p1 = attribute(v1, attribute_col_index) - isovalue;
        Scalar p2 = attribute(v2, attribute_col_index) - isovalue;

        const Index e01 = mesh_ref.get_edge_new(tri_id, 0);
        const Index e12 = mesh_ref.get_edge_new(tri_id, 1);
        const Index e20 = mesh_ref.get_edge_new(tri_id, 2);

        // guard against topological degeneracies
        if (p0 == 0) p0 = Scalar(1e-30);
        if (p1 == 0) p1 = Scalar(1e-30);
        if (p2 == 0) p2 = Scalar(1e-30);

        if (p0 < 0) {
            if (p1 < 0) {
                if (p2 < 0) {
                    return; // no contour here
                } else { /* p2>0 */
                    extracted_edges.push_back(
                        Edge(find_zero(e12, v1, v2, p1, p2), find_zero(e20, v0, v2, p0, p2)));
                } // p2
            } else { // p1>0
                if (p2 < 0) {
                    extracted_edges.push_back(
                        Edge(find_zero(e01, v0, v1, p0, p1), find_zero(e12, v1, v2, p1, p2)));
                } else { /* p2>0 */
                    extracted_edges.push_back(
                        Edge(find_zero(e01, v0, v1, p0, p1), find_zero(e20, v0, v2, p0, p2)));
                } // p2
            } // p1
        } else { // p0>0
            if (p1 < 0) {
                if (p2 < 0) {
                    extracted_edges.push_back(
                        Edge(find_zero(e20, v0, v2, p0, p2), find_zero(e01, v0, v1, p0, p1)));
                } else { /* p2>0 */
                    extracted_edges.push_back(
                        Edge(find_zero(e12, v1, v2, p1, p2), find_zero(e01, v0, v1, p0, p1)));
                } // p2
            } else { // p1>0
                if (p2 < 0) {
                    extracted_edges.push_back(
                        Edge(find_zero(e20, v0, v2, p0, p2), find_zero(e12, v1, v2, p1, p2)));
                } else { /* p2>0 */
                    return; // no contour here
                } // p2
            } // p1
        } // p0
    };


    //
    // Contour all triangles
    //
    for (auto tri : range(mesh_ref.get_num_facets())) {
        contour_triangle(tri);
    }

    //
    // Now convert to output
    //
    MarchingTrianglesOutput output;
    // Output edges
    output.edges.resize(extracted_edges.size(), 2);
    for (auto i : range(extracted_edges.size())) {
        output.edges.row(i) << extracted_edges[i][0], extracted_edges[i][1];
    }
    // output vertices
    // This we can just swap!
    output.vertices_parent_edge.swap(extracted_vertices_parent_edge);
    output.vertices_parent_param.swap(extracted_vertices_parent_param);
    // rest we have to copy
    output.vertices.resize(extracted_vertices.size(), mesh_ref.get_dim());
    for (auto i : range(extracted_vertices.size())) {
        output.vertices.row(i) = extracted_vertices[i];
    }
    return output;
}

} // namespace lagrange
