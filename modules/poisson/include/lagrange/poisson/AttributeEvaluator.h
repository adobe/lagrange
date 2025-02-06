/*
 * Copyright 2024 Adobe. All rights reserved.
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
#include <lagrange/poisson/CommonOptions.h>

#include <array>
#include <memory>
#include <string_view>

namespace lagrange::poisson {

///
/// Option struct for Poisson surface reconstruction.
///
struct EvaluatorOptions : public CommonOptions
{
    /// Attribute name of data to be interpolated at the vertices
    std::string_view interpolated_attribute_name;
};

///
/// Attribute evaluator class. Once constructed, it allows interpolating a signal from point cloud
/// data.
///
class AttributeEvaluator
{
public:
    ///
    /// Constructs a new attribute evaluator for a point cloud.
    ///
    /// @param[in]  points   Point cloud to evaluate.
    /// @param[in]  options  Attribute evaluation option.
    ///
    /// @tparam     Scalar   Point cloud scalar type.
    /// @tparam     Index    Point cloud index type.
    ///
    template <typename Scalar, typename Index>
    AttributeEvaluator(
        const SurfaceMesh<Scalar, Index>& points,
        const EvaluatorOptions& options = {});

    ///
    /// Destroys the object.
    ///
    ~AttributeEvaluator();

    ///
    /// Evaluate the extrapolated attribute at any point in 3D space.
    ///
    /// @param[in]  pos        Position to evalute at.
    /// @param[in]  out        Output buffer to write the evaluated attribute values.
    ///
    /// @tparam     Scalar     Point coordinate scalar type.
    /// @tparam     ValueType  Attribute value type.
    ///
    template <typename Scalar, typename ValueType>
    void eval(span<const Scalar> pos, span<ValueType> out) const;

private:
    struct Impl;

    // Opaque implementation (non-copyable)
    std::unique_ptr<Impl> m_impl;
};

/// @}

} // namespace lagrange::poisson
