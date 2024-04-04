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

#include <lagrange/ui/api.h>

#include <Eigen/Eigen>
#include <limits>

namespace lagrange {
namespace ui {

using RowMajorMatrixXf = Eigen::Matrix<float, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;
using RowMajorMatrixXi = Eigen::Matrix<int, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;

/// Returns 4x4 transformation matrix that can be used for normals
/// Performs transpose inverse
LA_UI_API Eigen::Matrix4f normal_matrix(const Eigen::Affine3f& transform);

/// Constructs perspective projection matrix
///
/// @param[in] fov_y    vertical field of vision in RADIANS
/// @param[in] aspect   aspect ratio
/// @param[in] zNear    near plane
/// @param[in] zFar     far plane
///
/// @return    Eigen::Projective3f      perspective projection matrix
LA_UI_API Eigen::Projective3f perspective(float fovy, float aspect, float zNear, float zFar);

/// Constructs orthograpic projection matrix
///
/// @param[in] left     left coordinate
/// @param[in] right    right coordinate
/// @param[in] bottom   bottom coordinate
/// @param[in] top      top coordinate
/// @param[in] zNear    near plane
/// @param[in] zFar     far plane
///
/// @return    Eigen::Projective3f      orthographic projection matrix
LA_UI_API Eigen::Projective3f
ortho(float left, float right, float bottom, float top, float zNear, float zFar);

/// Constructs "look at" view matrix,
///
/// @param[in] eye      camera position
/// @param[in] center   point to look at
/// @param[in] up       up direction
///
/// @return    Eigen::Matrix4f      view matrix
LA_UI_API Eigen::Matrix4f
look_at(const Eigen::Vector3f& eye, const Eigen::Vector3f& center, const Eigen::Vector3f& up);

/// Unprojects screen point (with x,y in screen coordinates and z in NDC) back to 3D world
///
/// @param[in] v            screen point with NDC z coordinate
/// @param[in] view         view matrix
/// @param[in] perspective  perspective matrix
/// @param[in] viewport     viewport coordinates (x0,y0,x1,y1), x0,y0 being lower left
///
/// @return    Eigen::Vector3f      unprojected 3D point in world space
LA_UI_API Eigen::Vector3f unproject_point(
    const Eigen::Vector3f& v,
    const Eigen::Matrix4f& view,
    const Eigen::Matrix4f& perspective,
    const Eigen::Vector4f& viewport);

LA_UI_API float pi();

/// 2 * pi
LA_UI_API float two_pi();

/// Projects vector onto 'onto' vector
LA_UI_API Eigen::Vector3f vector_projection(const Eigen::Vector3f& vector, const Eigen::Vector3f& onto);

/// Returns angle in radians between a and b
LA_UI_API float vector_angle(const Eigen::Vector3f& a, const Eigen::Vector3f& b);


} // namespace ui
} // namespace lagrange
