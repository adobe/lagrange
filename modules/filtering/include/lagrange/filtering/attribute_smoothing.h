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
#include <lagrange/filtering/api.h>

#include <string_view>

namespace lagrange::filtering {

/**
 * Options for controlling the attribute smoothing process.
 */
struct AttributeSmoothingOptions
{
    /**
     * Weight factor for curvature-based smoothing.
     * 
     * Controls the strength of the smoothing operation. Higher values result in
     * stretching in the surface metric, slowing down diffusion process.
     * The default value of 0.02 provides a moderate smoothing effect.
     * Values should typically be in the range [0.0, 1.0].
     */
    double curvature_weight = 0.02;

    /**
     * Weight factor for normal-based smoothing.
     * 
     * Controls the influence of normal-based smoothing on the attribute values.
     * Higher values result in stronger smoothing along the surface normal direction.
     * The default value of 1e-4 provides a subtle normal-based smoothing effect.
     */
    double normal_smoothing_weight = 1e-4;

    /**
     * Weight factor for gradient-based smoothing.
     * 
     * Controls the strength of gradient-based smoothing operations.
     * Higher values result in stronger smoothing of attribute gradients.
     * The default value of 1e-4 provides a moderate gradient smoothing effect.
     */
    double gradient_weight = 1e-4;

    /**
     * Scale factor for gradient modulation.
     * 
     * Controls how much the attribute gradients are modulated during smoothing.
     * A value of 0.0 (default) means no gradient modulation is applied.
     * Positive values increase gradient modulation, while negative values decrease it.
     */
    double gradient_modulation_scale = 0.;
};

/**
 * Smooths a scalar attribute on a surface mesh.
 * 
 * This function applies a smoothing operation to a specified scalar attribute
 * on the mesh. The smoothing algorithm uses a curvature-weighted approach to
 * preserve important features while reducing noise.
 * 
 * @tparam Scalar The scalar type used for mesh coordinates and attributes.
 * @tparam Index The index type used for mesh connectivity.
 * 
 * @param mesh           The surface mesh containing the attribute to smooth.
 * @param attribute_name The name of the scalar vertex attribute to smooth. If empty, all attributes
 *                       with scalar usage and vertex element type will be smoothed. The attribute 
 *                       will be modified in place. If an attribute with `Vector` usage is
 *                       specified, it is treated as a multi-channel scalar attribute, where each
 *                       channel will be smoothed independently.
 * @param options        Configuration options for the smoothing operation.
 */
template <typename Scalar, typename Index>
LA_FILTERING_API void scalar_attribute_smoothing(
    SurfaceMesh<Scalar, Index>& mesh,
    std::string_view attribute_name = "",
    const AttributeSmoothingOptions& options = {});

} // namespace lagrange::filtering
