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
#include <lagrange/image/Array3D.h>
#include <lagrange/image/View3D.h>

#include <Eigen/Geometry>

namespace lagrange::texproc {

/// @addtogroup module-texproc
/// @{

///
/// Parameters for computing the rendering of a mesh.
///
struct CameraOptions
{
    /// Camera view transform (world space -> view space).
    Eigen::Affine3f view_transform = Eigen::Affine3f::Identity();

    /// Camera projection transform (view space -> NDC space).
    ///
    /// This is the standard glTF/OpenGL projection matrix, where depth is remapped to [-1, 1] (near
    /// plane to -1, far plane to 1).
    Eigen::Projective3f projection_transform = Eigen::Projective3f::Identity();
};

///
/// Options for computing the texture map and confidence from a rendering.
///
struct TextureRasterizerOptions
{
    /// Erosion radius (in texels).
    unsigned int depth_discontinuity_erosion_radius = 20;

    /// Depth discontinuity threshold (in view space).
    double depth_discontinuity_threshold = 1;

    /// Depth precision (for determining visibility).
    ///
    /// Ideally we'd use raytracing to determine if a texel is visible. But for simplicity we just
    /// compare the texel depth with depth map value at the screen location.
    double depth_precision = 1e-3;

    /// Texture width.
    size_t width = 1024;

    /// Texture height.
    size_t height = 1024;
};

///
/// Given a mesh with UVs, unproject rendered images into a UV texture and confidence map.
///
/// @tparam     Scalar  Mesh scalar type.
/// @tparam     Index   Mesh index type.
///
template <typename Scalar, typename Index>
class TextureRasterizer
{
public:
    using Array3Df = image::experimental::Array3D<float>;

public:
    ///
    /// Construct a new instance of the rasterizer.
    ///
    /// @param[in]  mesh     Input mesh.
    /// @param[in]  options  Texture and confidence options.
    ///
    TextureRasterizer(
        const SurfaceMesh<Scalar, Index>& mesh,
        const TextureRasterizerOptions& options);

    ///
    /// Destructor.
    ///
    ~TextureRasterizer();

    ///
    /// Unproject a rendered image into a UV texture and confidence map.
    ///
    /// @param[in]  image    Input rendered color image.
    /// @param[in]  options  Camera option.
    ///
    /// @return     A pair of (texture, weight) images.
    ///
    std::pair<Array3Df, Array3Df> weighted_texture_from_render(
        image::experimental::View3D<const float> image,
        const CameraOptions& options) const;

private:
    /// @cond LA_INTERNAL_DOCS

    struct Impl;
    value_ptr<Impl> m_impl;

    /// @endcond
};

///
/// Discard low-confidence values. Texels whose weight is < threshold * max_weight are set to zero.
///
/// @param[in,out] textures_and_weights  A list of compute texture and confidence images.
/// @param[in]     low_ratio_threshold   Threshold for discarding low-confidence texels.
///
void filter_low_confidences(
    span<std::pair<image::experimental::Array3D<float>, image::experimental::Array3D<float>>>
        textures_and_weights,
    float low_ratio_threshold);

/// @}

} // namespace lagrange::texproc
