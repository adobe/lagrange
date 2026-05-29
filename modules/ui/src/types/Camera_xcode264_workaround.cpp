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

// Workaround for Xcode 26.4 compiler bug
// The VectorCombine pass in clang 21.0.0 crashes when compiling this function
// for x86_64 with optimization enabled. Disabling optimizations for this file
// prevents the crash. This works across all build systems (CMake, Make, etc.)
//
// Bug: VectorCombine::foldSelectShuffle() segfaults on (r_0 * r_arc).inverse()
// Affects: Apple Clang (Xcode 26.4, __apple_build_version__ 21000000-21999999), x86_64 only
//
// Why this function is in a separate file with file-level #pragma:
// - Localized #pragma around the function (function-level) does NOT work
// - The crash happens during Eigen template instantiation, which occurs during
//   the compiler's code generation phase, BEFORE function-level optimization
//   directives are applied
// - The VectorCombine pass runs at the translation-unit level, not function level
// - Therefore, the #pragma must be at FILE level to disable the pass for the
//   entire compilation unit containing the problematic template instantiations
//
// TODO: Remove this file when Apple fixes the bug in future Xcode release

#include <lagrange/utils/build.h>

// Only disable optimizations for the specific problematic configuration
// Note: __apple_build_version__ is only defined by Apple Clang, not Homebrew LLVM
#if LAGRANGE_TARGET_OS(APPLE) && LAGRANGE_TARGET_PLATFORM(x86_64) &&           \
    defined(__apple_build_version__) && __apple_build_version__ >= 21000000 && \
    __apple_build_version__ < 22000000
    #pragma clang optimize off
#endif

#include <lagrange/ui/types/Camera.h>
#include <lagrange/ui/utils/math.h>

#include <cmath>
namespace lagrange {
namespace ui {

void Camera::rotate_arcball(
    const Eigen::Vector3f& camera_pos_start,
    const Eigen::Vector3f& camera_up_start,
    const Eigen::Vector2f& mouse_start,
    const Eigen::Vector2f& mouse_current)
{
    // No change of camera
    if (mouse_start.x() == mouse_current.x() && mouse_start.y() == mouse_current.y()) return;

    const auto map_to_sphere = [&](const Eigen::Vector2f& pos) -> Eigen::Vector3f {
        Eigen::Vector3f p = Eigen::Vector3f::Zero();
        // Map to fullscreen ellipse
        p.x() = (2 * pos.x() / get_window_width() - 1.0f);
        p.y() = (2 * (pos.y() / get_window_height()) - 1.0f);

        float lensq = p.x() * p.x() + p.y() * p.y();

        if (lensq <= 1.0f) {
            p.z() = std::sqrt(1 - lensq);
        } else {
            p = p.normalized();
        }
        return p;
    };

    const auto decompose =
        [](const Eigen::Matrix4f& m, Eigen::Vector3f& T, Eigen::Matrix3f& R, Eigen::Vector3f& S) {
            T = m.col(3).head<3>();
            S = Eigen::Vector3f((m.col(0).norm()), (m.col(1).norm()), (m.col(2).norm()));
            R.col(0) = m.col(0).head<3>() * (1.0f / S(0));
            R.col(1) = m.col(1).head<3>() * (1.0f / S(1));
            R.col(2) = m.col(2).head<3>() * (1.0f / S(2));
        };

    // Calculate points on sphere and rotation axis/angle
    const Eigen::Vector3f p0 = map_to_sphere(mouse_start);
    const Eigen::Vector3f p1 = map_to_sphere(mouse_current);

    // Axis is in default coord system
    const Eigen::Vector3f axis = p0.cross(p1).normalized();
    const float angle = vector_angle(p0, p1);

    // Initial rotation
    const Eigen::Matrix4f r_0 = look_at(camera_pos_start, m_lookat, camera_up_start);

    // Rotate axis to current frame and rotate around it
    const Eigen::Vector3f rotated_axis = (r_0.inverse().block<3, 3>(0, 0) * axis);
    Eigen::Matrix4f r_arc = Eigen::Matrix4f::Identity();
    r_arc.block<3, 3>(0, 0) = Eigen::AngleAxisf(angle, rotated_axis).matrix();

    // Get inverse new view matrix
    const Eigen::Matrix4f r = (r_0 * r_arc).inverse().matrix();

    Eigen::Vector3f new_pos, new_scale;
    Eigen::Matrix3f r_decomp;

    // Decompose new position and new rotation
    decompose(r, new_pos, r_decomp, new_scale);

    // Rotate default up axis
    const Eigen::Vector3f up = r_decomp * Eigen::Vector3f(0, 1, 0);

    // Set new camera properties
    set_position(new_pos);
    set_up(up);
}

} // namespace ui
} // namespace lagrange

// Re-enable optimizations if they were disabled
#if LAGRANGE_TARGET_OS(APPLE) && LAGRANGE_TARGET_PLATFORM(x86_64) &&           \
    defined(__apple_build_version__) && __apple_build_version__ >= 21000000 && \
    __apple_build_version__ < 22000000
    #pragma clang optimize on
#endif
