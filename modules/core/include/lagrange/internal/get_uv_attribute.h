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
#pragma once

#include <lagrange/SurfaceMesh.h>
#include <lagrange/utils/Error.h>
#include <lagrange/utils/fmt/format.h>
#include <lagrange/uv_mesh.h>
#include <lagrange/views.h>

#include <string_view>
#include <tuple>
#include <type_traits>

namespace lagrange::internal {

/// Behavior when a named UV attribute has a mismatched scalar type.
enum class TypeMismatchPolicy {
    Assert, ///< Assert on type mismatch (default, for strict callers).
    Graceful, ///< Return invalid_attribute_id() on type mismatch (for dual-dispatch probing).
};

///
/// Get the ID of the UV attribute of a mesh.
///
/// @param      mesh               The mesh to get the UV attribute from.
/// @param      uv_attribute_name  The name of the UV attribute. If empty, use the first indexed or
///                                vertex UV attribute or, if element_types is set to
///                                UVMeshOptions::ElementTypes::All, the first corner attribute.
/// @param      element_types      Supported element types for the UV attribute lookup.
/// @param      type_mismatch      Policy for handling scalar type mismatches on named attributes.
///
/// @tparam     Scalar             Mesh scalar type.
/// @tparam     Index              Mesh index type.
/// @tparam     UVScalar           Target UV attribute value type.
///
/// @return     The ID of the UV attribute.
///
template <typename Scalar, typename Index, typename UVScalar = Scalar>
AttributeId get_uv_id(
    const SurfaceMesh<Scalar, Index>& mesh,
    std::string_view uv_attribute_name = "",
    UVMeshOptions::ElementTypes element_types = UVMeshOptions::ElementTypes::IndexedOrVertex,
    TypeMismatchPolicy type_mismatch = TypeMismatchPolicy::Assert);

///
/// Get the constant UV attribute buffers of a mesh.
///
/// @param      mesh               The mesh to get the UV attribute from.
/// @param      uv_attribute_name  The name of the UV attribute. If empty, use the first indexed or
///                                vertex UV attribute.
///
/// @tparam     Scalar             Mesh scalar type.
/// @tparam     Index              Mesh index type.
/// @tparam     UVScalar           Target attribute value type.
///
/// @return     A tuple containing the UV values and indices.
///
template <typename Scalar, typename Index, typename UVScalar = Scalar>
std::tuple<ConstRowMatrixView<UVScalar>, ConstVectorView<Index>> get_uv_attribute(
    const SurfaceMesh<Scalar, Index>& mesh,
    std::string_view uv_attribute_name = "");

///
/// Get the modifiable UV attribute buffers of a mesh.
///
/// @param      mesh               The mesh to get the UV attribute from.
/// @param      uv_attribute_name  The name of the UV attribute. If empty, use the first indexed or
///                                vertex UV attribute.
///
/// @tparam     Scalar             Mesh scalar type.
/// @tparam     Index              Mesh index type.
/// @tparam     UVScalar           Target attribute value type.
///
/// @return     A tuple containing the UV values and indices.
///
template <typename Scalar, typename Index, typename UVScalar = Scalar>
std::tuple<RowMatrixView<UVScalar>, VectorView<Index>> ref_uv_attribute(
    SurfaceMesh<Scalar, Index>& mesh,
    std::string_view uv_attribute_name = "");

/// Tag carrying a UV scalar type, used to disambiguate the type passed back to
/// `dispatch_uv_scalar_type` callers without relying on C++20 template lambdas.
template <typename T>
struct UVScalarTag
{
    using type = T;
};

///
/// Dispatch on the scalar type of a UV attribute.
///
/// Locates the (indexed or vertex) UV attribute on @p mesh, then invokes @p visitor with a
/// `UVScalarTag<UVScalar>` and the resolved attribute id. UVScalar is either the mesh scalar
/// type, or its float/double counterpart if the UV attribute uses the other type.
///
/// @param      mesh     The mesh to look up the UV attribute on.
/// @param      options  UV attribute lookup options.
/// @param      caller   Name of the calling function, used in the error message when no UV
///                      attribute is found.
/// @param      visitor  A callable of the form
///                      `auto(UVScalarTag<UVScalar>, AttributeId) -> R`.
///
/// @tparam     Scalar   Mesh scalar type.
/// @tparam     Index    Mesh index type.
/// @tparam     F        Visitor type.
///
/// @return     Whatever @p visitor returns.
///
template <typename Scalar, typename Index, typename F>
auto dispatch_uv_scalar_type(
    SurfaceMesh<Scalar, Index>& mesh,
    const UVMeshOptions& options,
    std::string_view caller,
    F&& visitor)
{
    using OtherScalar = std::conditional_t<std::is_same_v<Scalar, float>, double, float>;
    if (auto id = uv_attribute_id<Scalar, Index, Scalar>(mesh, options)) {
        return visitor(UVScalarTag<Scalar>{}, *id);
    }
    if (auto id = uv_attribute_id<Scalar, Index, OtherScalar>(mesh, options)) {
        return visitor(UVScalarTag<OtherScalar>{}, *id);
    }
    throw Error(format("{}: no suitable UV attribute found.", caller));
}

} // namespace lagrange::internal
