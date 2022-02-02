/*
 * Copyright 2020 Adobe. All rights reserved.
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

#include <lagrange/Logger.h>
#include <lagrange/common.h>
#include <lagrange/experimental/Array.h>
#include <lagrange/experimental/create_array.h>

namespace lagrange {
namespace experimental {

class Attribute
{
public:
    Attribute() = default;

    Attribute(const Attribute& other) = default;
    Attribute(Attribute&& other) = default;
    Attribute& operator=(const Attribute& other) = default;

    Attribute(std::shared_ptr<ArrayBase> values)
        : m_values(std::move(values))
    {}

    template <typename Derived>
    Attribute(const Eigen::MatrixBase<Derived>& values)
    {
        this->set(values);
    }

    template <typename Derived>
    Attribute(Eigen::MatrixBase<Derived>&& values)
    {
        this->set(std::move(values));
    }

public:
    std::shared_ptr<const ArrayBase> get() const { return m_values; }
    std::shared_ptr<ArrayBase> get() { return m_values; }

    template <typename Derived>
    decltype(auto) get() const
    {
        la_runtime_assert(m_values != nullptr);
        const auto& values = *m_values;
        return values.get<Derived>();
    }

    template <typename Derived>
    decltype(auto) get()
    {
        la_runtime_assert(m_values != nullptr);
        return m_values->get<Derived>();
    }

    template <typename Derived>
    decltype(auto) view() const
    {
        la_runtime_assert(m_values != nullptr);
        const auto& values = *m_values;
        return values.view<Derived>();
    }

    template <typename Derived>
    decltype(auto) view()
    {
        la_runtime_assert(m_values != nullptr);
        return m_values->view<Derived>();
    }

    template <typename Derived>
    void set(const Eigen::MatrixBase<Derived>& values)
    {
        if (m_values == nullptr) {
            m_values = create_array(values.derived());
        } else {
            m_values->set(values.derived());
        }
    }

    template <typename Derived>
    void set(Eigen::MatrixBase<Derived>&& values)
    {
#ifndef NDEBUG
        void* ptr = values.derived().data();
#endif
        if (m_values == nullptr) {
            m_values = create_array(std::move(values.derived()));
        } else {
            m_values->set(std::move(values.derived()));
        }
#ifndef NDEBUG
        void* ptr2 = m_values->data();
        if (ptr != ptr2) {
            logger().warn("Attribute values are copied when they should have been moved.  "
                          "Likely caused by inexact match of `Derived` type.");
        }
#endif
    }

    template <typename Derived>
    void set(const Eigen::ArrayBase<Derived>& values)
    {
        set(values.matrix());
    }

    template <typename Derived>
    void set(Eigen::ArrayBase<Derived>&& values)
    {
        set(std::move(values.matrix()));
    }

    void set(std::shared_ptr<ArrayBase> values) { m_values = std::move(values); }

    template <typename Archive>
    void serialize_impl(Archive& ar)
    {
        ar & m_values;
    }

private:
    std::shared_ptr<ArrayBase> m_values;
};

template <typename Archive>
void serialize(Attribute& attribute, Archive& ar)
{
    attribute.serialize_impl(ar);
}

} // namespace experimental
} // namespace lagrange
