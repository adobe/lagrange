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
#include <lagrange/views.h>

#include <lagrange/Attribute.h>
#include <lagrange/AttributeTypes.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/utils/assert.h>

namespace lagrange {

////////////////////////////////////////////////////////////////////////////////
// Generic attribute views
////////////////////////////////////////////////////////////////////////////////

template <typename ValueType>
RowMatrixView<ValueType> matrix_ref(Attribute<ValueType>& attribute)
{
    return {
        attribute.ref_all().data(),
        Eigen::Index(attribute.get_num_elements()),
        Eigen::Index(attribute.get_num_channels())};
}

template <typename ValueType>
ConstRowMatrixView<ValueType> matrix_view(const Attribute<ValueType>& attribute)
{
    return {
        attribute.get_all().data(),
        Eigen::Index(attribute.get_num_elements()),
        Eigen::Index(attribute.get_num_channels())};
}

template <typename ValueType>
VectorView<ValueType> vector_ref(Attribute<ValueType>& attribute)
{
    la_runtime_assert(attribute.get_num_channels() == 1);
    return {attribute.ref_all().data(), Eigen::Index(attribute.get_num_elements())};
}

template <typename ValueType>
ConstVectorView<ValueType> vector_view(const Attribute<ValueType>& attribute)
{
    la_runtime_assert(attribute.get_num_channels() == 1);
    return {attribute.get_all().data(), Eigen::Index(attribute.get_num_elements())};
}

template <typename ValueType>
RowMatrixView<ValueType> reshaped_ref(Attribute<ValueType>& attribute, size_t num_cols)
{
    if (attribute.empty()) {
        return {attribute.ref_all().data(), 0, Eigen::Index(num_cols)};
    }
    la_runtime_assert(num_cols != 0 && attribute.get_all().size() % num_cols == 0);
    return {
        attribute.ref_all().data(),
        Eigen::Index(attribute.get_all().size() / num_cols),
        Eigen::Index(num_cols)};
}

template <typename ValueType>
ConstRowMatrixView<ValueType> reshaped_view(const Attribute<ValueType>& attribute, size_t num_cols)
{
    if (attribute.empty()) {
        return {attribute.get_all().data(), 0, Eigen::Index(num_cols)};
    }
    la_runtime_assert(num_cols != 0 && attribute.get_all().size() % num_cols == 0);
    return {
        attribute.get_all().data(),
        Eigen::Index(attribute.get_all().size() / num_cols),
        Eigen::Index(num_cols)};
}

////////////////////////////////////////////////////////////////////////////////
// Generic attribute views (mesh)
////////////////////////////////////////////////////////////////////////////////

template <typename ValueType, typename Scalar, typename Index>
RowMatrixView<ValueType> attribute_matrix_ref(
    SurfaceMesh<Scalar, Index>& mesh,
    std::string_view name)
{
    return matrix_ref(mesh.template ref_attribute<ValueType>(name));
}

template <typename ValueType, typename Scalar, typename Index>
ConstRowMatrixView<ValueType> attribute_matrix_view(
    const SurfaceMesh<Scalar, Index>& mesh,
    std::string_view name)
{
    return matrix_view(mesh.template get_attribute<ValueType>(name));
}

template <typename ValueType, typename Scalar, typename Index>
VectorView<ValueType> attribute_vector_ref(SurfaceMesh<Scalar, Index>& mesh, std::string_view name)
{
    return vector_ref(mesh.template ref_attribute<ValueType>(name));
}

template <typename ValueType, typename Scalar, typename Index>
ConstVectorView<ValueType> attribute_vector_view(
    const SurfaceMesh<Scalar, Index>& mesh,
    std::string_view name)
{
    return vector_view(mesh.template get_attribute<ValueType>(name));
}

template <typename ValueType, typename Scalar, typename Index>
RowMatrixView<ValueType> attribute_matrix_ref(SurfaceMesh<Scalar, Index>& mesh, AttributeId id)
{
    return matrix_ref(mesh.template ref_attribute<ValueType>(id));
}

template <typename ValueType, typename Scalar, typename Index>
ConstRowMatrixView<ValueType> attribute_matrix_view(
    const SurfaceMesh<Scalar, Index>& mesh,
    AttributeId id)
{
    return matrix_view(mesh.template get_attribute<ValueType>(id));
}

template <typename ValueType, typename Scalar, typename Index>
VectorView<ValueType> attribute_vector_ref(SurfaceMesh<Scalar, Index>& mesh, AttributeId id)
{
    return vector_ref(mesh.template ref_attribute<ValueType>(id));
}

template <typename ValueType, typename Scalar, typename Index>
ConstVectorView<ValueType> attribute_vector_view(
    const SurfaceMesh<Scalar, Index>& mesh,
    AttributeId id)
{
    return vector_view(mesh.template get_attribute<ValueType>(id));
}

////////////////////////////////////////////////////////////////////////////////
// Specific attribute views
////////////////////////////////////////////////////////////////////////////////

template <typename Scalar, typename Index>
RowMatrixView<Scalar> vertex_ref(SurfaceMesh<Scalar, Index>& mesh)
{
    return matrix_ref(mesh.ref_vertex_to_position());
}

template <typename Scalar, typename Index>
ConstRowMatrixView<Scalar> vertex_view(const SurfaceMesh<Scalar, Index>& mesh)
{
    return matrix_view(mesh.get_vertex_to_position());
}

template <typename Scalar, typename Index>
RowMatrixView<Index> facet_ref(SurfaceMesh<Scalar, Index>& mesh)
{
    la_runtime_assert(mesh.is_regular());
    auto num_vpf = static_cast<size_t>(mesh.get_vertex_per_facet());
    return reshaped_ref(mesh.ref_corner_to_vertex(), num_vpf);
}

template <typename Scalar, typename Index>
ConstRowMatrixView<Index> facet_view(const SurfaceMesh<Scalar, Index>& mesh)
{
    la_runtime_assert(mesh.is_regular());
    auto num_vpf = static_cast<size_t>(mesh.get_vertex_per_facet());
    return reshaped_view(mesh.get_corner_to_vertex(), num_vpf);
}

////////////////////////////////////////////////////////////////////////////////
// Explicit template instantiations
////////////////////////////////////////////////////////////////////////////////

// Iterate over attribute types
#define LA_X_views_attr(_, ValueType)                                                          \
    template LA_CORE_API RowMatrixView<ValueType> matrix_ref(Attribute<ValueType>& attribute); \
    template LA_CORE_API ConstRowMatrixView<ValueType> matrix_view(                            \
        const Attribute<ValueType>& attribute);                                                \
    template LA_CORE_API VectorView<ValueType> vector_ref(Attribute<ValueType>& attribute);    \
    template LA_CORE_API ConstVectorView<ValueType> vector_view(                               \
        const Attribute<ValueType>& attribute);                                                \
    template LA_CORE_API RowMatrixView<ValueType> reshaped_ref(                                \
        Attribute<ValueType>& attribute,                                                       \
        size_t num_cols);                                                                      \
    template LA_CORE_API ConstRowMatrixView<ValueType> reshaped_view(                          \
        const Attribute<ValueType>& attribute,                                                 \
        size_t num_cols);
LA_ATTRIBUTE_X(views_attr, 0)

// Iterate over attribute types x mesh (scalar, index) types
#define LA_X_views_mesh_attr(ValueType, Scalar, Index)                        \
    template LA_CORE_API RowMatrixView<ValueType> attribute_matrix_ref(       \
        SurfaceMesh<Scalar, Index>& mesh,                                     \
        std::string_view name);                                               \
    template LA_CORE_API ConstRowMatrixView<ValueType> attribute_matrix_view( \
        const SurfaceMesh<Scalar, Index>& mesh,                               \
        std::string_view name);                                               \
    template LA_CORE_API VectorView<ValueType> attribute_vector_ref(          \
        SurfaceMesh<Scalar, Index>& mesh,                                     \
        std::string_view name);                                               \
    template LA_CORE_API ConstVectorView<ValueType> attribute_vector_view(    \
        const SurfaceMesh<Scalar, Index>& mesh,                               \
        std::string_view name);                                               \
    template LA_CORE_API RowMatrixView<ValueType> attribute_matrix_ref(       \
        SurfaceMesh<Scalar, Index>& mesh,                                     \
        AttributeId id);                                                      \
    template LA_CORE_API ConstRowMatrixView<ValueType> attribute_matrix_view( \
        const SurfaceMesh<Scalar, Index>& mesh,                               \
        AttributeId id);                                                      \
    template LA_CORE_API VectorView<ValueType> attribute_vector_ref(          \
        SurfaceMesh<Scalar, Index>& mesh,                                     \
        AttributeId id);                                                      \
    template LA_CORE_API ConstVectorView<ValueType> attribute_vector_view(    \
        const SurfaceMesh<Scalar, Index>& mesh,                               \
        AttributeId id);
#define LA_X_views_aux(_, ValueType) LA_SURFACE_MESH_X(views_mesh_attr, ValueType)
LA_ATTRIBUTE_X(views_aux, 0)

// Iterate over mesh (scalar, index) types
#define LA_X_views_mesh(_, Scalar, Index)                                                    \
    template LA_CORE_API RowMatrixView<Scalar> vertex_ref(SurfaceMesh<Scalar, Index>& mesh); \
    template LA_CORE_API ConstRowMatrixView<Scalar> vertex_view(                             \
        const SurfaceMesh<Scalar, Index>& mesh);                                             \
    template LA_CORE_API RowMatrixView<Index> facet_ref(SurfaceMesh<Scalar, Index>& mesh);   \
    template LA_CORE_API ConstRowMatrixView<Index> facet_view(                               \
        const SurfaceMesh<Scalar, Index>& mesh);
LA_SURFACE_MESH_X(views_mesh, 0)

} // namespace lagrange
