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

#include <lagrange/ui/types/Camera.h>

#include <Eigen/Eigen>

namespace lagrange {
namespace ui {

/// Affine transformation
struct Transform
{
    /// Transformation in entity's space (model transform)
    Eigen::Affine3f local = Eigen::Affine3f::Identity();

    /// Transformation to world space (hieararchical)
    Eigen::Affine3f global = Eigen::Affine3f::Identity();

    /// TODO: extract as its own entity
    Camera::ViewportTransform viewport = Camera::ViewportTransform();
};


} // namespace ui
} // namespace lagrange
