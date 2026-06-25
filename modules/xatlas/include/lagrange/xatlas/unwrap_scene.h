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
#include <lagrange/xatlas/Options.h>
#include <lagrange/xatlas/api.h>

#include <atomic>
#include <functional>
#include <string>

namespace lagrange::xatlas {

///
/// Unwrap all meshes in a scene using xatlas.
///
/// Each mesh (when sharing UVs) or each instance (when not sharing) is unwrapped independently as
/// its own xatlas atlas.
///
/// @param[in]  scene             Input scene.
/// @param[in]  unwrap_options    Per-mesh unwrap options.
/// @param[in]  scene_options     Scene-specific options.
/// @param[in]  notification_func Optional progress callback.
/// @param[in]  cancel            Optional cancellation flag.
///
/// @tparam     Scalar             Scene mesh scalar type.
/// @tparam     Index              Scene mesh index type.
///
/// @return     A new scene with the same instance topology and new indexed UV attributes.
///
template <typename Scalar, typename Index>
LA_XATLAS_API scene::SimpleScene<Scalar, Index, 3> unwrap_scene(
    const scene::SimpleScene<Scalar, Index, 3>& scene,
    const UnwrapOptions& unwrap_options = {},
    const SceneOptions& scene_options = {},
    std::function<void(const std::string&, float)> notification_func = nullptr,
    const std::atomic_bool* cancel = nullptr);

} // namespace lagrange::xatlas
