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

///
/// @file fmt/join.h
///
/// Provides `lagrange::join` — a uniform wrapper that dispatches to either a custom formattable
/// view (when `SPDLOG_USE_STD_FORMAT` is defined) or `fmt::join` (the default). For ranges, the
/// outer format spec (e.g. `{:.3g}`) is applied to each element, matching `fmt::join` semantics.
/// Tuple/pair joins only support `{}` (elements may have mixed types).
///

#ifdef SPDLOG_USE_STD_FORMAT

    #include <format>
    #include <iterator>
    #include <string_view>
    #include <tuple>
    #include <type_traits>
    #include <utility>

namespace lagrange {

/// @cond LA_INTERNAL_DOCS
namespace fmt_detail {

/// Lightweight range concept — avoids the heavy `<ranges>` header.
template <typename R>
concept range = requires(R& r) {
    std::begin(r);
    std::end(r);
};

/// Element type of a range.
template <typename R>
using range_value_t =
    std::remove_cvref_t<decltype(*std::begin(std::declval<std::remove_reference_t<R>&>()))>;

/// Lazy view returned by `lagrange::join` for ranges.
template <typename Range>
struct range_join_view
{
    Range range;
    std::string_view sep;
};

/// Lazy view returned by `lagrange::join` for tuples and pairs.
template <typename Tuple>
struct tuple_join_view
{
    const Tuple& tuple;
    std::string_view sep;
};

} // namespace fmt_detail
/// @endcond

/// Join all elements of @p r into a formattable view separated by @p sep. The outer format spec
/// (e.g. `{:.3g}`) is applied to each element, matching `fmt::join` semantics.
template <fmt_detail::range Range>
auto join(Range&& r, std::string_view sep)
{
    return fmt_detail::range_join_view<Range>{std::forward<Range>(r), sep};
}

/// Overload for `std::tuple` (not a range; mirrors `fmt::join` tuple support).
template <typename... Ts>
auto join(const std::tuple<Ts...>& t, std::string_view sep)
{
    return fmt_detail::tuple_join_view<std::tuple<Ts...>>{t, sep};
}

/// Overload for `std::pair` (not a range; mirrors `fmt::join` pair support).
template <typename T, typename U>
auto join(const std::pair<T, U>& p, std::string_view sep)
{
    return fmt_detail::tuple_join_view<std::pair<T, U>>{p, sep};
}

} // namespace lagrange

/// Formatter for `lagrange::fmt_detail::range_join_view`. Delegates the format spec to the
/// element formatter so that e.g. `format("{:.3g}", join(vec, ", "))` formats each float with
/// `.3g` precision.
template <typename Range>
struct std::formatter<lagrange::fmt_detail::range_join_view<Range>, char>
{
    using value_type = lagrange::fmt_detail::range_value_t<Range>;
    std::formatter<value_type, char> m_elem;

    constexpr auto parse(std::format_parse_context& ctx) { return m_elem.parse(ctx); }

    auto format(const lagrange::fmt_detail::range_join_view<Range>& jv, std::format_context& ctx)
        const
    {
        auto out = ctx.out();
        bool first = true;
        for (const auto& elem : jv.range) {
            if (!first) {
                for (auto c : jv.sep) *out++ = c;
                ctx.advance_to(out);
            }
            out = m_elem.format(elem, ctx);
            first = false;
        }
        return out;
    }
};

/// Formatter for `lagrange::fmt_detail::tuple_join_view`. Each element is formatted with `{}`.
/// Custom format specs are not supported for tuple/pair joins (elements may have mixed types).
template <typename Tuple>
struct std::formatter<lagrange::fmt_detail::tuple_join_view<Tuple>, char>
{
    constexpr auto parse(std::format_parse_context& ctx)
    {
        if (ctx.begin() != ctx.end() && *ctx.begin() != '}') {
            throw std::format_error("format spec not supported for tuple/pair join");
        }
        return ctx.begin();
    }

    auto format(const lagrange::fmt_detail::tuple_join_view<Tuple>& jv, std::format_context& ctx)
        const
    {
        auto out = ctx.out();
        bool first = true;
        std::apply(
            [&](const auto&... elems) {
                (
                    [&](const auto& elem) {
                        if (!first) {
                            for (auto c : jv.sep) *out++ = c;
                            ctx.advance_to(out);
                        }
                        out = std::format_to(out, "{}", elem);
                        first = false;
                    }(elems),
                    ...);
            },
            jv.tuple);
        return out;
    }
};

#else

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <spdlog/fmt/ranges.h>
#include <lagrange/utils/warnon.h>
// clang-format on

namespace lagrange {

/// Join all elements of @p r into a formattable view separated by @p sep using `fmt::join`.
template <typename Range>
auto join(Range&& r, std::string_view sep)
{
    return fmt::join(std::forward<Range>(r), sep);
}

} // namespace lagrange

#endif
