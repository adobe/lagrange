/*
 * Copyright 2025 Adobe. All rights reserved.
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

#include <lagrange/utils/assert.h>

#include <vector>

#ifdef LAGRANGE_WITH_PYTHON

    #include <memory>

namespace lagrange {

template <typename T>
class SharedPtrVector : public std::vector<std::shared_ptr<T>>
{
public:
    using Super = std::vector<std::shared_ptr<T>>;

public:
    struct Iterator : public Super::iterator
    {
    private:
        using SuperIt = typename Super::iterator;

    public:
        using iterator_category = std::input_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = T;
        using pointer = T*;
        using reference = T&;

    public:
        using SuperIt::SuperIt;
        Iterator(const SuperIt& it)
            : SuperIt(it)
        {}
        Iterator(SuperIt&& it)
            : SuperIt(std::move(it))
        {}
        const T& operator*() const { return *SuperIt::operator*(); }
        T& operator*() { return *SuperIt::operator*(); }
    };

    struct ConstIterator : public Super::const_iterator
    {
    private:
        using SuperIt = typename Super::const_iterator;

    public:
        using iterator_category = std::input_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = T;
        using pointer = const T*;
        using reference = const T&;

    public:
        using SuperIt::SuperIt;
        ConstIterator(const SuperIt& it)
            : SuperIt(it)
        {}
        ConstIterator(SuperIt&& it)
            : SuperIt(std::move(it))
        {}
        const T& operator*() const { return *SuperIt::operator*(); }
    };

public:
    using Super::Super;

    template <typename InputIter>
    SharedPtrVector(InputIter first, InputIter last)
    {
        for (auto it = first; it != last; ++it) {
            Super::push_back(std::make_shared<T>(*it));
        }
    }
    SharedPtrVector(std::initializer_list<T> init)
        : SharedPtrVector(init.begin(), init.end())
    {}
    SharedPtrVector(const std::vector<T>& values)
        : SharedPtrVector(values.begin(), values.end())
    {}
    SharedPtrVector(std::vector<T>&& values)
        : SharedPtrVector(values.begin(), values.end())
    {}

    void push_back(const T& value) { Super::emplace_back(std::make_shared<T>(value)); }
    void push_back(T&& value) { Super::push_back(std::make_shared<T>(std::move(value))); }
    template <typename... Args>
    void emplace_back(Args&&... args)
    {
        Super::push_back(std::make_shared<T>(std::forward<Args>(args)...));
    }
    const T& back() const { return *Super::back().get(); }
    T& back() { return *Super::back().get(); }
    const T& front() const { return *Super::front().get(); }
    T& front() { return *Super::front().get(); }
    const T& operator[](size_t i) const { return *Super::operator[](i).get(); }
    T& operator[](size_t i) { return *Super::operator[](i).get(); }
    const T& at(size_t i) const { return *Super::at(i).get(); }
    T& at(size_t i) { return *Super::at(i).get(); }

    Iterator begin() { return Super::begin(); }
    Iterator end() { return Super::end(); }
    ConstIterator begin() const { return Super::begin(); }
    ConstIterator end() const { return Super::end(); }
};

template <typename T>
struct DerivedVector : public std::vector<T>
{
    using Super = std::vector<T>;

    using Super::Super;

    DerivedVector(const std::vector<T>& values)
        : Super(values.begin(), values.end())
    {}
    DerivedVector(std::vector<T>&& values)
        : Super(values.begin(), values.end())
    {}
};

template <typename T>
using SafeVector =
    std::conditional_t<std::is_arithmetic_v<T>, DerivedVector<T>, SharedPtrVector<T>>;

} // namespace lagrange

#else

namespace lagrange {

template <typename T>
using SafeVector = std::vector<T>;

}

#endif
