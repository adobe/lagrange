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

#include <lagrange/chain_corners_around_edges.h>
#include <lagrange/chain_corners_around_vertices.h>
#include <lagrange/corner_to_edge_mapping.h>

#include <Eigen/Core>

namespace lagrange {

///
/// This class is used to navigate elements of a mesh. By chaining facet corners around vertices and
/// edges, this class is able to provide efficient iteration over incident facets of a vertex/edge,
/// as well as detect boundary edges/vertices.
///
/// @tparam     MeshType  Mesh type.
///
template <typename MeshType>
class MeshNavigation
{
public:
    using Scalar = typename MeshType::Scalar;
    using Index = typename MeshType::Index;
    using FacetArray = typename MeshType::FacetArray;
    using IndexArray = Eigen::Matrix<Index, Eigen::Dynamic, 1>;

public:
    MeshNavigation(const MeshType& mesh) { initialize(mesh); }

public:
    ///
    /// Gets the number of edges.
    ///
    /// @return     The number of edges.
    ///
    Index get_num_edges() const { return safe_cast<Index>(m_e2c.size()); }

    ///
    /// Gets the edge index corresponding to (f, lv) -- (f, lv+1).
    ///
    /// @param[in]  f     Facet index.
    /// @param[in]  lv    Local vertex index [0, get_vertex_per_facet()[.
    ///
    /// @return     The edge.
    ///
    Index get_edge(Index f, Index lv) const { return m_c2e(f * m_vertex_per_facet + lv); }

    ///
    /// Gets the edge index corresponding to a corner index. Given a face (v0, v1, v2) with
    /// associated corners (c0, c1, c2), the edge associated to corner ci is the edge between (vi,
    /// vi+1), as determined by the corner_to_edge_mapping function.
    ///
    /// @param[in]  c     Corner index.
    ///
    /// @return     The edge.
    ///
    Index get_edge_from_corner(Index c) const { return m_c2e(c); }

    ///
    /// Get the index of the first corner around a given edge.
    ///
    /// @param[in]  e     Queried edge index.
    ///
    /// @return     Index of the first corner around the queried edge.
    ///
    Index get_first_corner_around_edge(Index e) const { return m_e2c[e]; }

    ///
    /// Gets the next corner around the edge associated to a corner. If the corner is the last one
    /// in the chain, this function returns INVALID<Index>.
    ///
    /// @param[in]  c     Corner index.
    ///
    /// @return     Next corner around the edge.
    ///
    Index get_next_corner_around_edge(Index c) const { return m_next_corner_around_edge[c]; }

    ///
    /// Get the index of the first corner around a given vertex.
    ///
    /// @param[in]  v     Queried vertex index.
    ///
    /// @return     Index of the first corner around the queried vertex.
    ///
    Index get_first_corner_around_vertex(Index v) const { return m_v2c[v]; }

    ///
    /// Gets the next corner around the vertex associated to a corner. If the corner is the last one
    /// in the chain, this function returns INVALID<Index>.
    ///
    /// @param[in]  c     Corner index.
    ///
    /// @return     Next corner around the vertex.
    ///
    Index get_next_corner_around_vertex(Index c) const { return m_next_corner_around_vertex[c]; }

    ///
    /// Retrieve edge endpoints.
    ///
    /// @param[in]  facets  #F x k array of facet indices.
    /// @param[in]  e       Queried edge index.
    ///
    /// @return     Array of vertex ids at the edge endpoints.
    ///
    std::array<Index, 2> get_edge_vertices(const FacetArray& facets, Index e) const
    {
        Index c = m_e2c[e];
        if (c == INVALID<Index>()) {
            logger().error("Invalid corner id for edge {}", e);
            throw std::runtime_error("Invalid edge id");
        }
        Index f = c / m_vertex_per_facet;
        Index lv = c % m_vertex_per_facet;
        return {{facets(f, lv), facets(f, (lv + 1) % m_vertex_per_facet)}};
    }

    ///
    /// Returns a vertex id opposite the edge. If the edge is a boundary edge, there is only one
    /// incident facet f, and the returned vertex will the vertex id opposite e on facet f.
    /// Otherwise, the returned vertex will be a vertex opposite e on a arbitrary incident facet f.
    ///
    /// @param[in]  facets  #F x k array of facet indices.
    /// @param[in]  e       Queried edge index.
    ///
    /// @return     Vertex id.
    ///
    Index get_vertex_opposite_edge(const FacetArray& facets, Index e) const
    {
        la_runtime_assert(m_vertex_per_facet == 3, "This method is only for triangle meshes.");
        Index c = m_e2c[e];
        if (c == INVALID<Index>()) {
            logger().error("Invalid corner id for edge {}", e);
            throw std::runtime_error("Invalid edge id");
        }
        Index f = c / m_vertex_per_facet;
        Index lv = c % m_vertex_per_facet;
        return facets(f, (lv + 2) % m_vertex_per_facet);
    }

    /// Count the number of facets incident to a given vertex.
    ///
    /// @param[in]  v     Queried vertex index.
    ///
    /// @return     Number of facets incident to the queried vertex.
    ///
    Index get_num_facets_around_vertex(Index v) const
    {
        // Count number of incident facets
        Index num_incident_facets = 0;
        foreach_facets_around_vertex(v, [&](Index /*f*/) { ++num_incident_facets; });
        return num_incident_facets;
    }

    /// Count the number of facets incident to a given edge.
    ///
    /// @param[in]  e     Queried edge index.
    ///
    /// @return     Number of facets incident to the queried edge.
    ///
    Index get_num_facets_around_edge(Index e) const
    {
        // Count number of incident facets
        Index num_incident_facets = 0;
        foreach_facets_around_edge(e, [&](Index /*f*/) { ++num_incident_facets; });
        return num_incident_facets;
    }

    ///
    /// Get the index of one facet around a given edge.
    ///
    /// @param[in]  e     Queried edge index.
    ///
    /// @return     Face index of one facet incident to the queried edge.
    ///
    Index get_one_facet_around_edge(Index e) const
    {
        Index c = m_e2c[e];
        if (c != INVALID<Index>()) {
            return c / m_vertex_per_facet;
        } else {
            return INVALID<Index>();
        }
    }

    ///
    /// Get the index of one corner around a given edge.
    ///
    /// @note       While this is technically redundant with get_first_corner_around_edge, the
    ///             latter can be used when iterating manually over a chain of corners, while this
    ///             method can be used to retrieve a single corner around a given edge.
    ///
    /// @param[in]  e     Queried edge index.
    ///
    /// @return     Index of the first corner around the queried edge.
    ///
    Index get_one_corner_around_edge(Index e) const { return m_e2c[e]; }

    ///
    /// Get the index of one corner around a given vertex.
    ///
    /// @note       While this is technically redundant with get_first_corner_around_vertex, the
    ///             latter can be used when iterating manually over a chain of corners, while this
    ///             method can be used to retrieve a single corner around a given vertex.
    ///
    /// @param[in]  v     Queried vertex index.
    ///
    /// @return     Index of the first corner around the queried vertex.
    ///
    Index get_one_corner_around_vertex(Index v) const { return m_v2c[v]; }

    ///
    /// Determines whether the specified edge e is a boundary edge.
    ///
    /// @param[in]  e     Queried edge index.
    ///
    /// @return     True if the specified edge e is a boundary edge, False otherwise.
    ///
    bool is_boundary_edge(Index e) const
    {
        Index c = m_e2c[e];
        assert(c != INVALID<Index>());
        return (m_next_corner_around_edge[c] == INVALID<Index>());
    }

    ///
    /// Determines whether the specified vertex v is a boundary vertex.
    ///
    /// @param[in]  v     Queried vertex index.
    ///
    /// @return     True if the specified vertex v is a boundary vertex, False otherwise.
    ///
    bool is_boundary_vertex(Index v) const { return m_is_boundary_vertex[v]; }

public:
    ///
    /// Applies a function to each facet around a prescribed vertex.
    ///
    /// @param[in]  v     Queried vertex index.
    /// @param[in]  func  Callback to apply to each incident facet.
    ///
    /// @tparam     Func  A callable function of type Index -> void.
    ///
    template <typename Func>
    void foreach_facets_around_vertex(Index v, Func func) const
    {
        // Loop over incident facets
        for (Index c = m_v2c[v]; c != INVALID<Index>(); c = m_next_corner_around_vertex[c]) {
            Index f = c / m_vertex_per_facet;
            func(f);
        }
    }

    ///
    /// Applies a function to each facet around a prescribed edge.
    ///
    /// @param[in]  e     Queried edge index.
    /// @param[in]  func  Callback to apply to each incident facet.
    ///
    /// @tparam     Func  A callable function of type Index -> void.
    ///
    template <typename Func>
    void foreach_facets_around_edge(Index e, Func func) const
    {
        // Loop over incident facets
        for (Index c = m_e2c[e]; c != INVALID<Index>(); c = m_next_corner_around_edge[c]) {
            Index f = c / m_vertex_per_facet;
            func(f);
        }
    }

    ///
    /// Applies a function to each corner around a prescribed vertex.
    ///
    /// @param[in]  v     Queried vertex index.
    /// @param[in]  func  Callback to apply to each incident facet.
    ///
    /// @tparam     Func  A callable function of type Index -> void.
    ///
    template <typename Func>
    void foreach_corners_around_vertex(Index v, Func func) const
    {
        // Loop over incident facets
        for (Index c = m_v2c[v]; c != INVALID<Index>(); c = m_next_corner_around_vertex[c]) {
            func(c);
        }
    }

    ///
    /// Applies a function to each corner around a prescribed edge.
    ///
    /// @param[in]  e     Queried edge index.
    /// @param[in]  func  Callback to apply to each incident facet.
    ///
    /// @tparam     Func  A callable function of type Index -> void.
    ///
    template <typename Func>
    void foreach_corners_around_edge(Index e, Func func) const
    {
        // Loop over incident facets
        for (Index c = m_e2c[e]; c != INVALID<Index>(); c = m_next_corner_around_edge[c]) {
            func(c);
        }
    }

protected:
    void initialize(const MeshType& mesh)
    {
        // Assumed to be constant
        m_vertex_per_facet = mesh.get_vertex_per_facet();

        // Compute unique edge ids
        corner_to_edge_mapping(mesh.get_facets(), m_c2e);

        // Chain corners around vertices and edges
        chain_corners_around_edges(mesh.get_facets(), m_c2e, m_e2c, m_next_corner_around_edge);
        chain_corners_around_vertices(
            mesh.get_num_vertices(),
            mesh.get_facets(),
            m_v2c,
            m_next_corner_around_vertex);

        // Tag boundary vertices
        m_is_boundary_vertex.assign(mesh.get_num_vertices(), false);
        for (Index e = 0; e < get_num_edges(); ++e) {
            if (is_boundary_edge(e)) {
                auto v = get_edge_vertices(mesh.get_facets(), e);
                m_is_boundary_vertex[v[0]] = true;
                m_is_boundary_vertex[v[1]] = true;
            }
        }
    }

protected:
    Index m_vertex_per_facet; ///< Number of vertex per facet (assumed constant).

    IndexArray m_c2e; ///< Corner to edge mapping.
    IndexArray m_e2c; ///< Edge to first corner in the chain.
    IndexArray m_next_corner_around_edge; ///< Next corner in the chain around an edge.
    IndexArray m_v2c; ///< Vertex to first corner in the chain.
    IndexArray m_next_corner_around_vertex; ///< Next corner in the chain around a vertex.
    std::vector<bool> m_is_boundary_vertex;
};

} // namespace lagrange
