/*
 * Copyright 2026 Adobe. All rights reserved.
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

#include <Eigen/Geometry>

namespace lagrange {

///
/// View and projection matrices defining a camera in world space.
///
struct CameraTransforms
{
    /// Camera view transform (world space -> view space).
    Eigen::Affine3f view = Eigen::Affine3f::Identity();

    /// Camera projection transform (view space -> NDC space).
    ///
    /// This is the standard glTF/OpenGL projection matrix, where depth is remapped to [-1, 1] (near
    /// plane to -1, far plane to 1).
    Eigen::Projective3f projection = Eigen::Projective3f::Identity();
};

} // namespace lagrange
