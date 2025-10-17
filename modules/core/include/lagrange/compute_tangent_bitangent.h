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

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
    #include <lagrange/legacy/compute_tangent_bitangent.h>
#endif

#include <lagrange/SurfaceMesh.h>

#include <string_view>

namespace lagrange {

///
/// @defgroup   group-surfacemesh-utils Mesh utilities
/// @ingroup    group-surfacemesh
///
/// Various mesh processing utilities.
///
/// @{

///
/// Option struct for computing tangent and bitangent vectors.
///
struct TangentBitangentOptions
{
    /// Output tangent attribute name.
    std::string_view tangent_attribute_name = "@tangent";

    /// Output bitangent attribute name.
    std::string_view bitangent_attribute_name = "@bitangent";

    /// UV attribute name used to orient the BTN frame. Must be an indexed attribute. If left empty,
    /// the first indexed UV attribute is used.
    std::string_view uv_attribute_name = "";

    /// Normal attribute name used to compute the BTN frame. Must be an indexed attribute. The
    /// output tangent/bitangent vectors are projected into the plane orthogonal to the normal
    /// vector at each triangle corner. If the output element type is set to `Indexed`, the indices
    /// in the provided normal attribute are also used to aggregate BTN frames between neighboring
    /// triangles. If left empty, the first indexed Normal attribute is used.
    std::string_view normal_attribute_name = "";

    /// Output element type. Can be either Corner or Indexed.
    AttributeElement output_element_type = AttributeElement::Indexed;

    /// Whether to pad the tangent/bitangent vectors with a 4th coordinate indicating the sign of
    /// the UV triangle.
    bool pad_with_sign = false;

    /// Whether to compute the bitangent as sign * cross(normal, tangent)
    /// If false, the bitangent is computed as the derivative of v-coordinate
    bool orthogonalize_bitangent = false;

    /// Whether to recompute tangent if the tangent attribute (specified by tangent_attribute_name)
    /// already exists.
    /// * If true, bitangent will be computed by sign * normalized(cross(normal, existing_tangent))
    ///   - `orthogonalize_bitangent` must be true
    /// * If false, the tangent will be recomputed and potentially overwritten.
    bool keep_existing_tangent = false;
};

/// Result type of the compute_tangent_bitangent function.
struct TangentBitangentResult
{
    /// Tangent vector attribute id.
    AttributeId tangent_id = invalid_attribute_id();

    /// Bitangent vector attribute id.
    AttributeId bitangent_id = invalid_attribute_id();
};

///
/// Compute mesh tangent and bitangent vectors orthogonal to the input mesh normals.
///
/// @note       Unless `options.keep_existing_tangent` is true, the input mesh must have existing indexed normal and UV attributes.
///             The input UV attribute is used to orient the resulting T/B vectors coherently wrt to the UV mapping.
///
/// @param[in]  mesh     The input mesh.
/// @param[in]  options  Optional arguments to control tangent/bitangent generation.
///
/// @tparam     Scalar   Mesh scalar type.
/// @tparam     Index    Mesh index type.
///
/// @return     A struct containing the id of the generated tangent/bitangent attributes.
///
/// @see        @ref TangentBitangentOptions
///
template <typename Scalar, typename Index>
TangentBitangentResult compute_tangent_bitangent(
    SurfaceMesh<Scalar, Index>& mesh,
    TangentBitangentOptions options = {});

/// @}

} // namespace lagrange
