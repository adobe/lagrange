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

#include <lagrange/scene/SimpleScene.h>

#include <Eigen/Geometry>

namespace lagrange::scene {

///
/// Compute the axis-aligned bounding box of all instantiated mesh vertices in a simple scene.
///
/// Each mesh is transformed independently by each of its instance transformations. Meshes without
/// instances do not contribute to the bounding box. If the scene contains no instantiated
/// vertices, the returned bounding box is empty.
///
/// @param[in]  scene  Input scene.
///
/// @tparam     Scalar     Scene scalar type.
/// @tparam     Index      Scene index type.
/// @tparam     Dimension  Spatial dimension of the scene. Must be 2 or 3.
///
/// @return     The axis-aligned bounding box of all transformed instance vertices.
///
template <typename Scalar, typename Index, size_t Dimension>
[[nodiscard]]
Eigen::AlignedBox<Scalar, static_cast<int>(Dimension)> simple_scene_bbox(
    const SimpleScene<Scalar, Index, Dimension>& scene);

} // namespace lagrange::scene
