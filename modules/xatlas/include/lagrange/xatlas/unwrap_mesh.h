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

#include <lagrange/SurfaceMesh.h>
#include <lagrange/xatlas/Options.h>
#include <lagrange/xatlas/api.h>

#include <atomic>
#include <functional>
#include <string>

namespace lagrange::xatlas {

///
/// Unwrap a single mesh using xatlas.
///
/// @param[in]  mesh               Mesh to unwrap. Must be triangle-only.
/// @param[in]  options            Unwrap options.
/// @param[in]  notification_func  Optional progress callback.
/// @param[in]  cancel             Optional cancellation flag.
///
/// @tparam     Scalar             Scene mesh scalar type.
/// @tparam     Index              Scene mesh index type.
///
/// @return     Mesh with the new indexed UV attribute (and optional atlas/chart index attributes).
///
template <typename Scalar, typename Index>
LA_XATLAS_API SurfaceMesh<Scalar, Index> unwrap_mesh(
    const SurfaceMesh<Scalar, Index>& mesh,
    const UnwrapOptions& options = {},
    std::function<void(const std::string&, float)> notification_func = nullptr,
    const std::atomic_bool* cancel = nullptr);

} // namespace lagrange::xatlas
