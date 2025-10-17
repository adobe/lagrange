// Source: https://github.com/X-czh/smart_ptr
// SPDX-License-Identifier: MIT
//
// This file has been modified by Adobe.
//
// All modifications are Copyright 2022 Adobe.

// control_block implementation

#pragma once

#include <lagrange/internal/smart_ptr/default_delete.h>
#include <lagrange/internal/smart_ptr/ptr.h>

#include <atomic> // atomic
#include <memory> // allocator, addressof

namespace lagrange::internal {

// control block interface
// Type erasure for storing deleter and allocators

class control_block_base
{
public:
    virtual ~control_block_base() {};

    virtual void inc_ref() noexcept = 0;
    virtual void inc_wref() noexcept = 0;
    virtual void dec_ref() noexcept = 0;
    virtual void dec_wref() noexcept = 0;

    virtual long use_count() const noexcept = 0;
    virtual bool unique() const noexcept = 0;
    virtual long weak_use_count() const noexcept = 0;
    virtual bool expired() const noexcept = 0;

    virtual void* get_deleter() noexcept = 0;
};

// control block for reference counting of shared_ptr and weak_ptr

/**
 * NOT implemented: custom allocator support.
 *
 * The allocator is intended to be used to allocate and deallocate
 *  internal shared_ptr details, not the object.
 */

template <typename T, typename D = default_delete<T>>
class control_block : public control_block_base
{
public:
    using element_type = T;
    using deleter_type = D;

    // Constructors

    control_block(T* p)
        : _impl{p}
    {}

    control_block(T* p, D d)
        : _impl{p, d}
    {}


    // Destructor

    ~control_block() = default;

    // Modifiers

    void inc_ref() noexcept override { ++_use_count; }

    void inc_wref() noexcept override { ++_weak_use_count; }

    void dec_ref() noexcept override
    {
        auto _ptr = _impl._impl_ptr();
        auto& _deleter = _impl._impl_deleter();
        if (--_use_count == 0) {
            if (_ptr) _deleter(_ptr); // destroy the object _ptr points to
            dec_wref();
        }
    }

    void dec_wref() noexcept override
    {
        if (--_weak_use_count == 0) {
            delete this; // destroy control_block itself
        }
    }

    // Observers

    long use_count() const noexcept override // Returns #shared_ptr
    {
        return _use_count;
    }

    bool unique() const noexcept override { return _use_count == 1; }

    long weak_use_count() const noexcept override // Returns #weak_ptr
    {
        return _weak_use_count - ((_use_count > 0) ? 1 : 0);
    }

    bool expired() const noexcept override { return _use_count == 0; }

    void* get_deleter() noexcept override // Type erasure for storing deleter
    {
        return reinterpret_cast<void*>(std::addressof(_impl._impl_deleter()));
    }

private:
    std::atomic<long> _use_count{1};

    // Note: _weak_use_count = #weak_ptrs + (#shared_ptr > 0) ? 1 : 0
    std::atomic<long> _weak_use_count{1};

    ptr<T, D> _impl;
};

} // namespace lagrange::internal
