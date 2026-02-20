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

#include <lagrange/internal/constants.h>
#include <array>
#include <cmath>
#include <string_view>

namespace lagrange::primitive {

/// @addtogroup module-primitive
/// @{

///
/// Common settings shared by all primitives
///
struct PrimitiveOptions
{
    using Scalar = float;

    ///
    /// Center of the primitive in 3D space.
    ///
    /// @note The center is not necessarily the mesh centroid or bounding box center due to the
    /// nature of parametric primitive. The center is invariant to the parameter changes.
    /// * For torus, the center is the center of the torus ring.
    /// * For sphere, the center is the center of the sphere.
    /// * For cylinder, the center is the center of the bottom cap of the cylinder.
    /// * For cone, the center is the center of the bottom cap of the cone.
    ///
    std::array<Scalar, 3> center{0, 0, 0};

    /// Whether to generate top cap of the primitive (if applicable).
    bool with_top_cap = true;

    /// Whether to generate bottom cap of the primitive (if applicable).
    bool with_bottom_cap = true;

    /// Whether to generate cross section of the primitive (if applicable).
    bool with_cross_section = true;

    /// Whether to triangulate the generated surface mesh.
    bool triangulate = false;

    /// Whether to use fixed UV coordinates regardless of the primitive parameters.
    bool fixed_uv = false;

    /// Name of the output indexed attribute storing the normal vectors.
    std::string_view normal_attribute_name = "@normal";

    /// Name of the output indexed attribute storing the UV coordinates.
    std::string_view uv_attribute_name = "@uv";

    /// Name of the output facet attribute storing the semantic labels.
    std::string_view semantic_label_attribute_name = "@semantic_label";

    /// Two vertices are considered coinciding if the distance between them is
    /// smaller than `dist_threshold`.
    Scalar dist_threshold = static_cast<Scalar>(1e-6);

    /// An edge is considered sharp if its dihedral angle is larger than
    /// `angle_threshold`.
    Scalar angle_threshold = static_cast<Scalar>(30 * lagrange::internal::pi / 180);

    /// Numerical tolerance used for comparing Scalar values.
    Scalar epsilon = static_cast<Scalar>(1e-6);

    /// Padding size for UV charts to avoid bleeding.
    Scalar uv_padding = static_cast<Scalar>(0.005);
};

/// @}

} // namespace lagrange::primitive
