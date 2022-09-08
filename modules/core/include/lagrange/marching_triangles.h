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

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <Eigen/Core>

#include <lagrange/Edge.h>
#include <lagrange/MeshTrait.h>
#include <lagrange/utils/assert.h>
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

/**
 * Perform marching triangles to extract isocontour on a field defined as
 * the linear interpolation of values provided by the `get_value` function.
 *
 * Adapted from https://www.cs.ubc.ca/~rbridson/download/common_2008_nov_12.tar.gz
 * (code released to the __public domain__ by Robert Bridson)
 *
 * @param[in]  mesh_ref  The input mesh.
 * @param[in]  isovalue  The isovalue of the field at which to contour.
 * @param[in]  get_value A function that takes the facet id and a local corner
 *                       id and returns the field value at that corner.
 *
 * @returns The extracted isocontour.
 */
template <typename MeshType, typename ValueFn>
MarchingTrianglesOutput<MeshType> marching_triangles_general(
    MeshType& mesh_ref,
    ScalarOf<MeshType> isovalue,
    const ValueFn& get_value)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");
    using Index = typename MeshType::Index;
    using Scalar = typename MeshType::Scalar;
    using VertexType = typename MeshType::VertexType;
    using MarchingTrianglesOutput = ::lagrange::MarchingTrianglesOutput<MeshType>;
    using Edge = typename MeshType::Edge;

    la_runtime_assert(mesh_ref.get_vertex_per_facet() == 3, "only works for triangle meshes");
    mesh_ref.initialize_edge_data();

    const auto& facets = mesh_ref.get_facets();
    const auto& vertices = mesh_ref.get_vertices();

    std::vector<Edge> extracted_edges;
    std::vector<VertexType> extracted_vertices;
    std::vector<Index> extracted_vertices_parent_edge;
    std::vector<Scalar> extracted_vertices_parent_param;
    std::vector<Index> parent_edge_to_extracted_vertex(mesh_ref.get_num_edges(), invalid<Index>());

    //
    // Find the point that attains a zero value on an edge
    // (if it does not exists, creates it)
    // Returns the index of this vertex
    //
    auto find_zero = [&](Index parent_edge_id, Index v0, Index v1, Scalar p0, Scalar p1) -> Index {
        if (parent_edge_to_extracted_vertex[parent_edge_id] != invalid<Index>()) {
            return parent_edge_to_extracted_vertex[parent_edge_id];
        } else {
            // Get the edge and see if the order of v0 v1 and the edge values are the same.
            const auto parent_edge = mesh_ref.get_edge_vertices(parent_edge_id);
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
            la_runtime_assert(!std::isnan(a));
            la_runtime_assert(!std::isnan(b));
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
        Scalar p0 = get_value(tri_id, 0) - isovalue;
        Scalar p1 = get_value(tri_id, 1) - isovalue;
        Scalar p2 = get_value(tri_id, 2) - isovalue;

        const Index e01 = mesh_ref.get_edge(tri_id, 0);
        const Index e12 = mesh_ref.get_edge(tri_id, 1);
        const Index e20 = mesh_ref.get_edge(tri_id, 2);

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
    using Index = IndexOf<MeshType>;
    la_runtime_assert(
        mesh_ref.has_vertex_attribute(vertex_attribute_name),
        "attribute does not exist in the mesh");
    const auto& attribute = mesh_ref.get_vertex_attribute(vertex_attribute_name);
    la_runtime_assert(
        attribute_col_index < safe_cast<Index>(attribute.cols()),
        "col_index is invalid");

    const auto& facets = mesh_ref.get_facets();
    return marching_triangles_general(mesh_ref, isovalue, [&](Index fi, Index ci) {
        return attribute(facets(fi, ci), attribute_col_index);
    });
}

/**
 * Perform marching triangles to extract isocontours on a field defined as
 * the linear interpolation of an indexed attribute.
 *
 * @param[in]  mesh_ref                The input triangle mesh.
 * @param[in]  isovalue                The isovalue of the field at which to contour.
 * @param[in]  indexed_attribute_name  The indexed attribute name defining the
 *                                     field.
 * @param[in]  attribute_col_index     The attribute channel to use (for vector
 *                                     attributes only).
 * @returns The extracted isocontour.
 *
 * @note The indexed attribute can be used to define fields with
 * discontinuities.  However, result may contain artifacts if the desired
 * iso-contour passes through such discontiuity.
 */
template <typename MeshType>
MarchingTrianglesOutput<MeshType> marching_triangles_indexed(
    MeshType& mesh_ref,
    const typename MeshType::Scalar isovalue,
    const std::string indexed_attribute_name,
    const typename MeshType::Index attribute_col_index = 0)
{
    using Index = IndexOf<MeshType>;
    la_runtime_assert(
        mesh_ref.has_indexed_attribute(indexed_attribute_name),
        "attribute does not exist in the mesh");
    const auto& attribute = mesh_ref.get_indexed_attribute(indexed_attribute_name);
    la_runtime_assert(
        attribute_col_index < safe_cast<Index>(std::get<0>(attribute).cols()),
        "col_index is invalid");

    return marching_triangles_general(mesh_ref, isovalue, [&](Index fi, Index ci) {
        return std::get<0>(attribute)(std::get<1>(attribute)(fi, ci), attribute_col_index);
    });
}

} // namespace lagrange
