// Public domain implementation of the scoped guard concept
// https://github.com/ricab/scope_guard

#pragma once

#include <type_traits>
#include <utility>

namespace lagrange {

namespace sg_detail {

// --- Some custom type traits ---

// Type trait determining whether a type is callable with no arguments
template <typename T, typename = void>
struct is_noarg_callable_t : public std::false_type
{
    // in general, false
};

template <typename T>
struct is_noarg_callable_t<T, decltype(std::declval<T&&>()())> : public std::true_type
{
    // only true when call expression valid
};

// Type trait determining whether a no-argument callable returns void
template <typename T>
struct returns_void_t : public std::is_same<void, decltype(std::declval<T&&>()())>
{
};

// Type trait determining whether a no-arg callable is nothrow invocable if
// required. This is where SG_REQUIRE_NOEXCEPT logic is encapsulated.
template <typename T>
struct is_nothrow_invocable_if_required_t : public std::true_type
{
};

// logic AND of two or more type traits
template <typename A, typename B, typename... C>
struct and_t : public and_t<A, and_t<B, C...>>
{
    // for more than two arguments
};

template <typename A, typename B>
struct and_t<A, B> : public std::conditional<A::value, B, A>::type
{
    // for two arguments
};

// Type trait determining whether a type is a proper scope_guard callback.
template <typename T>
struct is_proper_sg_callback_t : public and_t<
                                     is_noarg_callable_t<T>,
                                     returns_void_t<T>,
                                     is_nothrow_invocable_if_required_t<T>,
                                     std::is_nothrow_destructible<T>>
{
};

// --- The actual scope_guard template ---

template <
    typename Callback,
    typename = typename std::enable_if<is_proper_sg_callback_t<Callback>::value>::type>
class scope_guard;


// --- Now the friend maker ---

// we need this in the inner namespace due to MSVC bugs preventing sg_detail::scope_guard from befriending a sg::make_scope_guard template instance in the parent namespace (see https://is.gd/xFfFhE).
template <typename Callback>
sg_detail::scope_guard<Callback> make_scope_guard(Callback&& callback) noexcept(
    std::is_nothrow_constructible<Callback, Callback&&>::value);

// --- The template specialization that actually defines the class ---

template <typename Callback>
class scope_guard<Callback> final
{
public:
    typedef Callback callback_type;

    scope_guard(scope_guard&& other) noexcept(
        std::is_nothrow_constructible<Callback, Callback&&>::value);

    ~scope_guard() noexcept; // highlight noexcept dtor

    void dismiss() noexcept;

public:
    scope_guard() = delete;
    scope_guard(const scope_guard&) = delete;
    scope_guard& operator=(const scope_guard&) = delete;
    scope_guard& operator=(scope_guard&&) = delete;

private:
    // meant for friends only
    explicit scope_guard(Callback&& callback) noexcept(
        std::is_nothrow_constructible<Callback, Callback&&>::value);

    // only make_scope_guard can create scope_guards from scratch (i.e. non-move)
    friend scope_guard<Callback> make_scope_guard<Callback>(Callback&&) noexcept(
        std::is_nothrow_constructible<Callback, Callback&&>::value);

private:
    Callback m_callback;
    bool m_active;
};

} // namespace sg_detail


// --- Now the single public maker function ---

using sg_detail::make_scope_guard; // see comment on declaration above

////////////////////////////////////////////////////////////////////////////////

template <typename Callback>
sg_detail::scope_guard<Callback>::scope_guard(Callback&& callback) noexcept(
    std::is_nothrow_constructible<Callback, Callback&&>::value)
    // use () instead of {} because of DR 1467 (https://is.gd/WHmWuo), which still impacts older compilers (e.g. GCC 4.x and clang <=3.6, see https://godbolt.org/g/TE9tPJ and https://is.gd/Tsmh8G)
    : m_callback(std::forward<Callback>(callback))
    , m_active{true}
{}

////////////////////////////////////////////////////////////////////////////////

template <typename Callback>
sg_detail::scope_guard<Callback>::~scope_guard() noexcept
{
    if (m_active) m_callback();
}

////////////////////////////////////////////////////////////////////////////////

template <typename Callback>
sg_detail::scope_guard<Callback>::scope_guard(scope_guard&& other) noexcept(
    std::is_nothrow_constructible<Callback, Callback&&>::value)
    : m_callback(std::forward<Callback>(other.m_callback)) // idem
    , m_active{std::move(other.m_active)}
{
    other.m_active = false;
}

////////////////////////////////////////////////////////////////////////////////

template <typename Callback>
inline void sg_detail::scope_guard<Callback>::dismiss() noexcept
{
    m_active = false;
}

////////////////////////////////////////////////////////////////////////////////

template <typename Callback>
inline auto sg_detail::make_scope_guard(Callback&& callback) noexcept(
    std::is_nothrow_constructible<Callback, Callback&&>::value) -> sg_detail::scope_guard<Callback>
{
    return sg_detail::scope_guard<Callback>{std::forward<Callback>(callback)};
}

} // namespace lagrange
