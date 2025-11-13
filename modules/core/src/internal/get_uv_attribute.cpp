/*
 * Copyright 2025 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/Attribute.h>
#include <lagrange/IndexedAttribute.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/internal/find_attribute_utils.h>
#include <lagrange/internal/get_uv_attribute.h>
#include <lagrange/utils/assert.h>
#include <lagrange/views.h>

namespace lagrange::internal {

template <typename Scalar, typename Index>
AttributeId get_uv_id(const SurfaceMesh<Scalar, Index>& mesh, std::string_view uv_attribute_name)
{
    AttributeId uv_attr_id;
    if (uv_attribute_name.empty()) {
        uv_attr_id = internal::find_matching_attribute<Scalar>(
            mesh,
            "",
            AttributeElement::Indexed,
            AttributeUsage::UV,
            2);

        if (uv_attr_id == invalid_attribute_id()) {
            // No indexed UV attribute found. Try to find a vertex attribute instead.
            uv_attr_id = internal::find_matching_attribute<Scalar>(
                mesh,
                "",
                AttributeElement::Vertex,
                AttributeUsage::UV,
                2);
        }
    } else {
        uv_attr_id = mesh.get_attribute_id(uv_attribute_name);
    }
    return uv_attr_id;
}

template <typename Scalar, typename Index>
std::tuple<ConstRowMatrixView<Scalar>, ConstVectorView<Index>> get_uv_attribute(
    const SurfaceMesh<Scalar, Index>& mesh,
    std::string_view uv_attribute_name)
{
    AttributeId uv_attr_id = get_uv_id(mesh, uv_attribute_name);
    la_runtime_assert(uv_attr_id != invalid_attribute_id(), "No UV attribute found.");

    if (mesh.is_attribute_indexed(uv_attr_id)) {
        const auto& uv_attr = mesh.template get_indexed_attribute<Scalar>(uv_attr_id);
        auto uv_values = matrix_view(uv_attr.values());
        auto uv_indices = vector_view(uv_attr.indices());
        return {uv_values, uv_indices};
    } else {
        const auto& uv_attr = mesh.template get_attribute<Scalar>(uv_attr_id);
        la_runtime_assert(
            uv_attr.get_element_type() == AttributeElement::Vertex,
            "UV attribute must be a vertex attribute.");
        auto uv_values = matrix_view(uv_attr);
        auto uv_indices = vector_view(mesh.get_corner_to_vertex());
        return {uv_values, uv_indices};
    }
}

template <typename Scalar, typename Index>
std::tuple<RowMatrixView<Scalar>, VectorView<Index>> ref_uv_attribute(
    SurfaceMesh<Scalar, Index>& mesh,
    std::string_view uv_attribute_name)
{
    AttributeId uv_attr_id = get_uv_id(mesh, uv_attribute_name);
    la_runtime_assert(uv_attr_id != invalid_attribute_id(), "No UV attribute found.");

    if (mesh.is_attribute_indexed(uv_attr_id)) {
        auto& uv_attr = mesh.template ref_indexed_attribute<Scalar>(uv_attr_id);
        auto uv_values = matrix_ref(uv_attr.values());
        auto uv_indices = vector_ref(uv_attr.indices());
        return {uv_values, uv_indices};
    } else {
        auto& uv_attr = mesh.template ref_attribute<Scalar>(uv_attr_id);
        la_runtime_assert(
            uv_attr.get_element_type() == AttributeElement::Vertex,
            "UV attribute must be a vertex attribute.");
        auto uv_values = matrix_ref(uv_attr);
        auto uv_indices = vector_ref(mesh.ref_corner_to_vertex());
        return {uv_values, uv_indices};
    }
}

#define LA_X_get_uv_attribute(_, Scalar, Index)                                           \
    template LA_CORE_API AttributeId get_uv_id<Scalar, Index>(                            \
        const SurfaceMesh<Scalar, Index>&,                                                \
        std::string_view);                                                                \
    template LA_CORE_API std::tuple<ConstRowMatrixView<Scalar>, ConstVectorView<Index>>   \
    get_uv_attribute<Scalar, Index>(const SurfaceMesh<Scalar, Index>&, std::string_view); \
    template LA_CORE_API std::tuple<RowMatrixView<Scalar>, VectorView<Index>>             \
    ref_uv_attribute<Scalar, Index>(SurfaceMesh<Scalar, Index>&, std::string_view);
LA_SURFACE_MESH_X(get_uv_attribute, 0)

} // namespace lagrange::internal
