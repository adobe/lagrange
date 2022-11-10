// Source: https://github.com/KonanM/small_vector
// SPDX-License-Identifier: Unlicense
//
// This file has been modified by Adobe.
//
// All modifications are Copyright 2022 Adobe.

#pragma once

#include <initializer_list>
#include <memory>
#include <type_traits>
#include <vector>

namespace lagrange {

///
/// Allocator that uses the stack upto a maximum size, and the heap beyond that.
///
/// @note From https://github.com/KonanM/small_vector, renamed and formatted to be consistent with
///   Lagrange conventions.
///
template <typename T, std::size_t MaxSize = 8, typename NonReboundT = T>
struct SmallBufferAllocator
{
    alignas(alignof(T)) char m_small_buffer[MaxSize * sizeof(T)];
    std::allocator<T> m_alloc{};
    bool m_small_buffer_used = false;

    using value_type = T;
    // we have to set these three values, as they are responsible for the correct handling of the
    // move assignment operator
    using propagate_on_container_move_assignment = std::false_type;
    using propagate_on_container_swap = std::false_type;
    using is_always_equal = std::false_type;

    template <class U>
    struct rebind
    {
        typedef SmallBufferAllocator<U, MaxSize, NonReboundT> other;
    };

    constexpr SmallBufferAllocator() noexcept = default;
    template <class U>
    constexpr SmallBufferAllocator(const SmallBufferAllocator<U, MaxSize, NonReboundT>&) noexcept
    {}
    // don't copy the small buffer for the copy/move constructors, as the copying is done through
    // the vector
    constexpr SmallBufferAllocator(const SmallBufferAllocator& other) noexcept
        : m_small_buffer_used(other.m_small_buffer_used)
    {}
    constexpr SmallBufferAllocator& operator=(const SmallBufferAllocator& other) noexcept
    {
        m_small_buffer_used = other.m_small_buffer_used;
        return *this;
    }
    constexpr SmallBufferAllocator(SmallBufferAllocator&&) noexcept {}
    constexpr SmallBufferAllocator& operator=(const SmallBufferAllocator&&) noexcept
    {
        return *this;
    }

    constexpr T* allocate(const std::size_t n)
    {
        // when the allocator was rebound we don't want to use the small buffer
        if constexpr (std::is_same<T, NonReboundT>::value) {
            if (n <= MaxSize) {
                m_small_buffer_used = true;
                // as long as we use less memory than the small buffer, we return a pointer to it
                return reinterpret_cast<T*>(&m_small_buffer);
            }
        }
        m_small_buffer_used = false;
        // otherwise use the default allocator
        return m_alloc.allocate(n);
    }
    constexpr void deallocate(void* p, const std::size_t n)
    {
        // we don't deallocate anything if the memory was allocated in small buffer
        if (&m_small_buffer != p) m_alloc.deallocate(static_cast<T*>(p), n);
        m_small_buffer_used = false;
    }
    // according to the C++ standard when propagate_on_container_move_assignment is set to false,
    // the comparison operators are used to check if two allocators are equal. When they are not, an
    // element wise move is done instead of just taking over the memory. For our implementation this
    // means the comparision has to return false, when the small buffer is active
    friend constexpr bool operator==(
        const SmallBufferAllocator& lhs,
        const SmallBufferAllocator& rhs)
    {
        return !lhs.m_small_buffer_used && !rhs.m_small_buffer_used;
    }
    friend constexpr bool operator!=(
        const SmallBufferAllocator& lhs,
        const SmallBufferAllocator& rhs)
    {
        return !(lhs == rhs);
    }
};

///
/// Hybrid vector that uses the stack upto a maximum size, and the heap beyond that.
///
/// @tparam     T     Value type.
/// @tparam     N     Maximum size.
///
/// @note From https://github.com/KonanM/small_vector, renamed and formatted to be consistent with
///   Lagrange conventions.
///
template <typename T, std::size_t N = 8>
class SmallVector : public std::vector<T, SmallBufferAllocator<T, N>>
{
private:
    using vectorT = std::vector<T, SmallBufferAllocator<T, N>>;

public:
    // default initialize with the small buffer size
    constexpr SmallVector() noexcept { vectorT::reserve(N); }
    SmallVector(const SmallVector&) = default;
    SmallVector& operator=(const SmallVector&) = default;
    SmallVector(SmallVector&& other) noexcept(std::is_nothrow_move_constructible<T>::value)
    {
        if (other.size() <= N) vectorT::reserve(N);
        vectorT::operator=(std::move(other));
    }
    SmallVector& operator=(SmallVector&& other) noexcept(
        std::is_nothrow_move_constructible<T>::value)
    {
        if (other.size() <= N) vectorT::reserve(N);
        vectorT::operator=(std::move(other));
        return *this;
    }
    // use the default constructor first to reserve then construct the values
    explicit SmallVector(std::size_t count)
        : SmallVector()
    {
        vectorT::resize(count);
    }
    SmallVector(std::size_t count, const T& value)
        : SmallVector()
    {
        vectorT::assign(count, value);
    }
    template <class InputIt>
    SmallVector(InputIt first, InputIt last)
        : SmallVector()
    {
        vectorT::insert(vectorT::begin(), first, last);
    }
    SmallVector(std::initializer_list<T> init)
        : SmallVector()
    {
        vectorT::insert(vectorT::begin(), init);
    }
    friend void swap(SmallVector& a, SmallVector& b) noexcept
    {
        using std::swap;
        swap(static_cast<vectorT&>(a), static_cast<vectorT&>(b));
    }
};

} // namespace lagrange
