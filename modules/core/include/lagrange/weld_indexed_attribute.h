/*
 * Copyright 2023 Adobe. All rights reserved.
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
#include <lagrange/utils/function_ref.h>

#include <optional>

namespace lagrange {

///
/// @defgroup   group-surfacemesh-attr-utils Attributes utilities
/// @ingroup    group-surfacemesh
///
/// Various attribute processing utilities
///
/// @{

/// Options for welding indexed attributes
struct WeldOptions
{
    /// If set, values whose relative difference (in L-inf norm) is less than epsilon are merged
    /// together. If not set, default to `Eigen::NumTraits<ValueType>::dummy_precision()`.
    std::optional<double> epsilon_rel;

    /// If set, values whose absolute difference (in L-inf norm) is less than epsilon are merged
    /// together. If not set, default to `Eigen::NumTraits<ValueType>::epsilon()`.
    std::optional<double> epsilon_abs;

    /// If set, the absolute angle threshold (in radians) for merging attributes. If the angle
    /// between two corners is less than this value, the attributes are merged. If not set, this
    /// condition is not checked.
    ///
    /// @note Angle is computed as @f$ arccos(v1 \cdot v2 / (\|v1\| * \|v2\|)) $f@, where v1 and v2
    /// are the corner attribute values. It is well defined for all dimensions. The angle check is
    /// robust against degeneracies (e.g. zero-length vectors).
    ///
    /// @note Angle check is done in conjunction with the relative and absolute precision checks.
    /// To only check the angle, set `epsilon_rel` to 0 and `epsilon_abs` to
    /// `std::numeric_limits<double>::infinity()`.
    std::optional<double> angle_abs;

    /// If indices are shared between corners around different vertices, merge them together.
    /// Otherwise, separate indices will be created for each group of welded corners around each
    /// vertex.
    bool merge_across_vertices = false;

    /// Vertices to exclude from welding.
    ///
    /// This is mainly useful for cone singularities where welding similar attribute values may not
    /// be desirable.
    span<const size_t> exclude_vertices = {};
};

///
/// Weld an indexed attribute by combining all corners around a vertex with the same attribute
/// value.
///
/// @tparam Scalar    The mesh scalar type.
/// @tparam Index     The mesh index type.
///
/// @param mesh       The source mesh.
/// @param attr_id    The indexed attribute id.
///
template <typename Scalar, typename Index>
void weld_indexed_attribute(
    SurfaceMesh<Scalar, Index>& mesh,
    AttributeId attr_id,
    const WeldOptions& options = {});

/// @}

} // namespace lagrange
