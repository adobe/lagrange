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

#include <lagrange/scene/SimpleScene.h>

#include <vector>

namespace lagrange::scene::internal {

///
/// Bake a uniform per-instance scaling facotr into the mesh transforms. In order to prevent any
/// numerical error when unbaking, we simply store the old transform data in the instance user data.
///
/// @param[in]  scene                 Scene to bake.
/// @param[in]  per_instance_scaling  Per instance scaling factor.
///
/// @tparam     Scalar                Scene scalar type.
/// @tparam     Index                 Scene index type.
/// @tparam     Dimension             Scene dimension.
///
/// @return     A new scene where each instance transform has been modified to account for the
///             provided scaling.
///
template <typename Scalar, typename Index, size_t Dimension>
SimpleScene<Scalar, Index, Dimension> bake_scaling(
    SimpleScene<Scalar, Index, Dimension> scene,
    const std::vector<float>& per_instance_scaling);

///
/// Unbake previously baked scaling factors from the scene instance transforms.
///
/// @param[in]  scene      Scene to unbake.
///
/// @tparam     Scalar     Scene scalar type.
/// @tparam     Index      Scene index type.
/// @tparam     Dimension  Scene dimension.
///
/// @return     A new scene where the instance transforms/user data have been restored to their
///             previous state.
///
template <typename Scalar, typename Index, size_t Dimension>
SimpleScene<Scalar, Index, Dimension> unbake_scaling(SimpleScene<Scalar, Index, Dimension> scene);

} // namespace lagrange::scene::internal
