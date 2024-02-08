/*
 * Copyright 2016 Adobe. All rights reserved.
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

#include <lagrange/ActingMeshGeometry.h>
#include <lagrange/Attributes.h>
#include <lagrange/Components.h>
#include <lagrange/Connectivity.h>
#include <lagrange/Edge.h>
#include <lagrange/GenuineMeshGeometry.h>
#include <lagrange/MeshGeometry.h>
#include <lagrange/MeshNavigation.h>
#include <lagrange/MeshTopology.h>
#include <lagrange/common.h>
#include <lagrange/corner_to_edge_mapping.h>
#include <lagrange/experimental/AttributeManager.h>
#include <lagrange/experimental/IndexedAttributeManager.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/safe_cast.h>
#include <lagrange/utils/invalid.h>

#include <Eigen/Core>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace lagrange {

// TODO: Extend base class with non-templated methods for access to vertex/facet/attribute generic arrays!
class MeshBase
{
public:
    virtual ~MeshBase() = default;
};

template <typename _VertexArray, typename _FacetArray>
class Mesh : public MeshBase
{
public:
    using VertexArray = _VertexArray;
    using FacetArray = _FacetArray;
    using VertexType =
        Eigen::Matrix<typename VertexArray::Scalar, 1, VertexArray::ColsAtCompileTime>;
    using MeshType = Mesh<VertexArray, FacetArray>;

    using Scalar = typename VertexArray::Scalar;
    using Index = typename FacetArray::Scalar;

    using AttributeArray = Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;
    using IndexArray = FacetArray;
    using AttributeMesh = Mesh<AttributeArray, IndexArray>;
    using Geometry = MeshGeometry<VertexArray, FacetArray>;

    using AdjacencyList = typename Connectivity<Geometry>::AdjacencyList;
    using IndexList = typename Connectivity<Geometry>::IndexList;

    using Edge = EdgeType<Index>;

    using UVArray = AttributeArray;
    using UVIndices = IndexArray;
    using UVType = Eigen::Matrix<Scalar, 1, 2>;

public:
    /**
     * The default constructor only build a frame of the data structure with
     * null geometry and attributes.
     *
     * One could initialize everything using initialize() method.
     */
    Mesh()
    {
        m_connectivity = std::make_unique<Connectivity<Geometry>>();
        m_components = std::make_unique<Components<Geometry>>();
        m_topology = std::make_unique<MeshTopology<MeshType>>();
    }

    Mesh(std::shared_ptr<Geometry> geom)
        : m_geometry(geom)
    {
        init_attributes();
        m_connectivity = std::make_unique<Connectivity<Geometry>>();
        m_components = std::make_unique<Components<Geometry>>();
        m_topology = std::make_unique<MeshTopology<MeshType>>();
    }
    Mesh(const Mesh& other) = delete;
    void operator=(const Mesh& other) = delete;
    virtual ~Mesh() = default;

public:
    void initialize(const VertexArray& vertices, const FacetArray& facets)
    {
        m_geometry =
            std::make_shared<GenuineMeshGeometry<VertexArray, FacetArray>>(vertices, facets);
        init_attributes();
    }

    bool is_initialized() const
    {
        return m_geometry && m_vertex_attributes && m_facet_attributes && m_corner_attributes &&
               m_edge_attributes && m_indexed_attributes;
    }

    Index get_dim() const
    {
        la_runtime_assert(is_initialized());
        return m_geometry->get_dim();
    }
    Index get_num_vertices() const
    {
        la_runtime_assert(is_initialized());
        return m_geometry->get_num_vertices();
    }
    Index get_num_facets() const
    {
        la_runtime_assert(is_initialized());
        return m_geometry->get_num_facets();
    }
    Index get_vertex_per_facet() const
    {
        la_runtime_assert(is_initialized());
        return m_geometry->get_vertex_per_facet();
    }

    const VertexArray& get_vertices() const
    {
        la_runtime_assert(is_initialized());
        return m_geometry->get_vertices();
    }
    const FacetArray& get_facets() const
    {
        la_runtime_assert(is_initialized());
        return m_geometry->get_facets();
    }

    VertexArray& ref_vertices()
    {
        la_runtime_assert(is_initialized());
        return m_geometry->get_vertices_ref();
    }
    FacetArray& ref_facets()
    {
        la_runtime_assert(is_initialized());
        return m_geometry->get_facets_ref();
    }

    std::vector<std::string> get_vertex_attribute_names() const
    {
        la_runtime_assert(is_initialized());
        return m_vertex_attributes->get_names();
    }

    std::vector<std::string> get_facet_attribute_names() const
    {
        la_runtime_assert(is_initialized());
        return m_facet_attributes->get_names();
    }

    std::vector<std::string> get_corner_attribute_names() const
    {
        la_runtime_assert(is_initialized());
        return m_corner_attributes->get_names();
    }

    std::vector<std::string> get_edge_attribute_names() const
    {
        la_runtime_assert(is_initialized());
        return m_edge_attributes->get_names();
    }

    std::vector<std::string> get_indexed_attribute_names() const
    {
        la_runtime_assert(is_initialized());
        return m_indexed_attributes->get_names();
    }

    bool has_vertex_attribute(const std::string& name) const
    {
        la_runtime_assert(is_initialized());
        return m_vertex_attributes->has(name);
    }

    bool has_facet_attribute(const std::string& name) const
    {
        la_runtime_assert(is_initialized());
        return m_facet_attributes->has(name);
    }

    bool has_corner_attribute(const std::string& name) const
    {
        la_runtime_assert(is_initialized());
        return m_corner_attributes->has(name);
    }

    bool has_edge_attribute(const std::string& name) const
    {
        la_runtime_assert(is_initialized());
        return m_edge_attributes->has(name);
    }

    bool has_indexed_attribute(const std::string& name) const
    {
        la_runtime_assert(is_initialized());
        return m_indexed_attributes->has(name);
    }

    void add_vertex_attribute(const std::string& name) const
    {
        la_runtime_assert(is_initialized());
        m_vertex_attributes->add(name);
    }

    void add_facet_attribute(const std::string& name) const
    {
        la_runtime_assert(is_initialized());
        m_facet_attributes->add(name);
    }

    void add_corner_attribute(const std::string& name) const
    {
        la_runtime_assert(is_initialized());
        m_corner_attributes->add(name);
    }

    void add_edge_attribute(const std::string& name) const
    {
        la_runtime_assert(is_initialized() && is_edge_data_initialized());
        m_edge_attributes->add(name);
    }

    void add_indexed_attribute(const std::string& name) const
    {
        la_runtime_assert(is_initialized());
        m_indexed_attributes->add(name);
    }

    const AttributeArray& get_vertex_attribute(const std::string& name) const
    {
        la_runtime_assert(is_initialized());
        const auto* attr = m_vertex_attributes->get(name);
        la_runtime_assert(attr != nullptr, "Attribute " + name + " is not initialized.");
        return attr->get()->template get<AttributeArray>();
    }

    decltype(auto) get_vertex_attribute_array(const std::string& name) const
    {
        la_runtime_assert(is_initialized());
        const auto* attr = m_vertex_attributes->get(name);
        la_runtime_assert(attr != nullptr, "Attribute " + name + " is not initialized.");
        return attr->get();
    }

    decltype(auto) get_vertex_attribute_array(const std::string& name)
    {
        la_runtime_assert(is_initialized());
        auto* attr = m_vertex_attributes->get(name);
        la_runtime_assert(attr != nullptr, "Attribute " + name + " is not initialized.");
        return attr->get();
    }


    const AttributeArray& get_facet_attribute(const std::string& name) const
    {
        la_runtime_assert(is_initialized());
        const auto* attr = m_facet_attributes->get(name);
        la_runtime_assert(attr != nullptr, "Attribute " + name + " is not initialized.");
        return attr->get()->template get<AttributeArray>();
    }

    decltype(auto) get_facet_attribute_array(const std::string& name) const
    {
        la_runtime_assert(is_initialized());
        const auto* attr = m_facet_attributes->get(name);
        la_runtime_assert(attr != nullptr, "Attribute " + name + " is not initialized.");
        return attr->get();
    }

    decltype(auto) get_facet_attribute_array(const std::string& name)
    {
        la_runtime_assert(is_initialized());
        auto* attr = m_facet_attributes->get(name);
        la_runtime_assert(attr != nullptr, "Attribute " + name + " is not initialized.");
        return attr->get();
    }

    const AttributeArray& get_corner_attribute(const std::string& name) const
    {
        la_runtime_assert(is_initialized());
        const auto* attr = m_corner_attributes->get(name);
        la_runtime_assert(attr != nullptr, "Attribute " + name + " is not initialized.");
        return attr->get()->template get<AttributeArray>();
    }

    decltype(auto) get_corner_attribute_array(const std::string& name) const
    {
        la_runtime_assert(is_initialized());
        const auto* attr = m_corner_attributes->get(name);
        la_runtime_assert(attr != nullptr, "Attribute " + name + " is not initialized.");
        return attr->get();
    }

    decltype(auto) get_corner_attribute_array(const std::string& name)
    {
        la_runtime_assert(is_initialized());
        auto* attr = m_corner_attributes->get(name);
        la_runtime_assert(attr != nullptr, "Attribute " + name + " is not initialized.");
        return attr->get();
    }

    const AttributeArray& get_edge_attribute(const std::string& name) const
    {
        la_runtime_assert(is_initialized() && is_edge_data_initialized());
        const auto* attr = m_edge_attributes->get(name);
        la_runtime_assert(attr != nullptr);
        return attr->get()->template get<AttributeArray>();
    }

    decltype(auto) get_edge_attribute_array(const std::string& name) const
    {
        la_runtime_assert(is_initialized() && is_edge_data_initialized());
        auto* attr = m_edge_attributes->get(name);
        la_runtime_assert(attr != nullptr, "Attribute " + name + " is not initialized.");
        return attr->get();
    }

    decltype(auto) get_edge_attribute_array(const std::string& name)
    {
        la_runtime_assert(is_initialized() && is_edge_data_initialized());
        auto* attr = m_edge_attributes->get(name);
        la_runtime_assert(attr != nullptr, "Attribute " + name + " is not initialized.");
        return attr->get();
    }

    auto get_indexed_attribute(const std::string& name) const
    {
        la_runtime_assert(is_initialized());
        const auto data = m_indexed_attributes->get(name);
        la_runtime_assert(data != nullptr);
        return std::forward_as_tuple(
            data->template get_values<AttributeArray>(),
            data->template get_indices<IndexArray>());
    }

    auto get_indexed_attribute_array(const std::string& name) const
    {
        la_runtime_assert(is_initialized());
        const auto data = m_indexed_attributes->get(name);
        la_runtime_assert(data != nullptr);
        return std::make_tuple(data->get_values(), data->get_indices());
    }

    auto get_indexed_attribute_array(const std::string& name)
    {
        la_runtime_assert(is_initialized());
        const auto data = m_indexed_attributes->get(name);
        la_runtime_assert(data != nullptr);
        return std::make_tuple(data->get_values(), data->get_indices());
    }

    void set_vertex_attribute(const std::string& name, const AttributeArray& attr)
    {
        la_runtime_assert(is_initialized());
        m_vertex_attributes->set(name, attr);
    }

    void set_facet_attribute(const std::string& name, const AttributeArray& attr)
    {
        la_runtime_assert(is_initialized());
        m_facet_attributes->set(name, attr);
    }

    void set_corner_attribute(const std::string& name, const AttributeArray& attr)
    {
        la_runtime_assert(is_initialized());
        m_corner_attributes->set(name, attr);
    }

    void set_edge_attribute(const std::string& name, const AttributeArray& attr)
    {
        la_runtime_assert(is_initialized() && is_edge_data_initialized());
        m_edge_attributes->set(name, attr);
    }

    template <typename Derived>
    void set_vertex_attribute_array(const std::string& name, Derived&& attr)
    {
        la_runtime_assert(is_initialized());
        m_vertex_attributes->set(name, std::forward<Derived>(attr));
    }

    template <typename Derived>
    void set_facet_attribute_array(const std::string& name, Derived&& attr)
    {
        la_runtime_assert(is_initialized());
        m_facet_attributes->set(name, std::forward<Derived>(attr));
    }

    template <typename Derived>
    void set_corner_attribute_array(const std::string& name, Derived&& attr)
    {
        la_runtime_assert(is_initialized());
        m_corner_attributes->set(name, std::forward<Derived>(attr));
    }

    template <typename Derived>
    void set_edge_attribute_array(const std::string& name, Derived&& attr)
    {
        la_runtime_assert(is_initialized() && is_edge_data_initialized());
        m_edge_attributes->set(name, std::forward<Derived>(attr));
    }

    void set_indexed_attribute(
        const std::string& name,
        const AttributeArray& values,
        const IndexArray& indices)
    {
        la_runtime_assert(is_initialized());
        auto attr = m_indexed_attributes->get(name);
        attr->set_values(values);
        attr->set_indices(indices);
    }

    void remove_vertex_attribute(const std::string& name)
    {
        la_runtime_assert(is_initialized());
        m_vertex_attributes->remove(name);
    }

    void remove_facet_attribute(const std::string& name)
    {
        la_runtime_assert(is_initialized());
        m_facet_attributes->remove(name);
    }

    void remove_corner_attribute(const std::string& name)
    {
        la_runtime_assert(is_initialized());
        m_corner_attributes->remove(name);
    }

    void remove_edge_attribute(const std::string& name)
    {
        la_runtime_assert(is_initialized() && is_edge_data_initialized());
        m_edge_attributes->remove(name);
    }

    void remove_indexed_attribute(const std::string& name)
    {
        la_runtime_assert(is_initialized());
        m_indexed_attributes->remove(name);
    }

public:
    //========================
    // Import/export functions
    //========================
    //
    // Data are moved from source varibles to target varaibles.
    // Once the data is moved out, the source variable is resized to have 0
    // rows.

    template <typename Derived>
    void import_vertices(Eigen::PlainObjectBase<Derived>& vertices)
    {
        la_runtime_assert(is_initialized());
        m_geometry->import_vertices(vertices);
    }

    template <typename Derived>
    void import_facets(Eigen::PlainObjectBase<Derived>& facets)
    {
        la_runtime_assert(is_initialized());
        m_geometry->import_facets(facets);
    }

    template <typename AttributeDerived>
    void import_vertex_attribute(const std::string& name, AttributeDerived&& attr) const
    {
        la_runtime_assert(is_initialized());
        m_vertex_attributes->import_data(name, std::forward<AttributeDerived>(attr));
    }

    template <typename AttributeDerived>
    void import_facet_attribute(const std::string& name, AttributeDerived&& attr) const
    {
        la_runtime_assert(is_initialized());
        m_facet_attributes->import_data(name, std::forward<AttributeDerived>(attr));
    }

    template <typename AttributeDerived>
    void import_corner_attribute(const std::string& name, AttributeDerived&& attr) const
    {
        la_runtime_assert(is_initialized());
        m_corner_attributes->import_data(name, std::forward<AttributeDerived>(attr));
    }

    template <typename AttributeDerived>
    void import_edge_attribute(const std::string& name, AttributeDerived&& attr) const
    {
        la_runtime_assert(is_initialized() && is_edge_data_initialized());
        m_edge_attributes->import_data(name, std::forward<AttributeDerived>(attr));
    }

    template <typename ValueDerived, typename IndexDerived>
    void
    import_indexed_attribute(const std::string& name, ValueDerived&& values, IndexDerived&& indices)
    {
        la_runtime_assert(is_initialized());
        m_indexed_attributes->import_data(
            name,
            std::forward<ValueDerived>(values),
            std::forward<IndexDerived>(indices));
    }

    template <typename Derived>
    void export_vertices(Eigen::PlainObjectBase<Derived>& vertices)
    {
        la_runtime_assert(is_initialized());
        m_geometry->export_vertices(vertices);
    }

    template <typename Derived>
    void export_facets(Eigen::PlainObjectBase<Derived>& facets)
    {
        la_runtime_assert(is_initialized());
        m_geometry->export_facets(facets);
    }

    template <typename Derived>
    void export_vertex_attribute(const std::string& name, Eigen::PlainObjectBase<Derived>& attr)
    {
        la_runtime_assert(is_initialized());
        m_vertex_attributes->export_data(name, attr);
    }

    template <typename Derived>
    void export_facet_attribute(const std::string& name, Eigen::PlainObjectBase<Derived>& attr)
    {
        la_runtime_assert(is_initialized());
        m_facet_attributes->export_data(name, attr);
    }

    template <typename Derived>
    void export_corner_attribute(const std::string& name, Eigen::PlainObjectBase<Derived>& attr)
    {
        la_runtime_assert(is_initialized());
        m_corner_attributes->export_data(name, attr);
    }

    template <typename Derived>
    void export_edge_attribute(const std::string& name, Eigen::PlainObjectBase<Derived>& attr)
    {
        la_runtime_assert(is_initialized() && is_edge_data_initialized());
        m_edge_attributes->export_data(name, attr);
    }

    template <typename ValueDerived, typename IndexDerived>
    void export_indexed_attribute(
        const std::string& name,
        Eigen::PlainObjectBase<ValueDerived>& values,
        Eigen::PlainObjectBase<IndexDerived>& indices)
    {
        m_indexed_attributes->export_data(name, values, indices);
    }

public:
    //========================
    // Connecitivity functions
    //========================

    void initialize_connectivity()
    {
        la_runtime_assert(m_connectivity);
        m_connectivity->initialize(*m_geometry);
    }

    bool is_connectivity_initialized() const
    {
        return (m_connectivity && m_connectivity->is_initialized());
    }

    const AdjacencyList& get_vertex_vertex_adjacency() const
    {
        la_runtime_assert(is_connectivity_initialized());
        return m_connectivity->get_vertex_vertex_adjacency();
    }

    const AdjacencyList& get_vertex_facet_adjacency() const
    {
        la_runtime_assert(is_connectivity_initialized());
        return m_connectivity->get_vertex_facet_adjacency();
    }

    const AdjacencyList& get_facet_facet_adjacency() const
    {
        la_runtime_assert(is_connectivity_initialized());
        return m_connectivity->get_facet_facet_adjacency();
    }

    const IndexList& get_vertices_adjacent_to_vertex(Index vi) const
    {
        la_runtime_assert(is_connectivity_initialized());
        return m_connectivity->get_vertices_adjacent_to_vertex(vi);
    }

    const IndexList& get_facets_adjacent_to_vertex(Index vi) const
    {
        la_runtime_assert(is_connectivity_initialized());
        return m_connectivity->get_facets_adjacent_to_vertex(vi);
    }

    const IndexList& get_facets_adjacent_to_facet(Index fi) const
    {
        la_runtime_assert(is_connectivity_initialized());
        return m_connectivity->get_facets_adjacent_to_facet(fi);
    }

public:
    // ========================
    // Edge-facet map functions.
    // ========================

    /// Edge data initialization
    void initialize_edge_data()
    {
        if (m_navigation) return;
        m_navigation = std::make_unique<MeshNavigation<MeshType>>(std::ref(*this));
    }

    /// Clear edge data
    void clear_edge_data() { m_navigation = nullptr; }

    /// Edge data accessors (const)
    bool is_edge_data_initialized() const { return m_navigation != nullptr; }

    ///
    /// Gets the number of edges.
    ///
    /// @return     The number of edges.
    ///
    Index get_num_edges() const
    {
        la_runtime_assert(is_edge_data_initialized(), "Edge data not initialized");
        return m_navigation->get_num_edges();
    }

    ///
    /// Gets the edge index corresponding to (f, lv) -- (f, lv+1).
    ///
    /// @param[in]  f     Facet index.
    /// @param[in]  lv    Local vertex index [0, get_vertex_per_facet()[.
    ///
    /// @return     The edge.
    ///
    Index get_edge(Index f, Index lv) const
    {
        la_runtime_assert(is_edge_data_initialized(), "Edge data not initialized");
        return m_navigation->get_edge(f, lv);
    }

    ///
    /// Gets the edge index corresponding to a corner index.
    ///
    /// @param[in]  c     Corner index.
    ///
    /// @return     The edge.
    ///
    Index get_edge_from_corner(Index c) const
    {
        la_runtime_assert(is_edge_data_initialized(), "Edge data not initialized");
        return m_navigation->get_edge_from_corner(c);
    }

    ///
    /// Retrieve the edge id from end vertices (v0, v1).
    ///
    /// @todo Move this to MeshNavigation?
    ///
    /// @param[in]  v0  Edge edge point 0.
    /// @param[in]  v1  Edge edge point 1.
    ///
    /// @return  The edge index if (v0, v1) is a valid edge;
    ///          invalid<Index>() otherwise.
    ///
    Index find_edge_from_vertices(Index v0, Index v1) const
    {
        la_runtime_assert(is_edge_data_initialized(), "Edge data not initialized");
        Index edge_id = invalid<Index>();

        const Index vertex_per_facet = get_vertex_per_facet();
        foreach_corners_around_vertex(v0, [&](Index c) {
            const Index fid = c / vertex_per_facet;
            const Index lv = c % vertex_per_facet;
            if (get_facets()(fid, (lv + 1) % vertex_per_facet) == v1) {
                edge_id = get_edge(fid, lv);
            }
        });
        if (edge_id != invalid<Index>()) return edge_id;

        // It is possible that (v0, v1) is a boundary edge, in which case
        // the above code will only find the edge_id if (v0, v1) is oriented
        // correctly (i.e. counterclockwise).  Thus, we need to check the
        // opposite orientation if the above code failed.
        foreach_corners_around_vertex(v1, [&](Index c) {
            const Index fid = c / vertex_per_facet;
            const Index lv = c % vertex_per_facet;
            if (get_facets()(fid, (lv + 1) % vertex_per_facet) == v0) {
                edge_id = get_edge(fid, lv);
            }
        });
        return edge_id;
    }

    ///
    /// Retrieve edge endpoints.
    ///
    /// @param[in]  e     Queried edge index.
    ///
    /// @return     Array of vertex ids at the edge endpoints.
    ///
    std::array<Index, 2> get_edge_vertices(Index e) const
    {
        la_runtime_assert(is_edge_data_initialized(), "Edge data not initialized");
        return m_navigation->get_edge_vertices(get_facets(), e);
    }

    ///
    /// Returns a vertex id opposite the edge. If the edge is a boundary edge, there is only one
    /// incident facet f, and the returned vertex will the vertex id opposite e on facet f.
    /// Otherwise, the returned vertex will be a vertex opposite e on a arbitrary incident facet f.
    ///
    /// @param[in]  e     Queried edge index.
    ///
    /// @return     Vertex id.
    ///
    Index get_vertex_opposite_edge(Index e) const
    {
        la_runtime_assert(is_edge_data_initialized(), "Edge data not initialized");
        return m_navigation->get_vertex_opposite_edge(get_facets(), e);
    }

    /// Count the number of facets incident to a given vertex.
    ///
    /// @param[in]  v     Queried vertex index.
    ///
    /// @return     Number of facets incident to the queried vertex.
    ///
    Index get_num_facets_around_vertex(Index v) const
    {
        la_runtime_assert(is_edge_data_initialized(), "Edge data not initialized");
        return m_navigation->get_num_facets_around_vertex(v);
    }

    /// Count the number of facets incident to a given edge.
    ///
    /// @param[in]  e     Queried edge index.
    ///
    /// @return     Number of facets incident to the queried edge.
    ///
    Index get_num_facets_around_edge(Index e) const
    {
        la_runtime_assert(is_edge_data_initialized(), "Edge data not initialized");
        return m_navigation->get_num_facets_around_edge(e);
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
        la_runtime_assert(is_edge_data_initialized(), "Edge data not initialized");
        return m_navigation->get_one_facet_around_edge(e);
    }

    ///
    /// Get the index of one corner around a given edge.
    ///
    /// @param[in]  e     Queried edge index.
    ///
    /// @return     Index of the first corner around the queried edge.
    ///
    Index get_one_corner_around_edge(Index e) const
    {
        la_runtime_assert(is_edge_data_initialized(), "Edge data not initialized");
        return m_navigation->get_one_corner_around_edge(e);
    }

    ///
    /// Get the index of one corner around a given vertex.
    ///
    /// @param[in]  v     Queried vertex index.
    ///
    /// @return     Index of the first corner around the queried vertex.
    ///
    Index get_one_corner_around_vertex(Index v) const
    {
        la_runtime_assert(is_edge_data_initialized(), "Edge data not initialized");
        return m_navigation->get_one_corner_around_vertex(v);
    }

    ///
    /// Determines whether the specified edge e is a boundary edge.
    ///
    /// @param[in]  e     Queried edge index.
    ///
    /// @return     True if the specified edge e is a boundary edge, False otherwise.
    ///
    bool is_boundary_edge(Index e) const
    {
        la_runtime_assert(is_edge_data_initialized(), "Edge data not initialized");
        return m_navigation->is_boundary_edge(e);
    }

    ///
    /// Determines whether the specified vertex v is a boundary vertex.
    ///
    /// @param[in]  v     Queried vertex index.
    ///
    /// @return     True if the specified vertex v is a boundary vertex, False otherwise.
    ///
    bool is_boundary_vertex(Index v) const
    {
        la_runtime_assert(is_edge_data_initialized(), "Edge data not initialized");
        return m_navigation->is_boundary_vertex(v);
    }

    // ==== edge data utility functions ====

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
        la_runtime_assert(is_edge_data_initialized(), "Edge data not initialized");
        m_navigation->foreach_facets_around_vertex(v, func);
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
        la_runtime_assert(is_edge_data_initialized(), "Edge data not initialized");
        m_navigation->foreach_facets_around_edge(e, func);
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
        la_runtime_assert(is_edge_data_initialized(), "Edge data not initialized");
        m_navigation->foreach_corners_around_vertex(v, func);
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
        la_runtime_assert(is_edge_data_initialized(), "Edge data not initialized");
        m_navigation->foreach_corners_around_edge(e, func);
    }

public:
    //========================
    // Topology functions
    //========================

    void initialize_topology()
    {
        la_runtime_assert(m_topology);
        m_topology->initialize(*this);
    }

    bool is_topology_initialized() const { return m_topology->is_initialized(); }

    bool is_edge_manifold() const
    {
        la_runtime_assert(m_topology->is_initialized(), "Mesh topology not initialized");
        return m_topology->is_edge_manifold();
    }

    bool is_vertex_manifold() const
    {
        la_runtime_assert(m_topology->is_initialized(), "Mesh topology not initialized");
        return m_topology->is_vertex_manifold();
    }

    const MeshTopology<MeshType>& get_topology() const { return *m_topology; }

public:
    //=====================
    // Components functions
    //=====================
    void initialize_components()
    {
        la_runtime_assert(m_components);
        la_runtime_assert(m_connectivity);
        if (!m_connectivity->is_initialized()) {
            initialize_connectivity();
        }
        m_components->initialize(*m_geometry, m_connectivity.get());
    }

    bool is_components_initialized() const
    {
        return (m_components && (get_num_facets() == 0 || m_components->get_num_components() > 0));
    }

    Index get_num_components() const
    {
        la_runtime_assert(is_components_initialized());
        return m_components->get_num_components();
    }

    const std::vector<IndexList>& get_components() const
    {
        la_runtime_assert(is_components_initialized());
        return m_components->get_components();
    }

    const IndexList& get_per_facet_component_ids() const
    {
        la_runtime_assert(is_components_initialized());
        return m_components->get_per_facet_component_ids();
    }

public:
    //========================
    // UV coordinates.
    //========================

    bool is_uv_initialized() const { return m_indexed_attributes->has("uv"); }

    void initialize_uv(const UVArray& uv, const UVIndices& uv_indices)
    {
        if (!m_indexed_attributes->has("uv")) {
            m_indexed_attributes->add("uv");
        }
        auto uv_attr = m_indexed_attributes->get("uv");

        uv_attr->set_values(uv);
        uv_attr->set_indices(uv_indices);
    }

    void import_uv(UVArray&& uv, UVIndices&& uv_indices)
    {
        if (!m_indexed_attributes->has("uv")) {
            m_indexed_attributes->add("uv");
        }
        this->import_indexed_attribute("uv", uv, uv_indices);
    }

    decltype(auto) get_uv() const
    {
        la_runtime_assert(is_uv_initialized());
        return m_indexed_attributes->get_values<UVArray>("uv");
    }

    decltype(auto) get_uv_indices() const
    {
        la_runtime_assert(is_uv_initialized());
        return m_indexed_attributes->get_indices<UVIndices>("uv");
    }

    decltype(auto) get_uv_mesh() const
    {
        const auto attr = m_indexed_attributes->get("uv");
        auto geometry = std::make_unique<GenuineMeshGeometry<UVArray, UVIndices>>(
            attr->get_values()->template get<UVArray>(),
            attr->get_indices()->template get<UVIndices>());
        return std::make_unique<Mesh<UVArray, UVIndices>>(std::move(geometry));
    }

    void clear_uv() const { m_indexed_attributes->remove("uv"); }

public:
    //========================
    // Serialization.
    //========================

    template <typename Archive>
    void serialize_impl(Archive& ar)
    {
        LA_IGNORE_SHADOW_WARNING_BEGIN
        const std::array<int32_t, 3> current_version = {{0, 1, 0}};
        std::array<int32_t, 3> version = current_version;
        if (ar.is_input()) {
            init_attributes();
        }
        enum {
            VERSION = 0,
            GEOMETRY = 1,
            VERTEX_ATTR = 2,
            FACET_ATTR = 3,
            CORNER_ATTR = 4,
            EDGE_ATTR = 5,
            INDEXED_ATTR = 6,
            DEPRECATED_EDGE_ATTR = 7, // Not used anymore, but we keep the slot occupied
        };
        ar.object([&](auto& ar) {
            ar("version", VERSION) & version;
            ar("geometry", GEOMETRY) & m_geometry;
            ar("vertex_attributes", VERTEX_ATTR) & m_vertex_attributes;
            ar("facet_attributes", FACET_ATTR) & m_facet_attributes;
            ar("corner_attributes", CORNER_ATTR) & m_corner_attributes;
            ar("edge_attributes", EDGE_ATTR) & m_edge_attributes;
            ar("indexed_attributes", INDEXED_ATTR) & m_indexed_attributes;
        });
        la_runtime_assert(version == current_version, "Incompatible version number");
        LA_IGNORE_SHADOW_WARNING_END

        // If mesh has edge attributes, we need to initialize edge data too
        if (ar.is_input()) {
            if (m_edge_attributes->get_size() > 0) {
                initialize_edge_data();
            }
        }

        // Hack/workaround until we have a stable API for mesh attributes. Currently `IndexArray`
        // can have a fixed num of columns at compile-time, but array serialization always save/load
        // arrays with Eigen::Dynamic as the number of rows/cols (as it should).
        if (ar.is_input()) {
            const auto names = m_indexed_attributes->get_names();
            for (const auto& name : names) {
                AttributeArray values = m_indexed_attributes->view_values<AttributeArray>(name);
                IndexArray indices = m_indexed_attributes->view_indices<IndexArray>(name);
                m_indexed_attributes->remove(name);
                m_indexed_attributes->add(name, std::move(values), std::move(indices));
            }
        }
    }

protected:
    void init_attributes()
    {
        m_vertex_attributes = std::make_unique<experimental::AttributeManager>();
        m_facet_attributes = std::make_unique<experimental::AttributeManager>();
        m_corner_attributes = std::make_unique<experimental::AttributeManager>();
        m_edge_attributes = std::make_unique<experimental::AttributeManager>();
        m_indexed_attributes = std::make_unique<experimental::IndexedAttributeManager>();
    }

protected:
    std::shared_ptr<Geometry> m_geometry;
    std::unique_ptr<MeshTopology<MeshType>> m_topology;
    std::unique_ptr<MeshNavigation<MeshType>> m_navigation;

    std::unique_ptr<Connectivity<Geometry>> m_connectivity;
    std::unique_ptr<Components<Geometry>> m_components;
    std::unique_ptr<experimental::AttributeManager> m_vertex_attributes;
    std::unique_ptr<experimental::AttributeManager> m_facet_attributes;
    std::unique_ptr<experimental::AttributeManager> m_corner_attributes;
    std::unique_ptr<experimental::AttributeManager> m_edge_attributes;
    std::unique_ptr<experimental::IndexedAttributeManager> m_indexed_attributes;
};

template <typename _VertexArray, typename _FacetArray, typename Archive>
void serialize(Mesh<_VertexArray, _FacetArray>& mesh, Archive& ar)
{
    mesh.serialize_impl(ar);
}

extern template class Mesh<Vertices3D, Triangles>;
extern template class Mesh<Vertices2D, Triangles>;
extern template class Mesh<Vertices3Df, Triangles>;
extern template class Mesh<Vertices2Df, Triangles>;
extern template class Mesh<Vertices3D, Quads>;
extern template class Mesh<Vertices2D, Quads>;
extern template class Mesh<Vertices3Df, Quads>;
extern template class Mesh<Vertices2Df, Quads>;

} // namespace lagrange
