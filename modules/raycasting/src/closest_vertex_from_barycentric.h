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

namespace lagrange::raycasting {

/// Determine which vertex of a triangle the barycentric coordinates are closest to.
/// Barycentric coordinates (u, v) represent: p = (1-u-v)*v0 + u*v1 + v*v2.
/// Returns 0, 1, or 2 for the three vertices of the triangle.
inline int closest_vertex_from_barycentric(float u, float v)
{
    float w = 1.0f - u - v;
    if (w >= u && w >= v) {
        return 0;
    } else if (u >= v) {
        return 1;
    } else {
        return 2;
    }
}

} // namespace lagrange::raycasting
