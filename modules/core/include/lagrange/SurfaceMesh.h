/*
 * Copyright 2022 Adobe. All rights reserved.
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

#include <lagrange/AttributeFwd.h>
#include <lagrange/utils/SharedSpan.h>
#include <lagrange/utils/function_ref.h>
#include <lagrange/utils/span.h>
#include <lagrange/utils/value_ptr.h>

#include <initializer_list>
#include <string_view>

namespace lagrange {


/// @defgroup group-surfacemesh SurfaceMesh
/// @ingroup module-core
/// Generic surface meshes.
/// @{

///
/// A general purpose polygonal mesh class.
///
/// @tparam     Scalar_  %Mesh scalar type.
/// @tparam     Index_  %Mesh index type.
///
template <typename Scalar_, typename Index_>
class SurfaceMesh
{
public:
    /// %Mesh scalar type, used for vertex coordinates.
    using Scalar = Scalar_;

    /// %Mesh index type, used for facet indices.
    using Index = Index_;

    /// Signed index type corresponding to the mesh index type.
    using SignedIndex = std::make_signed_t<Index>;

public:
    ///
    /// Callback function to set vertex coordinates.
    ///
    /// @param[in]  v     Index of the vertex to set coordinates for (relative to the newly inserted
    ///                   vertices, i.e. starts at 0).
    /// @param[out] p     Output coordinate buffer to write to. The output array will contain K
    ///                   elements to write to, where K is the dimension of the mesh (3 by default).
    ///
    /// @see        add_vertices
    ///
    using SetVertexCoordinatesFunction = function_ref<void(Index v, span<Scalar> p)>;

    ///
    /// Callback function to set indices of a single facet. The facet size is fixed and known in
    /// advance by the user.
    ///
    /// @param[out] t     Output index buffer to write to.
    ///
    /// @see        add_polygon
    ///
    using SetSingleFacetIndicesFunction = function_ref<void(span<Index> t)>;

    ///
    /// Callback function to set indices of a multiple facets.
    ///
    /// @param[in]  f     Index of the facet whose size to compute (relative to the newly inserted
    ///                   facets, starting with 0).
    /// @param[out] t     Output index buffer to write to. You can query the size of the current
    ///                   output facet by calling t.size().
    ///
    /// @see        add_triangles, add_quads, add_polygons, add_hybrid
    ///
    using SetMultiFacetsIndicesFunction = function_ref<void(Index f, span<Index> t)>;

    ///
    /// Callback function to get a facet size (number of vertices in the facet).
    ///
    /// @param[in]  f     Index of the facet whose size to compute (relative to the newly inserted
    ///                   facets, starting with 0).
    ///
    /// @see        add_hybrid
    ///
    using GetFacetsSizeFunction = function_ref<Index(Index f)>;

    ///
    /// Callback function to get the vertex indices of an edge endpoints in a user-provided ordering
    /// of a mesh edges.
    ///
    /// @param[in]  e     Index of the edge being queried.
    ///
    /// @return     A pair of indices for the vertex endpoints.
    ///
    using GetEdgeVertices = function_ref<std::array<Index, 2>(Index e)>;

public:
    ///
    /// @name Mesh construction
    /// @{

    ///
    /// Default constructor.
    ///
    /// @param[in]  dimension  Vertex dimension.
    ///
    explicit SurfaceMesh(Index dimension = 3);

    ///
    /// Default destructor.
    ///
    ~SurfaceMesh();

    ///
    /// Move constructor.
    ///
    /// @param      other  Instance to move from.
    ///
    SurfaceMesh(SurfaceMesh&& other) noexcept;

    ///
    /// Assignment move operator.
    ///
    /// @param      other  Instance to move from.
    ///
    /// @return     The result of the assignment.
    ///
    SurfaceMesh& operator=(SurfaceMesh&& other) noexcept;

    ///
    /// Copy constructor.
    ///
    /// @param[in]  other  Instance to copy from.
    ///
    SurfaceMesh(const SurfaceMesh& other);

    ///
    /// Assignment copy operator.
    ///
    /// @param[in]  other  Instance to copy from.
    ///
    /// @return     The result of the assignment.
    ///
    SurfaceMesh& operator=(const SurfaceMesh& other);

    ///
    /// Adds a vertex to the mesh.
    ///
    /// @param[in]  p     Vertex coordinates with the same dimensionality as the mesh.
    ///
    void add_vertex(span<const Scalar> p);

    ///
    /// Adds a vertex to the mesh.
    ///
    /// @param[in]  p     Vertex coordinates with the same dimensionality as the mesh.
    ///
    void add_vertex(std::initializer_list<const Scalar> p);

    ///
    /// Adds multiple vertices to the mesh.
    ///
    /// @param[in]  num_vertices  Number of vertices to add.
    /// @param[in]  coordinates   A contiguous array of point coordinates (num_vertices x
    ///                           dimension).
    ///
    void add_vertices(Index num_vertices, span<const Scalar> coordinates = {});

    ///
    /// Adds multiple vertices to the mesh.
    ///
    /// @param[in]  num_vertices            Number of vertices to add.
    /// @param[in]  set_vertex_coordinates  Function to set point coordinates of a given vertex.
    ///
    void add_vertices(Index num_vertices, SetVertexCoordinatesFunction set_vertex_coordinates);

    ///
    /// Adds a triangular facet to the mesh.
    ///
    /// @param[in]  v0    Index of the first vertex.
    /// @param[in]  v1    Index of the second vertex.
    /// @param[in]  v2    Index of the third vertex.
    ///
    void add_triangle(Index v0, Index v1, Index v2);

    ///
    /// Adds multiple triangular facets to the mesh.
    ///
    /// @warning    If the mesh contains edge/connectivity attributes, this function will throw an
    ///             exception if you pass an empty buffer of facet indices. This is because it is
    ///             impossible to update edge/connectivity information if the facet buffer is
    ///             directly modified by the user. Instead, the correct facet indices must be
    ///             provided when the facet is constructed.
    ///
    /// @param[in]  num_facets     Number of facets to add.
    /// @param[in]  facet_indices  A contiguous array of corner indices, where facet_indices[3*i+k]
    ///                            is the index of the k-th corner of the i-th facet.
    ///
    void add_triangles(Index num_facets, span<const Index> facet_indices = {});

    ///
    /// Adds multiple triangular facets to the mesh.
    ///
    /// @param[in]  num_facets          Number of facets to add.
    /// @param[in]  set_facets_indices  Callable function to set vertex indices of a given facet.
    ///
    void add_triangles(Index num_facets, SetMultiFacetsIndicesFunction set_facets_indices);

    ///
    /// Adds a quadrilateral facet to the mesh.
    ///
    /// @param[in]  v0    Index of the first vertex.
    /// @param[in]  v1    Index of the second vertex.
    /// @param[in]  v2    Index of the third vertex.
    /// @param[in]  v3    Index of the fourth vertex.
    ///
    void add_quad(Index v0, Index v1, Index v2, Index v3);

    ///
    /// Adds multiple quadrilateral facets to the mesh.
    ///
    /// @warning    If the mesh contains edge/connectivity attributes, this function will throw an
    ///             exception if you pass an empty buffer of facet indices. This is because it is
    ///             impossible to update edge/connectivity information if the facet buffer is
    ///             directly modified by the user. Instead, the correct facet indices must be
    ///             provided when the facet is constructed.
    ///
    /// @param[in]  num_facets     Number of facets to add.
    /// @param[in]  facet_indices  A contiguous array of corner indices, where facet_indices[4*i+k]
    ///                            is the index of the k-th corner of the i-th facet.
    ///
    void add_quads(Index num_facets, span<const Index> facet_indices = {});

    ///
    /// Adds multiple quadrilateral facets to the mesh.
    ///
    /// @param[in]  num_facets          Number of facets to add.
    /// @param[in]  set_facets_indices  Callable function to set vertex indices of a given facet.
    ///
    void add_quads(Index num_facets, SetMultiFacetsIndicesFunction set_facets_indices);

    ///
    /// Adds a single (uninitialized) polygonal facet to the mesh. Facet indices are set to 0.
    ///
    /// @param[in]  facet_size  Number of vertices in the facet.
    ///
    void add_polygon(Index facet_size);

    ///
    /// Adds a single polygonal facet to the mesh.
    ///
    /// @param[in]  facet_indices  A contiguous array of vertex indices in the facet.
    ///
    void add_polygon(span<const Index> facet_indices);

    ///
    /// Adds a single polygonal facet to the mesh.
    ///
    /// @param[in]  facet_indices  A contiguous array of vertex indices in the facet.
    ///
    void add_polygon(std::initializer_list<const Index> facet_indices);

    ///
    /// Adds a single polygonal facet to the mesh.
    ///
    /// @param[in]  facet_size         Number of vertices in the facet.
    /// @param[in]  set_facet_indices  Callable function to retrieve facet indices.
    ///
    void add_polygon(Index facet_size, SetSingleFacetIndicesFunction set_facet_indices);

    ///
    /// Adds multiple polygonal facets of the same size to the mesh.
    ///
    /// @warning    If the mesh contains edge/connectivity attributes, this function will throw an
    ///             exception if you pass an empty buffer of facet indices. This is because it is
    ///             impossible to update edge/connectivity information if the facet buffer is
    ///             directly modified by the user. Instead, the correct facet indices must be
    ///             provided when the facet is constructed.
    ///
    /// @param[in]  num_facets     Number of facets to add.
    /// @param[in]  facet_size     Size of each facet to be added.
    /// @param[in]  facet_indices  A contiguous array of corner indices, where
    ///                            facet_indices[facet_size*i+k] is the index of the k-th corner of
    ///                            the i-th facet.
    ///
    void add_polygons(Index num_facets, Index facet_size, span<const Index> facet_indices = {});

    ///
    /// Adds multiple polygonal facets of the same size to the mesh.
    ///
    /// @param[in]  num_facets          Number of facets to add.
    /// @param[in]  facet_size          Size of each facet to be added.
    /// @param[in]  set_facets_indices  Callable function to set vertex indices of a given facet.
    ///
    void add_polygons(
        Index num_facets,
        Index facet_size,
        SetMultiFacetsIndicesFunction set_facets_indices);

    ///
    /// Adds multiple polygonal facets of different sizes to the mesh.
    ///
    /// @warning    If the mesh contains edge/connectivity attributes, this function will throw an
    ///             exception if you pass an empty buffer of facet indices. This is because it is
    ///             impossible to update edge/connectivity information if the facet buffer is
    ///             directly modified by the user. Instead, the correct facet indices must be
    ///             provided when the facet is constructed.
    ///
    /// @param[in]  facet_sizes    A contiguous array representing the size of each facet to add.
    /// @param[in]  facet_indices  A contiguous array of corner indices, where
    ///                            facet_indices[sum(facet_sizes(j), j<=i) + k] is the index of the
    ///                            k-th corner of the i-th facet.
    ///
    void add_hybrid(span<const Index> facet_sizes, span<const Index> facet_indices = {});

    ///
    /// Adds multiple polygonal facets of different sizes to the mesh.
    ///
    /// @param[in]  num_facets          Number of facets to add.
    /// @param[in]  facet_sizes         Callable function to retrieve the size of each facet to be
    ///                                 added.
    /// @param[in]  set_facets_indices  Callable function to set vertex indices of a given facet.
    ///
    void add_hybrid(
        Index num_facets,
        GetFacetsSizeFunction facet_sizes,
        SetMultiFacetsIndicesFunction set_facets_indices);

    ///
    /// Removes a list of vertices. The set of vertices should be provided as a sorted list,
    /// otherwise an exception is raised.
    ///
    /// @note       This function will remove any facet incident to a removed vertex.
    ///
    /// @param[in]  vertices_to_remove  The vertices to remove.
    ///
    void remove_vertices(span<const Index> vertices_to_remove);

    ///
    /// Removes a list of vertices. The set of vertices should be provided as a sorted list,
    /// otherwise an exception is raised.
    ///
    /// @note       This function will remove any facet incident to a removed vertex.
    ///
    /// @param[in]  vertices_to_remove  The vertices to remove.
    ///
    void remove_vertices(std::initializer_list<const Index> vertices_to_remove);

    ///
    /// Removes a list of vertices, defined by a predicate function.
    ///
    /// @note       This function will remove any facet incident to a removed vertex.
    ///
    /// @param[in]  should_remove_func  Function to determine if a vertex of a particular index
    ///                                 should be removed.
    ///
    void remove_vertices(function_ref<bool(Index)> should_remove_func);

    ///
    /// Removes a list of facets. The set of facets should be provided as a sorted list, otherwise
    /// an exception is raised.
    ///
    /// @note       This function does _not_ remove any isolated vertex that occurs due to facet
    ///             removal.
    ///
    /// @param[in]  facets_to_remove  The facets to remove.
    ///
    void remove_facets(span<const Index> facets_to_remove);

    ///
    /// Removes a list of facets. The set of facets should be provided as a sorted list, otherwise
    /// an exception is raised.
    ///
    /// @note       This function does _not_ remove any isolated vertex that occurs due to facet
    ///             removal.
    ///
    /// @param[in]  facets_to_remove  The facets to remove.
    ///
    void remove_facets(std::initializer_list<const Index> facets_to_remove);

    ///
    /// Removes a list of facets, defined by a predicate function.
    ///
    /// @note       This function does _not_ remove any isolated vertex that occurs due to facet
    ///             removal.
    ///
    /// @param[in]  should_remove_func  Function to determine if a facet of a particular index
    ///                                 should be removed.
    ///
    void remove_facets(function_ref<bool(Index)> should_remove_func);

    ///
    /// Clear buffer for mesh vertices and other vertex attributes. Since this function also removes
    /// any invalid facet, the entire mesh will be cleared.
    ///
    void clear_vertices();

    ///
    /// Clear buffer for mesh facets and other facet/corner attributes. The resulting mesh will be a
    /// point cloud made of isolated vertices.
    ///
    void clear_facets();

    ///
    /// Shrink buffer capacities to fit current mesh attributes, deallocating any extra capacity.
    ///
    /// @todo       Not implemented yet.
    ///
    void shrink_to_fit();

    ///
    /// Compress mesh storage if the mesh is regular. This iterates over all facets to check if the
    /// mesh is regular. If so the following attributes are removed:
    ///
    /// - "$facet_to_first_corner"
    /// - "$corner_to_facet"
    ///
    /// @todo       Not implemented yet.
    ///
    void compress_if_regular();

public:
    /// @}
    /// @name Mesh accessors
    /// @{

    ///
    /// Whether the mesh *must* only contain triangular facets. A mesh with no facet is considered a
    /// triangle mesh.
    ///
    /// @note       A mesh with hybrid storage *may* still be a triangle mesh, which this method
    ///             does not check.
    ///
    /// @return     True if triangle mesh, False otherwise.
    ///
    [[nodiscard]] bool is_triangle_mesh() const;

    ///
    /// Whether the mesh *must* only contains quadrilateral facets. A mesh with no facet is
    /// considered a quad mesh.
    ///
    /// @note       A mesh with hybrid storage *may* still be a quad mesh, which this method
    ///             does not check.
    ///
    /// @return     True if quad mesh, False otherwise.
    ///
    [[nodiscard]] bool is_quad_mesh() const;

    ///
    /// Whether the mesh *must* only contains facets of equal sizes. A mesh with no facet is
    /// considered regular.
    ///
    /// @note       A mesh with hybrid storage *may* still be have regular facet sizes, which this
    ///             method does not check.
    ///
    /// @return     True if regular, False otherwise.
    ///
    [[nodiscard]] bool is_regular() const;

    ///
    /// Whether the mesh *may* contain facets of different sizes. This is the opposite of is_regular
    /// (an empty mesh is _not_ considered hybrid).
    ///
    /// @note       A mesh with hybrid storage *may* still have all its facet be the same size,
    ///             which this method does not check.
    ///
    /// @return     True if hybrid, False otherwise.
    ///
    [[nodiscard]] bool is_hybrid() const { return !is_regular(); }

    ///
    /// Retrieves the dimension of the mesh vertices.
    ///
    /// @return     The mesh dimension.
    ///
    [[nodiscard]] Index get_dimension() const { return m_dimension; }

    ///
    /// Retrieves the number of vertex per facet in a regular mesh. If the mesh is a hybrid mesh, an
    /// exception is thrown.s
    ///
    /// @return     The number of vertices per facet.
    ///
    [[nodiscard]] Index get_vertex_per_facet() const;

    ///
    /// Retrieves the number of vertices.
    ///
    /// @return     The number of vertices.
    ///
    [[nodiscard]] Index get_num_vertices() const { return m_num_vertices; }

    ///
    /// Retrieves the number of facets.
    ///
    /// @return     The number of facets.
    ///
    [[nodiscard]] Index get_num_facets() const { return m_num_facets; }

    ///
    /// Retrieves the number of corners.
    ///
    /// @return     The number of corners.
    ///
    [[nodiscard]] Index get_num_corners() const { return m_num_corners; }

    ///
    /// Retrieves the number of edges.
    ///
    /// @return     The number of edges.
    ///
    [[nodiscard]] Index get_num_edges() const { return m_num_edges; }

    ///
    /// Retrieves a read-only pointer to a vertex coordinates.
    ///
    /// @param[in]  v     Vertex index.
    ///
    /// @return     Point coordinates of the queried vertex.
    ///
    [[nodiscard]] span<const Scalar> get_position(Index v) const;

    ///
    /// Retrieves a writeable pointer to a vertex coordinates.
    ///
    /// @param[in]  v     Vertex index.
    ///
    /// @return     Point coordinates of the queried vertex.
    ///
    [[nodiscard]] span<Scalar> ref_position(Index v);

    ///
    /// Number of vertices in the facet.
    ///
    /// @param[in]  f     Facet index.
    ///
    /// @return     Number of vertices in the queried facet.
    ///
    [[nodiscard]] Index get_facet_size(Index f) const
    {
        return get_facet_corner_end(f) - get_facet_corner_begin(f);
    }

    ///
    /// Index of a vertex given from a facet + local index.
    ///
    /// @param[in]  f     Facet index.
    /// @param[in]  lv    Local vertex index inside the facet.
    ///
    /// @return     Index of the vertex in the mesh.
    ///
    [[nodiscard]] Index get_facet_vertex(Index f, Index lv) const
    {
        return get_corner_vertex(get_facet_corner_begin(f) + lv);
    }

    ///
    /// First corner around the facet.
    ///
    /// @param[in]  f     Facet index.
    ///
    /// @return     First corner index for the queried facet.
    ///
    [[nodiscard]] Index get_facet_corner_begin(Index f) const;

    ///
    /// Index past the last corner around the facet.
    ///
    /// @param[in]  f     Facet index.
    ///
    /// @return     Index past the last corner index for the queried facet.
    ///
    [[nodiscard]] Index get_facet_corner_end(Index f) const;

    ///
    /// Retrieves the index of a vertex given its corner index. E.g. this can be used in conjunction
    /// with get_facet_corner_begin and get_facet_corner_end to iterate over the vertices of a
    /// facet.
    ///
    /// @param[in]  c     Corner index.
    ///
    /// @return     Index of the vertex in the mesh.
    ///
    [[nodiscard]] Index get_corner_vertex(Index c) const;

    ///
    /// Retrieves the index of a facet given its corner index. If the mesh is regular, this is
    /// simply the corner index / num of vertex per facet. If the mesh is a hybrid polygonal mesh,
    /// this mapping is stored in a reserved attribute.s
    ///
    /// @param[in]  c     Corner index.
    ///
    /// @return     Index of the facet containing this corner.
    ///
    [[nodiscard]] Index get_corner_facet(Index c) const;

    ///
    /// Retrieves a read-only pointer to a facet indices.
    ///
    /// @param[in]  f     Facet index.
    ///
    /// @return     A pointer to the facet vertices.
    ///
    [[nodiscard]] span<const Index> get_facet_vertices(Index f) const;

    ///
    /// Retrieves a writable pointer to a facet indices.
    ///
    /// @warning    If the mesh contains edge/connectivity attributes, this function will throw an
    ///             exception. This is because it is impossible to update edge/connectivity
    ///             information if the facet buffer is directly modified by the user. Instead, the
    ///             correct facet indices must be provided when the facet is constructed.
    ///
    /// @param[in]  f     Facet index.
    ///
    /// @return     A pointer to the facet vertices.
    ///
    [[nodiscard]] span<Index> ref_facet_vertices(Index f);

public:
    /// @}
    /// @name Attribute construction
    /// @{

    ///
    /// Retrieve an attribute id given its name. If the attribute doesn't exist, invalid_attribute_id()
    /// is returned instead.
    ///
    /// @param[in]  name  %Attribute name.
    ///
    /// @return     The attribute identifier.
    ///
    [[nodiscard]] AttributeId get_attribute_id(std::string_view name) const;

    ///
    /// Retrieve attribute name from its id.
    ///
    /// @param[in]  id  %Attribute id.
    ///
    /// @return     %Attribute's name.
    ///
    /// @throws     Exception if id is invalid.
    ///
    [[nodiscard]] std::string_view get_attribute_name(AttributeId id) const;

    ///
    /// Create a new attribute and return the newly created attribute id. A mesh attribute is stored
    /// as a row-major R x C matrix. The number of rows (R) is determined by the number of elements
    /// in the mesh that the attribute is attached to. The number of columns (C) is determined by
    /// the user when the attribute is created (num_channels), and cannot be modified afterwards.
    /// Note that the attribute tag determines how many channels are acceptable for certain types of
    /// attributes.
    ///
    /// @param[in]  name             %Attribute name to create.
    /// @param[in]  element          %Mesh element to which the attribute is attached to (Vertex,
    ///                              Facet, etc.).
    /// @param[in]  num_channels     The number of channels for the attribute. Cannot be modified
    ///                              once the attribute has been created.
    /// @param[in]  usage            Tag to indicate how the values are modified under rigid
    ///                              transformation.
    /// @param[in]  initial_values   A span of initial values to populate the attribute values with.
    ///                              The data is copied into the attribute. If the span is provided,
    ///                              it must have the right dimension (number of elements x number
    ///                              of channels).
    /// @param[in]  initial_indices  A span of initial values to populate the attribute indices
    ///                              with. If the attribute element type is not Indexed, providing a
    ///                              non-empty value for this argument will result in a runtime
    ///                              error. The data is copied into the attribute. If the span is
    ///                              provided, it must have the right dimension (number of corners).
    /// @param[in]  policy           %Attribute creation policy. By default using a reserved
    ///                              attribute name (starting with a "$") will throw an exception.
    ///
    /// @tparam     ValueType        Value type for the attribute.
    ///
    /// @return     The attribute identifier.
    ///
    /// @see        AttributeUsage
    ///
    template <typename ValueType>
    AttributeId create_attribute(
        std::string_view name,
        AttributeElement element,
        size_t num_channels = 1,
        AttributeUsage usage = AttributeUsage::Vector,
        span<const ValueType> initial_values = {},
        span<const Index> initial_indices = {},
        AttributeCreatePolicy policy = AttributeCreatePolicy::ErrorIfReserved);

    ///
    /// @overload
    ///
    /// Create a new attribute and return the newly created attribute id. A mesh attribute is stored
    /// as a row-major R x C matrix. The number of rows (R) is determined by the number of elements
    /// in the mesh that the attribute is attached to. The number of columns (C) is determined by
    /// the user when the attribute is created (num_channels), and cannot be modified afterwards.
    /// Note that the attribute tag determines how many channels are acceptable for certain types of
    /// attributes.
    ///
    /// @param[in]  name             %Attribute name to create.
    /// @param[in]  element          %Mesh element to which the attribute is attached to (Vertex,
    ///                              Facet, etc.).
    /// @param[in]  usage            Tag to indicate how the values are modified under rigid
    ///                              transformation.
    /// @param[in]  num_channels     The number of channels for the attribute. Cannot be modified
    ///                              once the attribute has been created.
    /// @param[in]  initial_values   A span of initial values to populate the attribute values with.
    ///                              The data is copied into the attribute. If the span is provided,
    ///                              it must have the right dimension (number of elements x number
    ///                              of channels).
    /// @param[in]  initial_indices  A span of initial values to populate the attribute indices
    ///                              with. If the attribute element type is not Indexed, providing a
    ///                              non-empty value for this argument will result in a runtime
    ///                              error. The data is copied into the attribute. If the span is
    ///                              provided, it must have the right dimension (number of corners).
    /// @param[in]  policy           %Attribute creation policy. By default using a reserved
    ///                              attribute name (starting with a "$") will throw an exception.
    ///
    /// @tparam     ValueType        Value type for the attribute.
    ///
    /// @return     The attribute identifier.
    ///
    /// @see        AttributeUsage
    ///
    template <typename ValueType>
    AttributeId create_attribute(
        std::string_view name,
        AttributeElement element,
        AttributeUsage usage,
        size_t num_channels = 1,
        span<const ValueType> initial_values = {},
        span<const Index> initial_indices = {},
        AttributeCreatePolicy policy = AttributeCreatePolicy::ErrorIfReserved);

    ///
    /// Creates an new attribute by creating a shallow copy of another mesh's attribute. The mesh
    /// can reference itself. This only performs a shallow copy, sharing the underlying data buffer.
    /// If either meshes performs a write operation, a deep copy will be performed (the
    /// modifications are not shared). Note that if the number of elements differs between the
    /// source and target meshes, an exception is raised.
    ///
    /// @param[in]  name         %Attribute name to create.
    /// @param[in]  source_mesh  Source mesh from which to copy the attribute from. The mesh can
    ///                          reference itself.
    /// @param[in]  source_name  Attribute name to copy from. If left empty, the target attribute
    ///                          name will be used.
    ///
    /// @return     The attribute identifier.
    ///
    AttributeId create_attribute_from(
        std::string_view name,
        const SurfaceMesh& source_mesh,
        std::string_view source_name = {});

    ///
    /// Wraps a writable external buffer as a mesh attribute. The buffer must remain valid during
    /// the lifetime of the mesh object (and any derived meshes that might have been copied from
    /// it).
    ///
    /// @param[in]  name          %Attribute name to create.
    /// @param[in]  element       %Mesh element to which the attribute is attached to (Vertex,
    ///                           Facet, etc.). The element type must not be Indexed. Please use
    ///                           wrap_as_indexed_attribute for that.
    /// @param[in]  usage         Tag to indicate how the values are modified under rigid
    ///                           transformation.
    /// @param[in]  num_channels  The number of channels for the attribute. Cannot be modified once
    ///                           the attribute has been created.
    /// @param[in]  values_view   A span of the external buffer to use as storage for the attribute
    ///                           values. The provided span must have enough capacity to hold
    ///                           (number of elements x number of channels) items. It is ok to
    ///                           provide a span with a larger capacity than needed, which will
    ///                           allow for the mesh to grow.
    ///
    /// @tparam     ValueType     Value type for the attribute.
    ///
    /// @return     The attribute identifier.
    ///
    template <typename ValueType>
    AttributeId wrap_as_attribute(
        std::string_view name,
        AttributeElement element,
        AttributeUsage usage,
        size_t num_channels,
        span<ValueType> values_view);

    ///
    /// @overload
    ///
    /// @note
    /// This function differs from the prevous version by participating in
    /// shared ownership management of the buffer referred by `shared_values`.
    ///
    template <typename ValueType>
    AttributeId wrap_as_attribute(
        std::string_view name,
        AttributeElement element,
        AttributeUsage usage,
        size_t num_channels,
        SharedSpan<ValueType> shared_values);

    ///
    /// Wraps a read-only external buffer as a mesh attribute. The buffer must remain valid during
    /// the lifetime of the mesh object (and any derived meshes that might have been copied from
    /// it). Any operation that attempts to write data to the attribute will throw a runtime
    /// exception.
    ///
    /// @param[in]  name          %Attribute name to create.
    /// @param[in]  element       %Mesh element to which the attribute is attached to (Vertex,
    ///                           Facet, etc.). The element type must not be Indexed. Please use
    ///                           wrap_as_indexed_attribute for that.
    /// @param[in]  usage         Tag to indicate how the values are modified under rigid
    ///                           transformation.
    /// @param[in]  num_channels  The number of channels for the attribute. Cannot be modified once
    ///                           the attribute has been created.
    /// @param[in]  values_view   A span of the external buffer to use as storage for the attribute
    ///                           values. The provided span must have a size >= (number of elements
    ///                           x number of channels) items. It is ok to provide a span with a
    ///                           larger capacity than needed.
    ///
    /// @tparam     ValueType     Value type for the attribute.
    ///
    /// @return     The attribute identifier.
    ///
    template <typename ValueType>
    AttributeId wrap_as_const_attribute(
        std::string_view name,
        AttributeElement element,
        AttributeUsage usage,
        size_t num_channels,
        span<const ValueType> values_view);

    ///
    /// @overload
    ///
    /// @note
    /// This function differs from the prevous version by participating in
    /// shared ownership management of the buffer referred by `shared_values`.
    ///
    template <typename ValueType>
    AttributeId wrap_as_const_attribute(
        std::string_view name,
        AttributeElement element,
        AttributeUsage usage,
        size_t num_channels,
        SharedSpan<const ValueType> shared_values);

    ///
    /// Wraps a writable external buffer as a mesh attribute. The buffer must remain valid during
    /// the lifetime of the mesh object (and any derived meshes that might have been copied from
    /// it).
    ///
    /// @param[in]  name          %Attribute name to create.
    /// @param[in]  usage         Tag to indicate how the values are modified under rigid
    ///                           transformation.
    /// @param[in]  num_values    Initial number of rows in the value buffer for the attribute.
    /// @param[in]  num_channels  The number of channels for the attribute. Cannot be modified once
    ///                           the attribute has been created.
    /// @param[in]  values_view   A span of the external buffer to use as storage for the attribute
    ///                           values. The provided span must have enough capacity to hold
    ///                           (number of values x number of channels) items. It is ok to provide
    ///                           a span with a larger capacity than needed, which will allow for
    ///                           the attribute to grow.
    /// @param[in]  indices_view  A span of the external buffer to use as storage for the attribute
    ///                           indices. The provided span must have enough capacity to hold
    ///                           (number of corners) items. It is ok to provide a span with a
    ///                           larger capacity than needed, which will allow for the attribute to
    ///                           grow.
    ///
    /// @tparam     ValueType     Value type for the attribute.
    ///
    /// @return     The attribute identifier.
    ///
    template <typename ValueType>
    AttributeId wrap_as_indexed_attribute(
        std::string_view name,
        AttributeUsage usage,
        size_t num_values,
        size_t num_channels,
        span<ValueType> values_view,
        span<Index> indices_view);

    ///
    /// @overload
    ///
    /// @note
    /// This function differs from the prevous version by participating in
    /// shared ownership management of the buffer referred by `shared_values` and
    /// `shared_indices`.
    ///
    template <typename ValueType>
    AttributeId wrap_as_indexed_attribute(
        std::string_view name,
        AttributeUsage usage,
        size_t num_values,
        size_t num_channels,
        SharedSpan<ValueType> shared_values,
        SharedSpan<Index> shared_indices);

    ///
    /// @overload
    ///
    /// @note
    /// This function differs from the prevous version by participating in
    /// shared ownership management of the buffer referred by `shared_indices`.
    ///
    template <typename ValueType>
    AttributeId wrap_as_indexed_attribute(
        std::string_view name,
        AttributeUsage usage,
        size_t num_values,
        size_t num_channels,
        span<ValueType> values_view,
        SharedSpan<Index> shared_indices);

    ///
    /// @overload
    ///
    /// @note
    /// This function differs from the prevous version by participating in
    /// shared ownership management of the buffer referred by `shared_values`.
    ///
    template <typename ValueType>
    AttributeId wrap_as_indexed_attribute(
        std::string_view name,
        AttributeUsage usage,
        size_t num_values,
        size_t num_channels,
        SharedSpan<ValueType> shared_values,
        span<Index> indices_view);

    ///
    /// Wraps a read-only external buffer as a mesh attribute. The buffer must remain valid during
    /// the lifetime of the mesh object (and any derived meshes that might have been copied from
    /// it). Any operation that attempts to write data to the attribute will throw a runtime
    /// exception.
    ///
    /// @param[in]  name          %Attribute name to create.
    /// @param[in]  usage         Tag to indicate how the values are modified under rigid
    ///                           transformation.
    /// @param[in]  num_values    Initial number of rows in the value buffer for the attribute.
    /// @param[in]  num_channels  The number of channels for the attribute. Cannot be modified once
    ///                           the attribute has been created.
    /// @param[in]  values_view   A span of the external buffer to use as storage for the attribute
    ///                           values. The provided span must have enough capacity to hold
    ///                           (number of values x number of channels) items. It is ok to provide
    ///                           a span with a larger capacity than needed, which will allow for
    ///                           the attribute to grow.
    /// @param[in]  indices_view  A span of the external buffer to use as storage for the attribute
    ///                           indices. The provided span must have enough capacity to hold
    ///                           (number of corners) items. It is ok to provide a span with a
    ///                           larger capacity than needed, which will allow for the attribute to
    ///                           grow.
    ///
    /// @tparam     ValueType     Value type for the attribute.
    ///
    /// @return     The attribute identifier.
    ///
    template <typename ValueType>
    AttributeId wrap_as_const_indexed_attribute(
        std::string_view name,
        AttributeUsage usage,
        size_t num_values,
        size_t num_channels,
        span<const ValueType> values_view,
        span<const Index> indices_view);

    ///
    /// @overload
    ///
    /// @note
    /// This function differs from the prevous version by participating in
    /// shared ownership management of the buffers referred by `shared_values` and
    /// `shared_indices`.
    ///
    template <typename ValueType>
    AttributeId wrap_as_const_indexed_attribute(
        std::string_view name,
        AttributeUsage usage,
        size_t num_values,
        size_t num_channels,
        SharedSpan<const ValueType> shared_values,
        SharedSpan<const Index> shared_indices);

    ///
    /// @overload
    ///
    /// @note
    /// This function differs from the prevous version by participating in
    /// shared ownership management of the buffer referred by `shared_indices`.
    ///
    template <typename ValueType>
    AttributeId wrap_as_const_indexed_attribute(
        std::string_view name,
        AttributeUsage usage,
        size_t num_values,
        size_t num_channels,
        span<const ValueType> values_view,
        SharedSpan<const Index> shared_indices);

    ///
    /// @overload
    ///
    /// @note
    /// This function differs from the prevous version by participating in
    /// shared ownership management of the buffer referred by `shared_values`.
    ///
    template <typename ValueType>
    AttributeId wrap_as_const_indexed_attribute(
        std::string_view name,
        AttributeUsage usage,
        size_t num_values,
        size_t num_channels,
        SharedSpan<const ValueType> shared_values,
        span<const Index> indices_view);

    ///
    /// Wraps a writable external buffer as mesh vertices coordinates. The buffer must remain valid
    /// during the lifetime of the mesh object (and any derived meshes that might have been copied
    /// from it). The user must provide the new number of vertices to resize the mesh with.
    ///
    /// @note       The mesh dimension cannot be changed, and is determined by the mesh constructor.
    ///
    /// @param[in]  vertices_view  A span of the external buffer to use as storage for mesh
    ///                            vertices. The provided span must have a size >= (num vertices x
    ///                            mesh dimension). It is ok to provide a span larger than needed.
    /// @param[in]  num_vertices   Number of vertices to resize the mesh with.
    ///
    /// @return     The attribute identifier.
    ///
    AttributeId wrap_as_vertices(span<Scalar> vertices_view, Index num_vertices);

    ///
    /// @overload
    ///
    /// @note
    /// This function differs from the prevous version by participating in
    /// shared ownership management of the buffer referred by `shared_vertices`.
    ///
    AttributeId wrap_as_vertices(SharedSpan<Scalar> shared_vertices, Index num_vertices);

    ///
    /// Wraps a read-only external buffer as mesh vertices coordinates. The buffer must remain valid
    /// during the lifetime of the mesh object (and any derived meshes that might have been copied
    /// from it). The user must provide the new number of vertices to resize the mesh with. Any
    /// operation that attempts to write to mesh vertices will throw a runtime exception.
    ///
    /// @note       The mesh dimension cannot be changed, and is determined by the mesh constructor.
    ///
    /// @param[in]  vertices_view  A span of the external buffer to use as storage for mesh
    ///                            vertices. The provided span must have a size >= (num vertices x
    ///                            mesh dimension). It is ok to provide a span larger than needed.
    /// @param[in]  num_vertices   Number of vertices to resize the mesh with.
    ///
    /// @return     The attribute identifier.
    ///
    AttributeId wrap_as_const_vertices(span<const Scalar> vertices_view, Index num_vertices);

    ///
    /// @overload
    ///
    /// @note
    /// This function differs from the prevous version by participating in
    /// shared ownership management of the buffer referred by `shared_vertices`.
    ///
    AttributeId wrap_as_const_vertices(
        SharedSpan<const Scalar> shared_vertices,
        Index num_vertices);

    ///
    /// Wraps a writable external buffer as mesh facets for a regular mesh. The buffer must remain
    /// valid during the lifetime of the mesh object (and any derived meshes that might have been
    /// copied from it). The user must provide the new number of facets to resize the mesh with.
    ///
    /// @param[in]  facets_view       A span of the external buffer to use as storage for mesh facet
    ///                               indices. The provided span must have a size >= (num_facets x
    ///                               vertex_per_facet). It is ok to provide a span larger than
    ///                               needed.
    /// @param[in]  num_facets        Number of facets to resize the mesh with.
    /// @param[in]  vertex_per_facet  Number of vertices per facet in the provided span.
    ///
    /// @return     The attribute identifier.
    ///
    AttributeId wrap_as_facets(span<Index> facets_view, Index num_facets, Index vertex_per_facet);

    ///
    /// @overload
    ///
    /// @note
    /// This function differs from the prevous version by participating in
    /// shared ownership management of the buffer referred by `shared_facets`.
    ///
    AttributeId
    wrap_as_facets(SharedSpan<Index> shared_facets, Index num_facets, Index vertex_per_facet);

    ///
    /// Wraps a read-only external buffer as mesh facets for a regular mesh. The buffer must remain
    /// valid during the lifetime of the mesh object (and any derived meshes that might have been
    /// copied from it). The user must provide the new number of facets to resize the mesh with. Any
    /// operation that attempts to write to mesh facets will throw a runtime exception.
    ///
    /// @param[in]  facets_view       A span of the external buffer to use as storage for mesh facet
    ///                               indices. The provided span must have a size >= (num_facets x
    ///                               vertex_per_facet). It is ok to provide a span larger than
    ///                               needed.
    /// @param[in]  num_facets        Number of facets to resize the mesh with.
    /// @param[in]  vertex_per_facet  Number of vertices per facet in the provided span.
    ///
    /// @return     The attribute identifier.
    ///
    AttributeId
    wrap_as_const_facets(span<const Index> facets_view, Index num_facets, Index vertex_per_facet);

    ///
    /// @overload
    ///
    /// @note
    /// This function differs from the prevous version by participating in
    /// shared ownership management of the buffer referred by `shared_facets`.
    ///
    AttributeId wrap_as_const_facets(
        SharedSpan<const Index> shared_facets,
        Index num_facets,
        Index vertex_per_facet);

    ///
    /// Wraps writable external buffers as mesh facets for a hybrid mesh. The buffer must remain
    /// valid during the lifetime of the mesh object (and any derived meshes that might have been
    /// copied from it). The user must provide the new number of facets to resize the mesh with.
    ///
    /// @param[in]  offsets_view  A span of the external buffer to use as storage for mesh offset
    ///                           indices (facet -> first corner index). The provided span must have
    ///                           a size >= num_facets. It is ok to provide a span larger than
    ///                           needed.
    /// @param[in]  num_facets    Number of facets to resize the mesh with.
    /// @param[in]  facets_view   A span of the external buffer to use as storage for mesh facet
    ///                           indices (corner -> vertex index). The provided span must have a
    ///                           size >= num_corners. It is ok to provide a span larger than
    ///                           needed.
    /// @param[in]  num_corners   Total number of facet corners in the resized mesh.
    ///
    /// @return     The attribute identifier.
    ///
    AttributeId wrap_as_facets(
        span<Index> offsets_view,
        Index num_facets,
        span<Index> facets_view,
        Index num_corners);

    ///
    /// @overload
    ///
    /// @note
    /// This function differs from the prevous version by participating in
    /// shared ownership management of the buffers referred by `shared_offsets` and
    /// `shared_facets`.
    ///
    AttributeId wrap_as_facets(
        SharedSpan<Index> shared_offsets,
        Index num_facets,
        SharedSpan<Index> shared_facets,
        Index num_corners);

    ///
    /// @overload
    ///
    /// @note
    /// This function differs from the prevous version by participating in
    /// shared ownership management of the buffer referred by `shared_facets`.
    ///
    AttributeId wrap_as_facets(
        span<Index> offsets_view,
        Index num_facets,
        SharedSpan<Index> shared_facets,
        Index num_corners);

    ///
    /// @overload
    ///
    /// @note
    /// This function differs from the prevous version by participating in
    /// shared ownership management of the buffer referred by `shared_offsets`.
    ///
    AttributeId wrap_as_facets(
        SharedSpan<Index> shared_offsets,
        Index num_facets,
        span<Index> facets_view,
        Index num_corners);

    ///
    /// Wraps read-only external buffers as mesh facets for a hybrid mesh. The buffer must remain
    /// valid during the lifetime of the mesh object (and any derived meshes that might have been
    /// copied from it). The user must provide the new number of facets to resize the mesh with. Any
    /// operation that attempts to write to mesh facets will throw a runtime exception.
    ///
    /// @param[in]  offsets_view  A span of the external buffer to use as storage for mesh offset
    ///                           indices (facet -> first corner index). The provided span must have
    ///                           a size >= num_facets. It is ok to provide a span larger than
    ///                           needed.
    /// @param[in]  num_facets    Number of facets to resize the mesh with.
    /// @param[in]  facets_view   A span of the external buffer to use as storage for mesh facet
    ///                           indices (corner -> vertex index). The provided span must have a
    ///                           size >= num_corners. It is ok to provide a span larger than
    ///                           needed.
    /// @param[in]  num_corners   Total number of facet corners in the resized mesh.
    ///
    /// @return     The attribute identifier.
    ///
    AttributeId wrap_as_const_facets(
        span<const Index> offsets_view,
        Index num_facets,
        span<const Index> facets_view,
        Index num_corners);

    ///
    /// @overload
    ///
    /// @note
    /// This function differs from the prevous version by participating in
    /// shared ownership management of the buffers referred by `shared_offsets` and
    /// `shared_facets`.
    ///
    AttributeId wrap_as_const_facets(
        SharedSpan<const Index> shared_offsets,
        Index num_facets,
        SharedSpan<const Index> shared_facets,
        Index num_corners);

    ///
    /// @overload
    ///
    /// @note
    /// This function differs from the prevous version by participating in
    /// shared ownership management of the buffer referred by `shared_facets`.
    ///
    AttributeId wrap_as_const_facets(
        span<const Index> offsets_view,
        Index num_facets,
        SharedSpan<const Index> shared_facets,
        Index num_corners);

    ///
    /// @overload
    ///
    /// @note
    /// This function differs from the prevous version by participating in
    /// shared ownership management of the buffer referred by `shared_offsets`.
    ///
    AttributeId wrap_as_const_facets(
        SharedSpan<const Index> shared_offsets,
        Index num_facets,
        span<const Index> facets_view,
        Index num_corners);

    ///
    /// Duplicates an attribute. This creates a shallow copy of the data, until a write operation
    /// occurs on either attribute. The new name must not belong to an existing attribute.
    ///
    /// @param[in]  old_name  Old attribute name to duplicate.
    /// @param[in]  new_name  New attribute name to create.
    ///
    /// @return     The attribute identifier for the new attribute.
    ///
    AttributeId duplicate_attribute(std::string_view old_name, std::string_view new_name);

    ///
    /// Rename an existing attribute. %Attribute id remains unchanged.
    ///
    /// @param[in]  old_name  Old attribute name.
    /// @param[in]  new_name  New attribute name.
    ///
    void rename_attribute(std::string_view old_name, std::string_view new_name);

    ///
    /// Delete an attribute given by name. The attribute must exist. If the attribute is a reserved
    /// name (starts with a "$"), then you must specify force=true to delete the attribute.
    ///
    /// @note       There is no performance benefit in deleting an attribute given by id only, as we
    ///             would need to free its name from the list of existing attributes anyway.
    ///
    /// @param[in]  name    %Attribute name.
    /// @param[in]  policy  Delete policy for reserved attribute names.
    ///
    void delete_attribute(
        std::string_view name,
        AttributeDeletePolicy policy = AttributeDeletePolicy::ErrorIfReserved);

    ///
    /// Delete an attribute and export its content in a writable shared_ptr. The attribute must
    /// exist.
    /// - If the content is managed by the mesh:
    ///   1. If it is uniquely owned, no copy is performed.
    ///   2. If ownership is shared, a copy is performed (just like any other copy-on-write
    ///      operation).
    /// - If the content points to an external buffer, a policy flag determines what happens.
    ///
    /// @param[in]  name           %Attribute name
    /// @param[in]  delete_policy  Delete policy for reserved attribute names.
    /// @param[in]  export_policy  Export policy for non-owned buffers.
    ///
    /// @tparam     ValueType      Value type for the attribute.
    ///
    /// @return     A shared pointer holding the attribute data.
    ///
    template <typename ValueType>
    std::shared_ptr<Attribute<ValueType>> delete_and_export_attribute(
        std::string_view name,
        AttributeDeletePolicy delete_policy = AttributeDeletePolicy::ErrorIfReserved,
        AttributeExportPolicy export_policy = AttributeExportPolicy::CopyIfExternal);

    ///
    /// Delete an attribute and export its content in a read-only shared_ptr. The attribute must
    /// exist.
    /// - If the content is managed by the mesh:
    ///   1. If it is uniquely owned, no copy is performed.
    ///   2. If ownership is shared, a copy is performed (just like any other copy-on-write
    ///      operation).
    /// - If the content points to an external buffer, a policy flag determines what happens.
    ///
    /// @param[in]  name           %Attribute name
    /// @param[in]  delete_policy  Delete policy for reserved attribute names.
    /// @param[in]  export_policy  Export policy for non-owned buffers.
    ///
    /// @tparam     ValueType      Value type for the attribute.
    ///
    /// @return     A shared pointer holding the attribute data.
    ///
    template <typename ValueType>
    std::shared_ptr<const Attribute<ValueType>> delete_and_export_const_attribute(
        std::string_view name,
        AttributeDeletePolicy delete_policy = AttributeDeletePolicy::ErrorIfReserved,
        AttributeExportPolicy export_policy = AttributeExportPolicy::CopyIfExternal);

    ///
    /// Delete an indexed attribute and export its content in a writable shared_ptr. The attribute
    /// must exist.
    /// - If the content is managed by the mesh:
    ///   1. If it is uniquely owned, no copy is performed.
    ///   2. If ownership is shared, a copy is performed (just like any other copy-on-write
    ///      operation).
    /// - If the content points to an external buffer, a policy flag determines what happens.
    ///
    /// @param[in]  name       %Attribute name
    /// @param[in]  policy     Export policy for non-owned buffers.
    ///
    /// @tparam     ValueType  Value type for the attribute.
    ///
    /// @return     A shared pointer holding the attribute data.
    ///
    template <typename ValueType>
    auto delete_and_export_indexed_attribute(
        std::string_view name,
        AttributeExportPolicy policy = AttributeExportPolicy::CopyIfExternal)
        -> std::shared_ptr<IndexedAttribute<ValueType, Index>>;

    ///
    /// Delete an indexed attribute and export its content in a read-only shared_ptr. The attribute
    /// must exist.
    /// - If the content is managed by the mesh:
    ///   1. If it is uniquely owned, no copy is performed.
    ///   2. If ownership is shared, a copy is performed (just like any other copy-on-write
    ///      operation).
    /// - If the content points to an external buffer, a policy flag determines what happens.
    ///
    /// @param[in]  name       %Attribute name
    /// @param[in]  policy     Export policy for non-owned buffers.
    ///
    /// @tparam     ValueType  Value type for the attribute.
    ///
    /// @return     A shared pointer holding the attribute data.
    ///
    template <typename ValueType>
    auto delete_and_export_const_indexed_attribute(
        std::string_view name,
        AttributeExportPolicy policy = AttributeExportPolicy::CopyIfExternal)
        -> std::shared_ptr<const IndexedAttribute<ValueType, Index>>;

public:
    /// @}
    /// @name Attribute accessors
    /// @{

    ///
    /// Checks if an attribute of a given name is attached to the mesh.
    ///
    /// @note       %Attribute ids can be reused after deletion, so it does not make sense to test
    ///             if an attribute exist based on an attribute id.
    ///
    /// @param[in]  name  %Attribute name.
    ///
    /// @return     True if the attribute exists, False otherwise.
    ///
    [[nodiscard]] bool has_attribute(std::string_view name) const;

    ///
    /// Checks whether the specified attribute is of a given type.
    ///
    /// @param[in]  name        Name of the attribute to test.
    ///
    /// @tparam     ValueType  %Attribute type to test against.
    ///
    /// @return     True if the specified attribute has the given type, False otherwise.
    ///
    template <typename ValueType>
    [[nodiscard]] bool is_attribute_type(std::string_view name) const;

    ///
    /// Checks whether the specified attribute is of a given type.
    ///
    /// @param[in]  id          Id of the attribute to test.
    ///
    /// @tparam     ValueType  %Attribute type to test against.
    ///
    /// @return     True if the specified attribute has the given type, False otherwise.
    ///
    template <typename ValueType>
    [[nodiscard]] bool is_attribute_type(AttributeId id) const;

    /// Determines whether the specified attribute is indexed.
    ///
    /// @param[in]  name  Name of the attribute to test.
    ///
    /// @return     True if the specified attribute is indexed, False otherwise.
    ///
    [[nodiscard]] bool is_attribute_indexed(std::string_view name) const;

    /// Determines whether the specified attribute is indexed.
    ///
    /// @param[in]  id    Id of the attribute to test.
    ///
    /// @return     True if the specified attribute is indexed, False otherwise.
    ///
    [[nodiscard]] bool is_attribute_indexed(AttributeId id) const;

    ///
    /// Iterates over all attribute ids sequentially. This function is intended to be a low-level
    /// operator. For convenience functions to visit mesh attributes. See foreach_attribute.h for
    /// more information.
    ///
    /// @see        foreach_attribute.h
    ///
    /// @param[in]  func  Function to iterate over attributes ids.
    ///
    void seq_foreach_attribute_id(function_ref<void(AttributeId)> func) const;

    ///
    /// Iterates over all pairs of attribute names x ids sequentially. This function is intended to
    /// be a low-level operator. For convenience functions to visit mesh attributes. See
    /// foreach_attribute.h for more information.
    ///
    /// @see        foreach_attribute.h
    ///
    /// @param[in]  func  Function to iterate over attributes names x ids.
    ///
    void seq_foreach_attribute_id(function_ref<void(std::string_view, AttributeId)> func) const;

    ///
    /// Iterates over all attribute ids in parallel. This function is intended to be a low-level operator. For
    /// convenience functions to visit mesh attributes. See foreach_attribute.h for more
    /// information.
    ///
    /// @see        foreach_attribute.h
    ///
    /// @param[in]  func  Function to iterate over attributes ids.
    ///
    void par_foreach_attribute_id(function_ref<void(AttributeId)> func) const;

    ///
    /// Iterates over all pairs of attribute names x ids in parallel. This function is intended to
    /// be a low-level operator. For convenience functions to visit mesh attributes. See
    /// foreach_attribute.h for more information.
    ///
    /// @see        foreach_attribute.h
    ///
    /// @param[in]  func  Function to iterate over attributes names x ids.
    ///
    void par_foreach_attribute_id(function_ref<void(std::string_view, AttributeId)> func) const;

    ///
    /// Gets a read-only reference to the base class of attribute given its name.
    ///
    /// @param[in]  name  %Attribute name.
    ///
    /// @return     The attribute reference.
    ///
    [[nodiscard]] const AttributeBase& get_attribute_base(std::string_view name) const;

    ///
    /// Gets a read-only reference to an attribute given its id.
    ///
    /// @param[in]  id    %Attribute id.
    ///
    /// @return     The attribute reference.
    ///
    [[nodiscard]] const AttributeBase& get_attribute_base(AttributeId id) const;

    ///
    /// Gets a read-only reference to an attribute given its name.
    ///
    /// @param[in]  name        %Attribute name.
    ///
    /// @tparam     ValueType  Scalar type stored in the attribute.
    ///
    /// @return     The attribute reference.
    ///
    template <typename ValueType>
    [[nodiscard]] const Attribute<ValueType>& get_attribute(std::string_view name) const;

    ///
    /// Gets a read-only reference to an attribute given its id.
    ///
    /// @param[in]  id          %Attribute id.
    ///
    /// @tparam     ValueType  Scalar type stored in the attribute.
    ///
    /// @return     The attribute reference.
    ///
    template <typename ValueType>
    [[nodiscard]] const Attribute<ValueType>& get_attribute(AttributeId id) const;

    ///
    /// Gets a read-only weak pointer to the base attribute object.
    ///
    /// @param[in]  name  %Attribute name.
    ///
    /// @return     A weak ptr to the base attribute object.
    ///
    /// @note This method is for tracking the life span of the attribute object.
    ///
    [[nodiscard]] std::weak_ptr<const AttributeBase> _get_attribute_ptr(
        std::string_view name) const;

    ///
    /// Gets a read-only weak pointer to the base attribute object.
    ///
    /// @param[in]  id    %Attribute id.
    ///
    /// @return     A weak ptr to the base attribute object.
    ///
    /// @note This method is for tracking the life span of the attribute object.
    ///
    [[nodiscard]] std::weak_ptr<const AttributeBase> _get_attribute_ptr(AttributeId id) const;

    ///
    /// Gets a read-only reference to an indexed attribute given its name.
    ///
    /// @param[in]  name        %Attribute name.
    ///
    /// @tparam     ValueType  Scalar type stored in the attribute.
    ///
    /// @return     The attribute reference.
    ///
    template <typename ValueType>
    [[nodiscard]] auto get_indexed_attribute(std::string_view name) const
        -> const IndexedAttribute<ValueType, Index>&;

    ///
    /// Gets a read-only reference to an indexed attribute given its id.
    ///
    /// @param[in]  id          %Attribute id.
    ///
    /// @tparam     ValueType  Scalar type stored in the attribute.
    ///
    /// @return     The attribute reference.
    ///
    template <typename ValueType>
    [[nodiscard]] auto get_indexed_attribute(AttributeId id) const
        -> const IndexedAttribute<ValueType, Index>&;

    ///
    /// Gets a writable reference to an attribute given its name. If the attribute is a shallow copy
    /// with shared ownership, calling this function will result in a copy of the data.
    ///
    /// @param[in]  name        %Attribute name.
    ///
    /// @tparam     ValueType  Scalar type stored in the attribute.
    ///
    /// @return     The attribute reference.
    ///
    template <typename ValueType>
    [[nodiscard]] Attribute<ValueType>& ref_attribute(std::string_view name);

    ///
    /// Gets a writable reference to an attribute given its id. If the attribute is a shallow copy
    /// with shared ownership, calling this function will result in a copy of the data.
    ///
    /// @param[in]  id          %Attribute id.
    ///
    /// @tparam     ValueType  Scalar type stored in the attribute.
    ///
    /// @return     The attribute reference.
    ///
    template <typename ValueType>
    [[nodiscard]] Attribute<ValueType>& ref_attribute(AttributeId id);

    ///
    /// Gets a weak pointer to the base attribute object.
    ///
    /// @param[in]  name  %Attribute name.
    ///
    /// @return     A weak ptr to the base attribute object.
    ///
    /// @note This method is for tracking the life span of the attribute object.
    ///
    [[nodiscard]] std::weak_ptr<AttributeBase> _ref_attribute_ptr(std::string_view name);

    ///
    /// Gets a weak pointer to the base attribute object.
    ///
    /// @param[in]  id    %Attribute id.
    ///
    /// @return     A weak ptr to the base attribute object.
    ///
    /// @note This method is for tracking the life span of the attribute object.
    ///
    [[nodiscard]] std::weak_ptr<AttributeBase> _ref_attribute_ptr(AttributeId id);

    ///
    /// Gets a writable reference to an indexed attribute given its name. If the attribute is a
    /// shallow copy with shared ownership, calling this function will result in a copy of the data.
    ///
    /// @param[in]  name        %Attribute name.
    ///
    /// @tparam     ValueType  Scalar type stored in the attribute.
    ///
    /// @return     The attribute reference.
    ///
    template <typename ValueType>
    [[nodiscard]] auto ref_indexed_attribute(std::string_view name)
        -> IndexedAttribute<ValueType, Index>&;

    ///
    /// Gets a writable reference to an indexed attribute given its id. If the attribute is a
    /// shallow copy with shared ownership, calling this function will result in a copy of the data.
    ///
    /// @param[in]  id          %Attribute id.
    ///
    /// @tparam     ValueType  Scalar type stored in the attribute.
    ///
    /// @return     The attribute reference.
    ///
    template <typename ValueType>
    [[nodiscard]] auto ref_indexed_attribute(AttributeId id) -> IndexedAttribute<ValueType, Index>&;

    ///
    /// Gets a read-only reference to the vertex -> positions attribute.
    ///
    /// @return     Reference to the attribute.
    ///
    [[nodiscard]] const Attribute<Scalar>& get_vertex_to_position() const;

    ///
    /// Gets a writable reference to the vertex -> positions attribute.
    ///
    /// @return     Reference to the attribute.
    ///
    [[nodiscard]] Attribute<Scalar>& ref_vertex_to_position();

    ///
    /// Gets a read-only reference to the corner -> vertex id attribute.
    ///
    /// @return     Vertex indices attribute.
    ///
    [[nodiscard]] const Attribute<Index>& get_corner_to_vertex() const;

    ///
    /// Gets a writable reference to the corner -> vertex id attribute.
    ///
    /// @warning    If the mesh contains edge/connectivity attributes, this function will throw an
    ///             exception. This is because it is impossible to update edge/connectivity
    ///             information if the facet buffer is directly modified by the user. Instead, the
    ///             correct facet indices must be provided when the facet is constructed.
    ///
    /// @return     Vertex indices attribute.
    ///
    [[nodiscard]] Attribute<Index>& ref_corner_to_vertex();

public:
    /// @}
    /// @name Reserved attribute names and ids
    /// @{

    ///
    /// Check whether the given name corresponds to a reserved %attribute. Reserved %attributes are
    /// %attributes whose name starts with a "$".
    ///
    /// @param[in]  name  %Attribute name.
    ///
    /// @return     True if this is the name of a reserved %attribute, false otherwise.
    ///
    static bool attr_name_is_reserved(std::string_view name);

    ///
    /// %Attribute name for vertex -> position.
    ///
    /// @return     %Attribute name.
    ///
    static constexpr std::string_view attr_name_vertex_to_position()
    {
        return s_vertex_to_position;
    }

    ///
    /// %Attribute name for corner -> vertex indices.
    ///
    /// @return     %Attribute name.
    ///
    static constexpr std::string_view attr_name_corner_to_vertex() { return s_corner_to_vertex; }

    ///
    /// %Attribute name for facet -> first corner index.
    ///
    /// @return     %Attribute name.
    ///
    static constexpr std::string_view attr_name_facet_to_first_corner()
    {
        return s_facet_to_first_corner;
    }

    ///
    /// %Attribute name for corner -> facet index.
    ///
    /// @return     %Attribute name.
    ///
    static constexpr std::string_view attr_name_corner_to_facet() { return s_corner_to_facet; }

    ///
    /// %Attribute name for corner -> edge indices.
    ///
    /// @return     %Attribute name.
    ///
    static constexpr std::string_view attr_name_corner_to_edge() { return s_corner_to_edge; }

    ///
    /// %Attribute name for edge -> first corner index.
    ///
    /// @return     %Attribute name.
    ///
    static constexpr std::string_view attr_name_edge_to_first_corner()
    {
        return s_edge_to_first_corner;
    }

    ///
    /// %Attribute name for corner -> next corner around edge.
    ///
    /// @return     %Attribute name.
    ///
    static constexpr std::string_view attr_name_next_corner_around_edge()
    {
        return s_next_corner_around_edge;
    }

    ///
    /// %Attribute name for vertex -> first corner index.
    ///
    /// @return     %Attribute name.
    ///
    static constexpr std::string_view attr_name_vertex_to_first_corner()
    {
        return s_vertex_to_first_corner;
    }

    ///
    /// %Attribute name for corner -> next corner around vertex.
    ///
    /// @return     %Attribute name.
    ///
    static constexpr std::string_view attr_name_next_corner_around_vertex()
    {
        return s_next_corner_around_vertex;
    }

    ///
    /// %Attribute id for vertex -> positions.
    ///
    /// @return     Attribute id.
    ///
    AttributeId attr_id_vertex_to_positions() const { return m_vertex_to_position_id; }

    ///
    /// %Attribute id for corner -> vertex indices.
    ///
    /// @return     Attribute id.
    ///
    AttributeId attr_id_corner_to_vertex() const { return m_corner_to_vertex_id; }

    ///
    /// %Attribute id for facet -> first corner index.
    ///
    /// @return     Attribute id.
    ///
    AttributeId attr_id_facet_to_first_corner() const { return m_facet_to_first_corner_id; }

    ///
    /// %Attribute id for corner -> facet index.
    ///
    /// @return     Attribute id.
    ///
    AttributeId attr_id_corner_to_facet() const { return m_corner_to_facet_id; }

    ///
    /// %Attribute id for corner -> edge indices.
    ///
    /// @return     Attribute id.
    ///
    AttributeId attr_id_corner_to_edge() const { return m_corner_to_edge_id; }

    ///
    /// %Attribute id for edge -> first corner index.
    ///
    /// @return     Attribute id.
    ///
    AttributeId attr_id_edge_to_first_corner() const { return m_edge_to_first_corner_id; }

    ///
    /// %Attribute id for corner -> next corner around edge.
    ///
    /// @return     Attribute id.
    ///
    AttributeId attr_id_next_corner_around_edge() const { return m_next_corner_around_edge_id; }

    ///
    /// %Attribute id for vertex -> first corner index.
    ///
    /// @return     Attribute id.
    ///
    AttributeId attr_id_vertex_to_first_corner() const { return m_vertex_to_first_corner_id; }

    ///
    /// %Attribute id for corner -> next corner around vertex.
    ///
    /// @return     Attribute id.
    ///
    AttributeId attr_id_next_corner_around_vertex() const { return m_next_corner_around_vertex_id; }

public:
    /// @}
    /// @name Mesh edges and connectivity
    /// @{

    ///
    /// Initializes attributes associated to mesh edges and connectivity. If a user-defined
    /// ordering of the mesh edges is provided, it must be a valid indexing (all edges should appear
    /// in the provided array).
    ///
    /// @param[in]  edges  M x 2 continuous array of mapping edge -> vertices, where M is the number
    ///                    of edges in the mesh.
    ///
    void initialize_edges(span<const Index> edges = {});

    ///
    /// Initializes attributes associated to mesh edges and connectivity. In this overload, a
    /// user-defined ordering of the mesh edges is provided via a function callback. The
    /// user-provided ordering must be a valid indexing (all edges should appear exactly once).
    ///
    /// @param[in]  num_user_edges  Number of edges in the user-provided ordering. If it does not
    ///                             match the actual number of mesh edges after initialization, an
    ///                             exception is raised.
    /// @param[in]  get_user_edge   Callback function to retrieve the vertices endpoints of an edge
    ///                             given its user-provided index.
    ///
    void initialize_edges(Index num_user_edges, GetEdgeVertices get_user_edge);

    ///
    /// Clears attributes related to mesh edges and connectivity:
    ///
    /// - "$corner_to_edge"
    /// - "$vertex_to_first_corner"
    /// - "$next_corner_around_vertex"
    /// - "$edge_to_first_corner"
    /// - "$next_corner_around_edge"
    ///
    /// This method also sets the number of mesh edges to 0, effectively resizing all user-defined
    /// edge attributes to 0.
    ///
    void clear_edges();

    ///
    /// Determines if the attributes associated to mesh edges and connectivity have been initialized.
    ///
    /// @return     True if attributes have been initialized, False otherwise.
    ///
    bool has_edges() const { return m_edge_to_first_corner_id != invalid_attribute_id(); }

    ///
    /// Gets the edge index corresponding to (f, lv) -- (f, lv+1).
    ///
    /// @param[in]  f     Facet index.
    /// @param[in]  lv    Local vertex index [0, get_vertex_per_facet()[.
    ///
    /// @return     Edge index.
    ///
    Index get_edge(Index f, Index lv) const;

    ///
    /// Gets the edge index corresponding to a corner index. Given a face (v0, ..., vk) with
    /// associated corners (c0, ..., ck), the edge associated to corner ci is the edge between (vi,
    /// vi+1), as determined by the corner_to_edge_mapping function.
    ///
    /// @param[in]  c     Corner index.
    ///
    /// @return     Edge index.
    ///
    Index get_corner_edge(Index c) const;

    ///
    /// @copydoc get_corner_edge
    ///
    /// @deprecated Use get_corner_edge instead.
    ///
    [[deprecated("Use get_corner_edge() instead.")]] Index get_edge_from_corner(Index c) const;

    ///
    /// Retrieve edge endpoints.
    ///
    /// @param[in]  e     Queried edge index.
    ///
    /// @return     Array of vertex ids at the edge endpoints.
    ///
    std::array<Index, 2> get_edge_vertices(Index e) const;

    ///
    /// Get the index of the first corner around a given edge.
    ///
    /// @param[in]  e     Queried edge index.
    ///
    /// @return     Index of the first corner around the queried edge.
    ///
    Index get_first_corner_around_edge(Index e) const;

    ///
    /// Gets the next corner around the edge associated to a corner. If the corner is the last one
    /// in the chain, this function returns INVALID<Index>.
    ///
    /// @param[in]  c     Corner index.
    ///
    /// @return     Next corner around the edge.
    ///
    Index get_next_corner_around_edge(Index c) const;

    ///
    /// Get the index of the first corner around a given vertex.
    ///
    /// @param[in]  v     Queried vertex index.
    ///
    /// @return     Index of the first corner around the queried vertex.
    ///
    Index get_first_corner_around_vertex(Index v) const;

    ///
    /// Gets the next corner around the vertex associated to a corner. If the corner is the last one
    /// in the chain, this function returns INVALID<Index>.
    ///
    /// @note The sequence of corners around a vertex defined by this function
    /// has no geometric meaning. I.e. the current corner and its next corner
    /// may not be adjacent to the same edge.
    ///
    /// @param[in]  c     Corner index.
    ///
    /// @return     Next corner around the vertex.
    ///
    Index get_next_corner_around_vertex(Index c) const;

    ///
    /// Count the number of corners incident to a given edge.
    ///
    /// @note       This may not be the same as the number of incident facets. E.g. the same facet
    ///             may be incident to the same edge multiple times. To count the number of incident
    ///             facet without duplications, you will need to write your own function.
    ///
    /// @param[in]  e     Queried edge index.
    ///
    /// @return     Number of corners incident to the queried edge.
    ///
    Index count_num_corners_around_edge(Index e) const;

    ///
    /// Count the number of corners incident to a given vertex.
    ///
    /// @note       This may not be the same as the number of incident facets. E.g. the same facet
    ///             may be incident to the same vertex multiple times. To count the number of
    ///             incident facet without duplications, you will need to write your own function.
    ///
    /// @param[in]  v     Queried vertex index.
    ///
    /// @return     Number of corners incident to the queried vertex.
    ///
    Index count_num_corners_around_vertex(Index v) const;

    ///
    /// Get the index of one facet around a given edge.
    ///
    /// @param[in]  e     Queried edge index.
    ///
    /// @return     Face index of one facet incident to the queried edge.
    ///
    Index get_one_facet_around_edge(Index e) const;

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
    Index get_one_corner_around_edge(Index e) const;

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
    Index get_one_corner_around_vertex(Index v) const;

    ///
    /// Determines whether the specified edge e is a boundary edge.
    ///
    /// @param[in]  e     Queried edge index.
    ///
    /// @return     True if the specified edge e is a boundary edge, False otherwise.
    ///
    bool is_boundary_edge(Index e) const;

    ///
    /// Applies a function to each facet around a prescribed edge.
    ///
    /// @warning    If a facet is incident to the same edge multiple times (e.g. facet {0, 1, 0,
    ///             1}), the callback function will be called multiple times for te facet.
    ///
    /// @param[in]  e     Queried edge index.
    /// @param[in]  func  Callback to apply to each incident facet.
    ///
    void foreach_facet_around_edge(Index e, function_ref<void(Index)> func) const;

    ///
    /// Applies a function to each facet around a prescribed vertex.
    ///
    /// @warning    If a facet is incident to the same vertex multiple times (e.g. facet {0, 1, 0,
    ///             1}), the callback function will be called multiple times for the facet.
    ///
    /// @param[in]  v     Queried vertex index.
    /// @param[in]  func  Callback to apply to each incident facet.
    ///
    void foreach_facet_around_vertex(Index v, function_ref<void(Index)> func) const;

    ///
    /// Applies a function to each corner around a prescribed edge.
    ///
    /// @param[in]  e     Queried edge index.
    /// @param[in]  func  Callback to apply to each incident corner.
    ///
    void foreach_corner_around_edge(Index e, function_ref<void(Index)> func) const;

    ///
    /// Applies a function to each corner around a prescribed vertex.
    ///
    /// @param[in]  v     Queried vertex index.
    /// @param[in]  func  Callback to apply to each incident corner.
    ///
    void foreach_corner_around_vertex(Index v, function_ref<void(Index)> func) const;

    ///
    /// Applies a function to each edge around a prescribed vertex. The function will be applied
    /// repeatedly to each incident edges (once for each facet incident to the query vertex).
    ///
    /// @param[in]  v     Queried vertex index.
    /// @param[in]  func  Callback to apply to each incident edge.
    ///
    void foreach_edge_around_vertex_with_duplicates(Index v, function_ref<void(Index)> func) const;

    /// @}

protected:
    ///
    /// Same as create_attribute, but allows creation of reserved attributes.
    ///
    /// @param[in]  name             %Attribute name to create.
    /// @param[in]  element          %Mesh element to which the attribute is attached to (Vertex,
    ///                              Facet, etc.).
    /// @param[in]  usage            Tag to indicate how the values are modified under rigid
    ///                              transformation.
    /// @param[in]  num_channels     The number of channels for the attribute. Cannot be modified
    ///                              once the attribute has been created.
    /// @param[in]  initial_values   A span of initial values to populate the attribute values with.
    ///                              The data is copied into the attribute. If the span is provided,
    ///                              it must have the right dimension (number of elements x number
    ///                              of channels).
    /// @param[in]  initial_indices  A span of initial values to populate the attribute indices
    ///                              with. If the attribute element type is not Indexed, providing a
    ///                              non-empty value for this argument will result in a runtime
    ///                              error. The data is copied into the attribute. If the span is
    ///                              provided, it must have the right dimension (number of corners).
    ///
    /// @tparam     ValueType        Value type for the attribute.
    ///
    /// @return     The attribute identifier.
    ///
    template <typename ValueType>
    AttributeId create_attribute_internal(
        std::string_view name,
        AttributeElement element,
        AttributeUsage usage = AttributeUsage::Vector,
        size_t num_channels = 1,
        span<const ValueType> initial_values = {},
        span<const Index> initial_indices = {});

    ///
    /// Set attribute default value for known internal attributes.
    ///
    /// @param[in]  name       Name of the attribute being set.
    ///
    /// @tparam     ValueType  Value type for the attribute.
    ///
    template <typename ValueType>
    void set_attribute_default_internal(std::string_view name);

    ///
    /// Reindex mesh vertices according to the given mapping. It is ok to map several vertices to
    /// the same index. The mapping must form an increasing sequence, such that it is possible to
    /// copy values without an intermediate buffer. Elements mapped to invalid<Index>() will not be
    /// copied.
    ///
    /// @param[in]  old_to_new  Mapping old vertex index -> new vertex index.
    ///
    void reindex_vertices_internal(span<const Index> old_to_new);

    ///
    /// Reindex mesh facets according to the given mapping. Facet corners will be copied
    /// accordingly. It is ok to map several facets to the same index. The mapping must form an
    /// increasing sequence, such that it is possible to copy values without an intermediate buffer.
    /// Elements mapped to invalid<Index>() will not be copied.
    ///
    /// @param[in]  old_to_new  Mapping old facet index -> new facet index.
    ///
    /// @return     A pair (nc, ne) containing the new number of corners & edges, respectively.
    ///
    std::pair<Index, Index> reindex_facets_internal(span<const Index> old_to_new);

    ///
    /// Resize the buffers associated to a specific element type in the mesh. Newly inserted
    /// elements will be default-initialized.
    ///
    /// @param[in]  num_elements  New number of elements.
    ///
    /// @tparam     element       Element type to resize (vertices, facets, etc.).
    ///
    template <AttributeElement element>
    void resize_elements_internal(Index num_elements);

    ///
    /// Resize the buffers associated to mesh vertices and their attributes. Newly inserted elements
    /// will be default-initialized.
    ///
    /// @param[in]  num_vertices  New number of vertices.
    ///
    void resize_vertices_internal(Index num_vertices);

    ///
    /// Resize the buffers associated to mesh facets and their attributes. Newly inserted elements
    /// will be default-initialized.
    ///
    /// @param[in]  num_facets  New number of facets.
    ///
    void resize_facets_internal(Index num_facets);

    ///
    /// Resize the buffers associated to mesh corners and their attributes. Newly inserted elements
    /// will be default-initialized.
    ///
    /// @param[in]  num_corners  New number of corners.
    ///
    void resize_corners_internal(Index num_corners);

    ///
    /// Resize the buffers associated to mesh edges and their attributes. Newly inserted elements
    /// will be default-initialized.
    ///
    /// @param[in]  num_edges  New number of edges.
    ///
    void resize_edges_internal(Index num_edges);

    ///
    /// Reserve index buffer for multiple facets of a given size. If storage needs to be changed to
    /// a hybrid format, allocates facet offsets as necessary.
    ///
    /// @param[in]  num_facets  Number of facets to add.
    /// @param[in]  facet_size  Size of each facet to add.
    ///
    /// @return     A writable span pointing to the newly inserted rows of the corner_to_vertex
    ///             attribute.
    ///
    span<Index> reserve_indices_internal(Index num_facets, Index facet_size);

    ///
    /// Reserve index buffer for multiple facets of a given size. If storage needs to be changed to
    /// a hybrid format, allocates facet offsets as necessary.
    ///
    /// @param[in]  facet_sizes  A contiguous array representing the size of each facet to add.
    ///
    /// @return     A writable span pointing to the newly inserted rows of the corner_to_vertex
    ///             attribute.
    ///
    span<Index> reserve_indices_internal(span<const Index> facet_sizes);

    ///
    /// Reserve index buffer for multiple facets of a given size. If storage needs to be changed to
    /// a hybrid format, allocates facet offsets as necessary.
    ///
    /// @param[in]  num_facets       Number of facets to add.
    /// @param[in]  get_facets_size  Callable function to retrieve the size of each facet to be
    ///                              added.
    ///
    /// @return     A writable span pointing to the newly inserted rows of the corner_to_vertex
    ///             attribute.
    ///
    span<Index> reserve_indices_internal(Index num_facets, GetFacetsSizeFunction get_facets_size);

    ///
    /// Compute inverse mapping corner -> facet index for a specific range of facets.
    ///
    /// @param[in]  facet_begin  First facet in the range.
    /// @param[in]  facet_end    Index beyond the last facet in the range.
    ///
    void compute_corner_to_facet_internal(Index facet_begin, Index facet_end);

    ///
    /// Initializes attributes associated to mesh edges and connectivity. Internal method to avoid
    /// code duplication.
    ///
    /// @param[in]  num_user_edges     Number of edges in the user-provided ordering.
    /// @param[in]  get_user_edge_ptr  Callback function to retrieve the vertices endpoints of an
    ///                                edge given its user-provided index.
    ///
    void initialize_edges_internal(
        Index num_user_edges = 0,
        GetEdgeVertices* get_user_edge_ptr = nullptr);

    ///
    /// Update attributes associated to mesh edges and connectivity for a specific range of facets.
    /// Corner chains are recomputed for the affected facet corners, unused edge indices are
    /// recycled and edge attributes are resized accordingly.
    ///
    /// This function is called internally when edge attributes are initialized, when new facets are
    /// added to the mesh, or manually after a user updates facet indices.
    ///
    /// If a user-defined ordering of the mesh edges is provided, it must be a valid indexing for
    /// the newly added edges (i.e. all edges that were not already present in the mesh must appear
    /// exactly once in the user-provided array).
    ///
    /// @see        initialize_edges
    ///
    /// @param[in]  facet_begin        First facet in the range.
    /// @param[in]  facet_end          Index beyond the last facet in the range.
    /// @param[in]  num_user_edges     Number of edges in the user-provided ordering.
    /// @param[in]  get_user_edge_ptr  Callback function to retrieve the vertices endpoints of an
    ///                                edge given its user-provided index.
    ///
    void update_edges_range_internal(
        Index facet_begin,
        Index facet_end,
        Index num_user_edges = 0,
        GetEdgeVertices* get_user_edge_ptr = nullptr);

    ///
    /// Same as update_edges_range_internal, but operate on the last count facets in the mesh
    /// instead.
    ///
    /// @param[in]  count              Number of facets to update.
    /// @param[in]  num_user_edges     Number of edges in the user-provided ordering.
    /// @param[in]  get_user_edge_ptr  Callback function to retrieve the vertices endpoints of an
    ///                                edge given its user-provided index.
    ///
    void update_edges_last_internal(
        Index count,
        Index num_user_edges = 0,
        GetEdgeVertices* get_user_edge_ptr = nullptr);

    ///
    /// Gets the number of mesh elements, based on an element type. If the queried element type is
    /// edges, and edge data has not been initialized, an exception is thrown.
    ///
    /// @param[in]  element  Type of element (vertex, facet, etc.).
    ///
    /// @return     The number of such elements in the mesh.
    ///
    [[nodiscard]] size_t get_num_elements_internal(AttributeElement element) const;

    ///
    /// Wrap a span in an attribute.
    ///
    /// @tparam ValueType       The attribute value type.
    ///
    /// @param[in] attr         The attribute object.
    /// @param[in] num_values   The number of elements to wrap.
    /// @param[in] values_view  A view of the raw buffer.  Its size must sufficiently large to hold
    ///                         at least `num_values` elements.
    ///
    template <typename ValueType>
    void wrap_as_attribute_internal(
        Attribute<std::decay_t<ValueType>>& attr,
        size_t num_values,
        span<ValueType> values_view);

    ///
    /// Wrap a shared span in an attribute.  The attribute object will take a share of the memory
    /// ownership of the external buffer referred by the shared span so that it will stay alive
    /// as long as the attribute is using it.
    ///
    /// @tparam ValueType        The attribute value type.
    ///
    /// @param[in] attr          The attribute object.
    /// @param[in] num_values    The number of elements to wrap.
    /// @param[in] shared_values A shared span object with memory ownership tracking support.
    ///                          Its size must sufficiently large to hold at least `num_values`
    ///                          elements.
    ///
    template <typename ValueType>
    void wrap_as_attribute_internal(
        Attribute<std::decay_t<ValueType>>& attr,
        size_t num_values,
        SharedSpan<ValueType> shared_values);

    ///
    /// Wrap shared spans as offsets and facets.
    ///
    /// @tparam OffsetSpan  The offset span type.  Should be either `span<T>` or `SharedSpan<T>`.
    /// @tparam FacetSpan   The facet span type.  Should be either `span<T>` or `SharedSpan<T>`.
    ///
    /// @param offsets      The offset span.  Its size should be >= `num_facets`.
    /// @param num_facets   The number of facets.
    /// @param facets       The facet span.  Its size should be >= `num_corners`.
    /// @param num_corners  The number of corners.
    ///
    /// @return The corner-to-vertex attribute id.
    ///
    template <typename OffsetSpan, typename FacetSpan>
    AttributeId wrap_as_facets_internal(
        OffsetSpan offsets,
        Index num_facets,
        FacetSpan facets,
        Index num_corners);

    ///
    /// The most generic way of creating an attribute wrapped around external buffers.
    ///
    /// Based on the `element` type, it will create either a normal attribute or an indexed
    /// attribute.  The generated attribute will have the same "const-ness" as the `value_type` of
    /// the span.  If a SharedSpan is used, the attribute will participate in ownership sharing of
    /// the buffer it uses.
    ///
    /// @tparam ValueSpan   The value span type.  Should be either `span<T>` or `SharedSpan<T>`.
    /// @tparam IndexSpan   The index span type.  Should be either `span<T>` or `SharedSpan<T>`.
    ///
    /// @param name         The attribute name.
    /// @param element      The attribute element type.
    /// @param usage        The attribute usage type.
    /// @param num_values   The number of values.
    /// @param num_channels The number of channels.
    /// @param values       The value span.  Its size should be >= num_values * num_channels.
    /// @param indices      The index span.  Its size should be >= the number of corresponding
    ///                     elements.  It is only used for creating indexed attribute.
    ///
    /// @return The attribute id of the generated attribute.
    ///
    template <typename ValueSpan, typename IndexSpan = span<Index>>
    AttributeId wrap_as_attribute_internal(
        std::string_view name,
        AttributeElement element,
        AttributeUsage usage,
        size_t num_values,
        size_t num_channels,
        ValueSpan values,
        IndexSpan indices = {});

protected:
    /// @cond LA_INTERNAL_DOCS

    ///
    /// Hidden attribute manager class.
    ///
    struct AttributeManager;

    /// @endcond

protected:
    /// %Attribute name for vertex -> positions.
    static constexpr std::string_view s_vertex_to_position = "$vertex_to_position";

    /// %Attribute name for corner -> vertex indices.
    static constexpr std::string_view s_corner_to_vertex = "$corner_to_vertex";

    /// %Attribute name for facet -> first corner index (non-regular mesh).
    static constexpr std::string_view s_facet_to_first_corner = "$facet_to_first_corner";

    /// %Attribute name for corner -> facet index (non-regular mesh).
    static constexpr std::string_view s_corner_to_facet = "$corner_to_facet";

    /// %Attribute name for corner -> edge indices.
    static constexpr std::string_view s_corner_to_edge = "$corner_to_edge";

    /// %Attribute name for edge -> first corner index.
    static constexpr std::string_view s_edge_to_first_corner = "$edge_to_first_corner";

    /// %Attribute name for corner -> next corner around edge.
    static constexpr std::string_view s_next_corner_around_edge = "$next_corner_around_edge";

    /// %Attribute name for vertex -> first corner index.
    static constexpr std::string_view s_vertex_to_first_corner = "$vertex_to_first_corner";

    /// %Attribute name for corner -> next corner around vertex.
    static constexpr std::string_view s_next_corner_around_vertex = "$next_corner_around_vertex";

    /// Number of vertices
    Index m_num_vertices = Index(0);

    /// Number of facets
    Index m_num_facets = Index(0);

    /// Number of corners
    Index m_num_corners = Index(0);

    /// Number of edges
    Index m_num_edges = Index(0);

    /// Vertex dimension.
    Index m_dimension = Index(3);

    /// Number of vertices per facet. Either constant (> 0) or variable (= 0).
    Index m_vertex_per_facet = Index(0);

    /// %Attribute manager. Hidden implementation.
    value_ptr<AttributeManager> m_attributes;

    /// %Attribute id for vertex -> positions.
    AttributeId m_vertex_to_position_id = invalid_attribute_id();

    /// %Attribute id for corner -> vertex indices.
    AttributeId m_corner_to_vertex_id = invalid_attribute_id();

    /// %Attribute id for facet -> first corner index. If the mesh is regular (each facet has the
    /// same size), then this attribute doesn't exist.
    AttributeId m_facet_to_first_corner_id = invalid_attribute_id();

    /// %Attribute id for corner -> facet index. If the mesh is regular (each facet has the
    /// same size), then this attribute doesn't exist.
    AttributeId m_corner_to_facet_id = invalid_attribute_id();

    /// %Attribute id for corner -> edge indices.
    AttributeId m_corner_to_edge_id = invalid_attribute_id();

    /// %Attribute id for edge -> first corner index.
    AttributeId m_edge_to_first_corner_id = invalid_attribute_id();

    /// %Attribute id for corner -> next corner around edge.
    AttributeId m_next_corner_around_edge_id = invalid_attribute_id();

    /// %Attribute id for vertex -> first corner index.
    AttributeId m_vertex_to_first_corner_id = invalid_attribute_id();

    /// %Attribute id for corner -> next corner around vertex.
    AttributeId m_next_corner_around_vertex_id = invalid_attribute_id();
};

using SurfaceMesh32f = SurfaceMesh<float, uint32_t>;
using SurfaceMesh32d = SurfaceMesh<double, uint32_t>;
using SurfaceMesh64f = SurfaceMesh<float, uint64_t>;
using SurfaceMesh64d = SurfaceMesh<double, uint64_t>;

/// @}

} // namespace lagrange
