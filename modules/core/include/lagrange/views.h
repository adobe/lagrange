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

#include <lagrange/SurfaceMesh.h>

#include <Eigen/Core>

namespace lagrange {

///
/// @defgroup   group-surfacemesh-views Eigen views
/// @ingroup    group-surfacemesh
///
/// View mesh attributes as Eigen matrices.
///
/// Mesh attributes such as positions and facet indices can be views as Eigen matrices.
/// Specifically, we provide read-only views as `Eigen::Map<const ...>`:
///
/// @code
/// #include <lagrange/SurfaceMesh.h>
/// #include <lagrange/views.h>
///
/// #include <igl/massmatrix.h>
///
/// lagrange::SurfaceMesh<Scalar, Index> mesh;
///
/// // Fill up mesh data...
///
/// // With ADL, no need to prefix by lagrange::
/// auto V_view = vertex_view(mesh);
/// auto F_view = facet_view(mesh);
///
/// // Call your favorite libigl function
/// Eigen::SparseMatrix<Scalar> M;
/// igl::massmatrix(V_view, F_view, igl::MASSMATRIX_TYPE_VORONOI, M);
/// @endcode
///
/// Writable reference are also available:
/// @code
/// // Writable reference, creates a copy if buffer is not uniquely owned
/// auto V_ref = vertex_ref(mesh);
/// auto F_ref = facet_ref(mesh);
///
/// // Center mesh around vertex barycenter
/// V_ref.rowwise() -= V_ref.colwise().mean();
/// @endcode
///
/// @{

////////////////////////////////////////////////////////////////////////////////
/// @name Typedefs
/// @{
////////////////////////////////////////////////////////////////////////////////

/// Type alias for row-major Eigen matrices.
template <typename Scalar>
using RowMatrix = Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;

/// Type alias for row-major matrix views.
template <typename Scalar>
using RowMatrixView = Eigen::Map<RowMatrix<Scalar>, Eigen::Unaligned>;

/// Type alias for row-major const matrix view.
template <typename Scalar>
using ConstRowMatrixView = Eigen::Map<const RowMatrix<Scalar>, Eigen::Unaligned>;

/// Type alias for one-dimensional column Eigen vectors.
template <typename Scalar>
using Vector = Eigen::Matrix<Scalar, Eigen::Dynamic, 1>;

/// Type alias for row-major vector view.
template <typename Scalar>
using VectorView = Eigen::Map<Vector<Scalar>, Eigen::Unaligned>;

/// Type alias for row-major const vector view.
template <typename Scalar>
using ConstVectorView = Eigen::Map<const Vector<Scalar>, Eigen::Unaligned>;

////////////////////////////////////////////////////////////////////////////////
/// @}
/// @name Generic attribute views
/// @{
////////////////////////////////////////////////////////////////////////////////

///
/// Returns a writable view of a given attribute in the form of an Eigen matrix.
///
/// @param[in]  attribute  Attribute to view.
///
/// @tparam     ValueType  Attribute scalar type.
///
/// @return     An Eigen::Map wrapping the attribute data.
///
template <typename ValueType>
RowMatrixView<ValueType> matrix_ref(Attribute<ValueType>& attribute);

///
/// Returns a read-only view of a given attribute in the form of an Eigen matrix.
///
/// @param[in]  attribute  Attribute to view.
///
/// @tparam     ValueType  Attribute scalar type.
///
/// @return     An Eigen::Map wrapping the attribute data.
///
template <typename ValueType>
ConstRowMatrixView<ValueType> matrix_view(const Attribute<ValueType>& attribute);

///
/// Returns a writable view of a scalar attribute in the form of an Eigen vector. The attribute must
/// have exactly one channel.
///
/// @param[in]  attribute  Attribute to view.
///
/// @tparam     ValueType  Attribute scalar type.
///
/// @return     An Eigen::Map wrapping the attribute data.
///
template <typename ValueType>
VectorView<ValueType> vector_ref(Attribute<ValueType>& attribute);

///
/// Returns a read-only view of a scalar attribute in the form of an Eigen vector. The attribute
/// must have exactly one channel.
///
/// @param[in]  attribute  Attribute to view.
///
/// @tparam     ValueType  Attribute scalar type.
///
/// @return     An Eigen::Map wrapping the attribute data.
///
template <typename ValueType>
ConstVectorView<ValueType> vector_view(const Attribute<ValueType>& attribute);

///
/// Returns a writable view of a given single-channel attribute in the form of an Eigen matrix with
/// a prescribed number of columns. This is useful to view arbitrary corner attributes as 2D
/// matrices for regular meshes.
///
/// @param[in]  attribute  Attribute to view.
/// @param[in]  num_cols   Number of columns to use. It must divide the number of elements of the
///                        prescribed attribute.
///
/// @tparam     ValueType  Attribute scalar type.
///
/// @return     An Eigen::Map wrapping the attribute data.
///
template <typename ValueType>
RowMatrixView<ValueType> reshaped_ref(Attribute<ValueType>& attribute, size_t num_cols);

///
/// Returns a read-only view of a given single-channel attribute in the form of an Eigen matrix with
/// a prescribed number of columns. This is useful to view arbitrary corner attributes as 2D
/// matrices for regular meshes.
///
/// @param[in]  attribute  Attribute to view.
/// @param[in]  num_cols   Number of columns to use. It must divide the number of elements of the
///                        prescribed attribute.
///
/// @tparam     ValueType  Attribute scalar type.
///
/// @return     An Eigen::Map wrapping the attribute data.
///
template <typename ValueType>
ConstRowMatrixView<ValueType> reshaped_view(const Attribute<ValueType>& attribute, size_t num_cols);

////////////////////////////////////////////////////////////////////////////////
/// @}
/// @name Generic attribute views (mesh)
/// @{
////////////////////////////////////////////////////////////////////////////////

///
/// Returns a writable view of a mesh attribute in the form of an Eigen matrix.
///
/// @param[in]  mesh       Mesh whose attribute to view.
/// @param[in]  name       Name of the mesh attribute to view.
///
/// @tparam     ValueType  Attribute scalar type.
/// @tparam     Scalar     Mesh scalar type.
/// @tparam     Index      Mesh index type.
///
/// @return     An Eigen::Map wrapping the attribute data.
///
template <typename ValueType, typename Scalar, typename Index>
RowMatrixView<ValueType> attribute_matrix_ref(
    SurfaceMesh<Scalar, Index>& mesh,
    std::string_view name);

///
/// Returns a read-only view of a mesh attribute in the form of an Eigen matrix.
///
/// @param[in]  mesh       Mesh whose attribute to view.
/// @param[in]  name       Name of the mesh attribute to view.
///
/// @tparam     ValueType  Attribute scalar type.
/// @tparam     Scalar     Mesh scalar type.
/// @tparam     Index      Mesh index type.
///
/// @return     An Eigen::Map wrapping the attribute data.
///
template <typename ValueType, typename Scalar, typename Index>
ConstRowMatrixView<ValueType> attribute_matrix_view(
    const SurfaceMesh<Scalar, Index>& mesh,
    std::string_view name);

///
/// Returns a writable view of a mesh attribute in the form of an Eigen vector. The attribute must
/// have exactly one channel.
///
/// @param[in]  mesh       Mesh whose attribute to view.
/// @param[in]  name       Name of the mesh attribute to view.
///
/// @tparam     ValueType  Attribute scalar type.
/// @tparam     Scalar     Mesh scalar type.
/// @tparam     Index      Mesh index type.
///
/// @return     An Eigen::Map wrapping the attribute data.
///
template <typename ValueType, typename Scalar, typename Index>
RowMatrixView<ValueType> attribute_vector_ref(
    SurfaceMesh<Scalar, Index>& mesh,
    std::string_view name);

///
/// Returns a read-only view of a mesh attribute in the form of an Eigen vector. The attribute must
/// have exactly one channel.
///
/// @param[in]  mesh       Mesh whose attribute to view.
/// @param[in]  name       Name of the mesh attribute to view.
///
/// @tparam     ValueType  Attribute scalar type.
/// @tparam     Scalar     Mesh scalar type.
/// @tparam     Index      Mesh index type.
///
/// @return     An Eigen::Map wrapping the attribute data.
///
template <typename ValueType, typename Scalar, typename Index>
ConstRowMatrixView<ValueType> attribute_vector_view(
    const SurfaceMesh<Scalar, Index>& mesh,
    std::string_view name);

////////////////////////////////////////////////////////////////////////////////
/// @}
/// @name Specific attribute views
/// @{
////////////////////////////////////////////////////////////////////////////////

///
/// Returns a writable view of the mesh vertices in the form of an Eigen matrix.
///
/// @param[in]  mesh    Mesh whose vertices to view.
///
/// @tparam     Scalar  Mesh scalar type.
/// @tparam     Index   Mesh index type.
///
/// @return     An Eigen::Map wrapping the attribute data.
///
template <typename Scalar, typename Index>
RowMatrixView<Scalar> vertex_ref(SurfaceMesh<Scalar, Index>& mesh);

///
/// Returns a read-only view of the mesh vertices in the form of an Eigen matrix.
///
/// @param[in]  mesh    Mesh whose vertices to view.
///
/// @tparam     Scalar  Mesh scalar type.
/// @tparam     Index   Mesh index type.
///
/// @return     An Eigen::Map wrapping the attribute data.
///
template <typename Scalar, typename Index>
ConstRowMatrixView<Scalar> vertex_view(const SurfaceMesh<Scalar, Index>& mesh);

///
/// Returns a writable view of a mesh facets in the form of an Eigen matrix. This function only
/// works for regular meshes. If the mesh does not have a fixed facet size, an exception is thrown.
///
/// @param[in]  mesh    Mesh whose facets to view.
///
/// @tparam     Scalar  Mesh scalar type.
/// @tparam     Index   Mesh index type.
///
/// @return     An Eigen::Map wrapping the attribute data.
///
template <typename Scalar, typename Index>
RowMatrixView<Index> facet_ref(SurfaceMesh<Scalar, Index>& mesh);

///
/// Returns a read-only view of a mesh facets in the form of an Eigen matrix. This function only
/// works for regular meshes. If the mesh does not have a fixed facet size, an exception is thrown.
///
/// @param[in]  mesh    Mesh whose facets to view.
///
/// @tparam     Scalar  Mesh scalar type.
/// @tparam     Index   Mesh index type.
///
/// @return     An Eigen::Map wrapping the attribute data.
///
template <typename Scalar, typename Index>
ConstRowMatrixView<Index> facet_view(const SurfaceMesh<Scalar, Index>& mesh);

/// @}
/// @}

} // namespace lagrange
