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
/// Provides `lagrange::join` — a wrapper that returns a custom formattable view (in
/// `lagrange::fmt_detail`). For ranges, the outer format spec (e.g. `{:.3g}`) is applied to each
/// element, matching `fmt::join` semantics. Tuple/pair joins only support `{}` (elements may
/// have mixed types).
///
/// The view exposes both `std::formatter` and `fmt::formatter` specializations (each guarded by
/// the availability of its respective backend) so that whichever `format` overload is picked at
/// the call site — including `std::format` selected via ADL on `fmt::join_view` template
/// arguments under MSVC 14.50 / VS 2026 — finds a matching formatter.
///

// fmt is reachable whenever spdlog isn't running in pure-std::format mode.
#if !defined(SPDLOG_USE_STD_FORMAT)
// clang-format off
    #include <lagrange/utils/warnoff.h>
    #include <spdlog/fmt/fmt.h>
    #include <lagrange/utils/warnon.h>
// clang-format on
#endif

// std::format is available when explicitly selected for spdlog or when the standard library
// exposes the C++20 header.
#if defined(SPDLOG_USE_STD_FORMAT) || defined(__cpp_lib_format)
    #include <format>
#endif

#include <iterator>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace lagrange {

/// @cond LA_INTERNAL_DOCS
namespace fmt_detail {

/// Element type of a range (C++17-compatible substitute for `std::remove_cvref_t`).
template <typename R>
using range_value_t =
    std::remove_cv_t<std::remove_reference_t<decltype(*std::begin(std::declval<const R&>()))>>;

/// Lazy view returned by `lagrange::join` for ranges. Stores the range by pointer so that the
/// `Range` template parameter is always a value (non-reference) type — some MSVC versions
/// (e.g. 14.50) reject reference-typed template arguments inside formatter specializations during
/// compile-time format-string parsing.
template <typename Range>
struct range_join_view
{
    const Range* range;
    std::string_view sep;
};

/// Lazy view returned by `lagrange::join` for tuples and pairs.
template <typename Tuple>
struct tuple_join_view
{
    const Tuple* tuple;
    std::string_view sep;
};

/// Backend-agnostic formatting helpers. `format_elem` writes one element to `ctx.out()` and
/// returns the advanced iterator; the helper handles the separator interleaving.
template <typename Range, typename FormatContext, typename Func>
auto format_range(const range_join_view<Range>& jv, FormatContext& ctx, Func&& format_elem)
{
    auto out = ctx.out();
    bool first = true;
    for (const auto& elem : *jv.range) {
        if (!first) {
            for (auto c : jv.sep) *out++ = c;
            ctx.advance_to(out);
        }
        out = format_elem(elem, ctx);
        first = false;
    }
    return out;
}

template <typename Tuple, typename FormatContext, typename Func>
auto format_tuple(const tuple_join_view<Tuple>& jv, FormatContext& ctx, Func&& format_elem)
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
                    out = format_elem(elem, ctx);
                    first = false;
                }(elems),
                ...);
        },
        *jv.tuple);
    return out;
}

} // namespace fmt_detail
/// @endcond

/// Join all elements of @p r into a formattable view separated by @p sep. The outer format spec
/// (e.g. `{:.3g}`) is applied to each element, matching `fmt::join` semantics. The view stores a
/// pointer to @p r — the temporary must outlive the formatting call (true for the typical
/// `format(..., join(r, ","))` pattern). Tuple/pair overloads below take precedence by partial
/// ordering when @p r is a `std::tuple` or `std::pair`.
template <typename Range>
auto join(const Range& r, std::string_view sep)
{
    return fmt_detail::range_join_view<Range>{&r, sep};
}

/// Overload for `std::tuple` (not a range; mirrors `fmt::join` tuple support).
template <typename... Ts>
auto join(const std::tuple<Ts...>& t, std::string_view sep)
{
    return fmt_detail::tuple_join_view<std::tuple<Ts...>>{&t, sep};
}

/// Overload for `std::pair` (not a range; mirrors `fmt::join` pair support).
template <typename T, typename U>
auto join(const std::pair<T, U>& p, std::string_view sep)
{
    return fmt_detail::tuple_join_view<std::pair<T, U>>{&p, sep};
}

} // namespace lagrange

// std::formatter specializations. Required when std::format is selected at the call site, which
// can happen via ADL even if `lagrange::format` resolves to `fmt::format` — `fmt::v12::join_view`
// template arguments pull `std::` into the candidate set. Delegates the format spec to the
// element formatter so that e.g. `format("{:.3g}", join(vec, ", "))` formats each float with
// `.3g` precision.
#if defined(SPDLOG_USE_STD_FORMAT) || defined(__cpp_lib_format)

template <typename Range>
struct std::formatter<lagrange::fmt_detail::range_join_view<Range>, char>
{
    using value_type = lagrange::fmt_detail::range_value_t<Range>;
    std::formatter<value_type, char> m_elem;

    constexpr auto parse(std::format_parse_context& ctx) { return m_elem.parse(ctx); }

    auto format(const lagrange::fmt_detail::range_join_view<Range>& jv, std::format_context& ctx)
        const
    {
        return lagrange::fmt_detail::format_range(
            jv,
            ctx,
            [this](const auto& elem, std::format_context& c) { return m_elem.format(elem, c); });
    }
};

/// Tuple/pair joins only support `{}` (elements may have mixed types).
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
        return lagrange::fmt_detail::format_tuple(jv, ctx, [](const auto& elem, auto& c) {
            return std::format_to(c.out(), "{}", elem);
        });
    }
};

#endif // defined(SPDLOG_USE_STD_FORMAT) || defined(__cpp_lib_format)

// fmt::formatter specializations — required for callers routing through {fmt} (e.g. spdlog
// logger calls when SPDLOG_USE_STD_FORMAT is not set, or when `lagrange::format` resolves to
// `fmt::format`).
#if !defined(SPDLOG_USE_STD_FORMAT)

template <typename Range>
struct fmt::formatter<lagrange::fmt_detail::range_join_view<Range>, char>
{
    using value_type = lagrange::fmt_detail::range_value_t<Range>;
    fmt::formatter<value_type, char> m_elem;

    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx)
    {
        return m_elem.parse(ctx);
    }

    template <typename FormatContext>
    auto format(const lagrange::fmt_detail::range_join_view<Range>& jv, FormatContext& ctx) const
    {
        return lagrange::fmt_detail::format_range(
            jv,
            ctx,
            [this](const auto& elem, FormatContext& c) { return m_elem.format(elem, c); });
    }
};

template <typename Tuple>
struct fmt::formatter<lagrange::fmt_detail::tuple_join_view<Tuple>, char>
{
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx)
    {
        if (ctx.begin() != ctx.end() && *ctx.begin() != '}') {
            throw fmt::format_error("format spec not supported for tuple/pair join");
        }
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const lagrange::fmt_detail::tuple_join_view<Tuple>& jv, FormatContext& ctx) const
    {
        return lagrange::fmt_detail::format_tuple(jv, ctx, [](const auto& elem, auto& c) {
            return fmt::format_to(c.out(), "{}", elem);
        });
    }
};

#endif // !defined(SPDLOG_USE_STD_FORMAT)
