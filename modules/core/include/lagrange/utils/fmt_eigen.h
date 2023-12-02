/*
 * Copyright 2023 Adobe. All rights reserved.
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

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <spdlog/spdlog.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <Eigen/Core>

#if defined(LAGRANGE_FMT_EIGEN_FIX) && defined(_MSC_VER)
// MSVC crashes occasionally when using fmt with Eigen. e.g.
// ```
//     Eigen::Matrix3f m;
//     logger().info("{}", m); // C1001: internal compiler error
// ```
// This manifests as a C1001 internal compiler error, make it impossible to
// debug. This workaround is a temporary fix until MSCV fixes the bug.
//
// MSVC bug report: https://developercommunity.visualstudio.com/t/10376323
// Version affected: 17.6, 17.7
// Bug is fixed in version 17.8

#include <sstream>
#include <type_traits>
template <typename Derived>
struct fmt::formatter<Derived,
    std::enable_if_t<std::is_base_of<Eigen::DenseBase<Derived>, Derived>::value, char>>
{
    template<typename ParseContext>
    constexpr auto parse(ParseContext& ctx) { return ctx.begin(); }

    template<typename FormatContext>
    auto format(const Eigen::DenseBase<Derived>& m, FormatContext& ctx) {
        std::stringstream ss;
        ss << m;
        return fmt::format_to(ctx.out(), "{}", ss.str());
    }
};
#else
// clang-format off
#include <lagrange/utils/warnoff.h>
#include <spdlog/fmt/ostr.h>
#include <lagrange/utils/warnon.h>
// clang-format on
#endif
