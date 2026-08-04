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
// Bug: VectorCombine::foldSelectShuffle() segfaults on (perspective * view).inverse()
// Affects: Apple Clang (Xcode 26.4, __apple_build_version__ 21000099), x86_64 only
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
// TODO: Remove this file once we stop supporting Xcode 26...

#include <lagrange/utils/build.h>

// Only disable optimizations for the specific problematic configuration
//
// Note:
// - __apple_build_version__ is only defined by Apple Clang, not Homebrew LLVM
// - Bug appears in Xcode 26.4 (clang-2100.0.123.102)
// - Got fixed in Xcode-27.0.0-Beta.3 (clang-2100.3.25.1)
//
// Version numbers:
// - 21000099 = Xcode 26.4.0
// - 21000323 = Xcode 27.0.0 Beta 2
// - 21000325 = Xcode 27.0.0 Beta 3

#if LAGRANGE_TARGET_OS(APPLE) && LAGRANGE_TARGET_PLATFORM(x86_64) &&           \
    defined(__apple_build_version__) && __apple_build_version__ >= 21000000 && \
    __apple_build_version__ < 21000325
    #pragma clang optimize off
#endif

#include <lagrange/ui/utils/math.h>

namespace lagrange {
namespace ui {

Eigen::Vector3f unproject_point(
    const Eigen::Vector3f& v,
    const Eigen::Matrix4f& view,
    const Eigen::Matrix4f& perspective,
    const Eigen::Vector4f& viewport)
{
    Eigen::Vector4f tmp = Eigen::Vector4f(v.x(), v.y(), v.z(), 1.0f);
    tmp.x() = (tmp.x() - viewport(0)) / viewport(2);
    tmp.y() = (tmp.y() - viewport(1)) / viewport(3);
    tmp = tmp * 2.0f - Eigen::Vector4f::Ones();

    Eigen::Vector4f obj = (perspective * view).inverse() * tmp;
    obj *= 1.0f / obj.w();

    return obj.head<3>();
}

} // namespace ui
} // namespace lagrange

// Re-enable optimizations if they were disabled
#if LAGRANGE_TARGET_OS(APPLE) && LAGRANGE_TARGET_PLATFORM(x86_64) &&           \
    defined(__apple_build_version__) && __apple_build_version__ >= 21000000 && \
    __apple_build_version__ < 21000325
    #pragma clang optimize on
#endif
