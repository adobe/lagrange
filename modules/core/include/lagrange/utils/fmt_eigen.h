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

// Dealing with fmt-Eigen shenanigans. Ostream support was deprecated in fmt v9.x, and removed from
// the library in fmt v10.x [1]. Supposedly this was causing ODR violations [2] and was a source of
// headaches. The consequence of this is that formatting Eigen objects is broken with fmt >= v9.x.
// Neither the fmt author [3] nor the Eigen maintainers are really interested in shipping &
// supporting a fmt::formatter<> for Eigen objects. We provide one here as a workaround until a
// better solution comes along. We also provide an option to override this header with your own
// hook in case this conflicts with another fmt::formatter<> somewhere else in your codebase.
//
// [1]: https://github.com/fmtlib/fmt/issues/3318
// [2]: https://github.com/fmtlib/fmt/issues/2357
// [3]: https://github.com/fmtlib/fmt/issues/3465

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/fmt.h>
#include <spdlog/fmt/ranges.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <Eigen/Core>

#ifdef LA_FMT_EIGEN_FORMATTER

    // User-provided fmt::formatter<> for Eigen types.
    #include LA_FMT_EIGEN_FORMATTER

#elif defined(SPDLOG_USE_STD_FORMAT)

// It's still a bit early for C++20 format support...

#else

    // spdlog with fmt (either bundled or external, doesn't matter at this point)
    #if FMT_VERSION >= 100200

        // Use the new nested formatter with fmt >= 10.2.0.
        // This support nested Eigen types as well as padding/format specifiers.
        #include <type_traits>

template <typename T>
struct fmt::formatter<T, std::enable_if_t<std::is_base_of<Eigen::DenseBase<T>, T>::value, char>>
    : fmt::nested_formatter<typename T::Scalar>
{
    auto format(T const& a, format_context& ctx) const
    {
        return this->write_padded(ctx, [&](auto out) {
            for (Eigen::Index ir = 0; ir < a.rows(); ir++) {
                for (Eigen::Index ic = 0; ic < a.cols(); ic++) {
                    out = fmt::format_to(out, "{} ", this->nested(a(ir, ic)));
                }
                out = fmt::format_to(out, "\n");
            }
            return out;
        });
    }
};

template <typename Derived>
struct fmt::is_range<
    Derived,
    std::enable_if_t<std::is_base_of<Eigen::DenseBase<Derived>, Derived>::value, char>>
    : std::false_type
{
};

    #elif (FMT_VERSION >= 100000) || (FMT_VERSION >= 90000 && !defined(FMT_DEPRECATED_OSTREAM)) || \
        (defined(LAGRANGE_FMT_EIGEN_FIX) && defined(_MSC_VER))

        // fmt >= 10.x or fmt 9.x without deprecated ostream support.
        //
        // We also uses a fmt::formatter<> with fmt v9.x and certain versions of MSVC to workaround
        // a compiler bug, triggered by code like this:
        // ```
        //     Eigen::Matrix3f m;
        //     logger().info("{}", m); // C1001: internal compiler error
        // ```
        // This manifests as a C1001 internal compiler error, make it impossible to
        // debug.
        //
        // MSVC bug report: https://developercommunity.visualstudio.com/t/10376323
        // Version affected: 17.6, 17.7
        // Bug is fixed in version 17.8
        //
        #include <type_traits>

template <typename Derived>
struct fmt::formatter<
    Derived,
    std::enable_if_t<std::is_base_of<Eigen::DenseBase<Derived>, Derived>::value, char>>
{
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx)
    {
        return m_underlying.parse(ctx);
    }

    template <typename FormatContext>
    auto format(const Derived& mat, FormatContext& ctx) const
    {
        auto out = ctx.out();

        for (Eigen::Index row = 0; row < mat.rows(); ++row) {
            for (Eigen::Index col = 0; col < mat.cols(); ++col) {
                out = fmt::format_to(out, "  ");
                out = m_underlying.format(mat.coeff(row, col), ctx);
            }

            if (row < mat.rows() - 1) {
                out = fmt::format_to(out, "\n");
            }
        }

        return out;
    }

private:
    fmt::formatter<typename Derived::Scalar, char> m_underlying;
};

template <typename Derived>
struct fmt::is_range<
    Derived,
    std::enable_if_t<std::is_base_of<Eigen::DenseBase<Derived>, Derived>::value, char>>
    : std::false_type
{
};

    #else

    // Include legacy ostr support

    // clang-format off
    #include <lagrange/utils/warnoff.h>
    #include <spdlog/fmt/ostr.h>
    #include <lagrange/utils/warnon.h>
    // clang-format on

template <typename Derived>
struct fmt::is_range<
    Derived,
    std::enable_if_t<std::is_base_of<Eigen::DenseBase<Derived>, Derived>::value, char>>
    : std::false_type
{
};

    #endif

#endif
