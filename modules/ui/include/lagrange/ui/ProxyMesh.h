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
#include <lagrange/Mesh.h>
#include <lagrange/ui/AABB.h>
#include <lagrange/ui/Frustum.h>

#include <array>
#include <memory>
#include <unordered_set>
#include <vector>

namespace lagrange {
namespace ui {

class ElementSelection;
class ProxyMesh;
struct ProxyMeshAccelImpl;


///
/// Proxy representation of a lagrange mesh for rendering and picking
///     Expands 2D vertices to 3D (z=0)
///     Triangulates polygon meshes (and stores the bidirectional mapping)
///     Initializes normals if they do not exist
///     Computes tangent/bitangent vectors if UVs exist
///     Initializes edge data if they are not present in the original
class ProxyMesh
{
public:
    using Scalar = float;
    using Index = int; // converted to unsigned int for gl later
    using AttributeArray = TriangleMesh3Df::AttributeArray;
    using VertexArray = TriangleMesh3Df::VertexArray;
    using FacetArray = TriangleMesh3Df::FacetArray;
    using Edge = TriangleMesh3Df::Edge;

    const TriangleMesh3Df& mesh() const { return *m_mesh; }

    template <typename MeshType>
    ProxyMesh(const MeshType& mesh);

    ~ProxyMesh();

    // Forward common queries to the triangle mesh

    /// Get proxy's vertices
    const VertexArray& get_vertices() const { return m_mesh->get_vertices(); }

    /// Get proxy's facets
    const FacetArray& get_facets() const { return m_mesh->get_facets(); }

    /// Get number of proxy's vertices (identical to original)
    Index get_num_vertices() const { return m_mesh->get_num_vertices(); }

    /// Get humber of proxy's facets (after triangulation)
    Index get_num_triangles() const { return m_mesh->get_num_facets(); }

    /// Returns precomputed bounds
    AABB get_bounds() const;

    // Returns index of original facet given an index of triangulated mesh's triangle
    Index proxy_to_orig_facet(Index i) const;

    // Returns triangle indices of a given polygon
    std::vector<Index> polygon_triangles(Index i) const;

    /// Mapping from original vertex to the first vertex in a triangulated and flattened array
    /// I.e., the first index of original vertex in the GPU buffer
    /// This is copied over to the GPU to use as POINTS primitive indices
    const std::vector<unsigned int>& get_vertex_to_vertex_mapping() const
    {
        return m_vertex_to_vertex_attrib_mapping;
    }

    /// Mapping from original edge to vertices in a triangulated and flattened array
    /// To get two vertices for edge E, use [2*E + 0] and [2*E + 1]
    /// This is copied over to the GPU to use as LINES primitive indices
    const std::vector<unsigned int>& get_edge_to_vertices() const
    {
        return m_edge_to_vertices;
    }


    /// Returns facet index of the original mesh at a ray intersection
    bool get_original_facet_at(
        Eigen::Vector3f origin,
        Eigen::Vector3f dir,
        int& facet_id_out,
        float& t_out,
        Eigen::Vector3f& barycentric_out) const;

    /// Returns triangle index of the proxy mesh at a ray intersection
    bool get_proxy_facet_at(
        Eigen::Vector3f origin,
        Eigen::Vector3f dir,
        int& facet_id_out,
        float& t_out,
        Eigen::Vector3f& barycentric_out) const;


    /// Returns indices of facets intersecting the frustum
    /// returns proxy's indices if proxy_indices = true,
    /// otherwise returns original mesh facet indices
    std::unordered_set<int> get_facets_in_frustum(
        const Frustum& f,
        bool ignore_backfacing,
        bool proxy_indices = false) const;

    /// Returns indices of vertices intersecting the frustum
    /// Indices are same for proxy and original mesh
    std::unordered_set<int> get_vertices_in_frustum(const Frustum& f, bool ignore_backfacing) const;

    /// Returns indices of edges intersecting the frustum
    /// Indices are from original mesh
    std::unordered_set<int> get_edges_in_frustum(const Frustum& f, bool ignore_backfacing) const;

    /// Does the frustum intersect the mesh
    bool intersects(const Frustum& f) const;

    /// Returns bounds of selected elements (AABB is in model space)
    AABB get_selection_bounds(const ElementSelection& sel) const;

    /// Original number of indices per facet
    Index original_facet_dimension() const { return m_orig_facet_dim; }

    /// Original dimension of vertices
    Index original_vertex_dimension() const { return m_orig_vertex_dim; }

    /// Original number of facets
    Index original_facet_num() const { return m_orig_facet_num; }

    /// Returns true if original mesh was not a triangle-only mesh
    Index is_triangulated() const { return m_triangulated; }

    /// Returns original Edge -> Index mapping
    const std::unordered_map<Edge, Index>& get_original_edge_index_map() const
    {
        return m_original_edge_index_map;
    }

    /// Returns original edges
    const std::vector<Edge>& get_original_edges() const { return m_original_edges; }


    template <typename MatrixType>
    MatrixType flatten_vertex_attribute(const MatrixType& data) const
    {
        LA_ASSERT(data.rows() == mesh().get_num_vertices());

        MatrixType flattened = MatrixType(mesh().get_num_facets() * 3, data.cols());

        auto& F = get_facets();
        for (auto fi = 0; fi < mesh().get_num_facets(); fi++) {
            flattened.row(3 * fi + 0) = data.row(F(fi, 0));
            flattened.row(3 * fi + 1) = data.row(F(fi, 1));
            flattened.row(3 * fi + 2) = data.row(F(fi, 2));
        }

        return flattened;
    }

    template <typename MatrixType>
    MatrixType flatten_facet_attribute(const MatrixType& data) const
    {
        LA_ASSERT(data.rows() == original_facet_num());

        MatrixType flattened = MatrixType(get_num_triangles() * 3, data.cols());

        // Use polygon->triangle map
        if (is_triangulated()) {
            // Assign the same attribute to all vertices of all triangles of the
            // original facet
            for (auto i = 0; i < data.rows(); i++) {
                auto triangles = polygon_triangles(int(i));
                for (auto t : triangles) {
                    flattened.row(3 * t + 0) = data.row(i);
                    flattened.row(3 * t + 1) = data.row(i);
                    flattened.row(3 * t + 2) = data.row(i);
                }
            }
        }
        // Otherwise use plain triangles
        else {
            // Assign same attribute to all three vertices of a triangle
            for (auto i = 0; i < data.rows(); i++) {
                flattened.row(3 * i + 0) = data.row(i);
                flattened.row(3 * i + 1) = data.row(i);
                flattened.row(3 * i + 2) = data.row(i);
            }
        }
        return flattened;
    }

    template <typename MatrixType>
    MatrixType flatten_edge_attribute(const MatrixType& data) const
    {
        LA_ASSERT(safe_cast<size_t>(data.rows()) == get_original_edges().size());

        MatrixType flattened = MatrixType(get_num_triangles() * 3, data.cols());

        const auto& edge_index_map = get_original_edge_index_map();

        for (auto& it : edge_index_map) {
            flattened.row(m_edge_to_vertices[it.second * 2]) = data.row(it.second);
        }

        return flattened;
    }

    template <typename MatrixType>
    MatrixType flatten_corner_attribute(const MatrixType& data) const
    {
        LA_ASSERT(data.rows() == m_orig_facet_num * original_facet_dimension());

        // If no triangulation was needed, return as is
        if (!is_triangulated()) {
            return data;
        }

        MatrixType flattened = MatrixType(get_num_triangles() * 3, data.cols());

        const auto orig_facet_dim = original_facet_dimension();

        for (auto fi = 0; fi < original_facet_num(); fi++) {
            auto triangles = polygon_triangles(int(fi));
            int fan_index = 0;
            for (auto t : triangles) {
                flattened.row(t * 3 + 0) = data.row(fi * orig_facet_dim + 0);
                flattened.row(t * 3 + 1) = data.row(fi * orig_facet_dim + fan_index + 1);
                flattened.row(t * 3 + 2) = data.row(fi * orig_facet_dim + fan_index + 2);
                fan_index++;
            }
        }

        return flattened;
    }

    const std::unordered_map<int, std::vector<int>>& get_material_indices() const
    {
        return m_material_indices;
    }


private:
    void init_acceleration();

    std::unique_ptr<TriangleMesh3Df> m_mesh;
    std::unique_ptr<ProxyMeshAccelImpl> m_accel_impl;

    Index m_orig_vertex_dim;
    Index m_orig_facet_dim;
    Index m_orig_facet_num;

    /// Prefix sum of number of triangles for each of the original facets
    std::vector<Index> m_triangulation_prefix_sum;

    /// Mapping from newly triangulated triangles to old facets
    std::vector<Index> m_new_triangle_to_orig;

    /// Mapping between vertex index and its first vertex attrib in
    /// the triangulated and flattened array
    std::vector<unsigned int> m_vertex_to_vertex_attrib_mapping;


    /// Mapping between original edge index and two vertex attributes in
    /// the triangulated and flattened array
    std::vector<unsigned int> m_edge_to_vertices;

    std::unordered_map<Edge, Index> m_original_edge_index_map;
    std::vector<Edge> m_original_edges;

    // Material ID -> triangle indices
    std::unordered_map<int, std::vector<int>> m_material_indices;

    bool m_triangulated;
};

} // namespace ui
} // namespace lagrange
