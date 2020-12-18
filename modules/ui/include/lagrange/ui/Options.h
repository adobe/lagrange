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


#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <functional>
#include <memory>
#include <limits>


#include <variant>

#include <lagrange/ui/Color.h>
#include <lagrange/ui/Callbacks.h>
#include <lagrange/ui/utils/math.h>

namespace lagrange {
namespace ui {

using OptionType = std::variant<
    std::string,
    char, int,
    float, double,
    bool,
    Color,
    Eigen::Vector2f, Eigen::Vector3f, Eigen::Vector4f,
    Eigen::Vector2i, Eigen::Vector3i, Eigen::Vector4i,
    Eigen::Matrix2f, Eigen::Matrix3f, Eigen::Matrix4f
>;


struct OptionDomain {
    OptionType min_value;
    OptionType max_value;
    OptionType delta;
};

namespace option_detail {
template <typename T>
struct defaults {
    static T min() { return std::numeric_limits<T>::lowest(); }
    static T max() { return (std::numeric_limits<T>::max)(); }
    static T delta() { return T(1); }
};


template <typename T>
struct eigen_defaults
{
    static T min() { return T::Constant(std::numeric_limits<typename T::Scalar>::lowest()); }
    static T max() { return T::Constant((std::numeric_limits<typename T::Scalar>::max)()); }
    static T delta() { return T::Ones(); }
};

template <> struct defaults<std::string>
{
    static std::string min() { return ""; }
    static std::string max() { return ""; }
    static std::string delta() { return ""; }
};

template <> struct defaults<Eigen::Vector2f> : public eigen_defaults<Eigen::Vector2f> {};
template <> struct defaults<Eigen::Vector3f> : public eigen_defaults<Eigen::Vector3f> {};
template <> struct defaults<Eigen::Vector4f> : public eigen_defaults<Eigen::Vector4f> {};

template <> struct defaults<Eigen::Vector2i> : public eigen_defaults<Eigen::Vector2i> {};
template <> struct defaults<Eigen::Vector3i> : public eigen_defaults<Eigen::Vector3i> {};
template <> struct defaults<Eigen::Vector4i> : public eigen_defaults<Eigen::Vector4i> {};

template <> struct defaults<Eigen::Matrix2f> : public eigen_defaults<Eigen::Matrix2f> {};
template <> struct defaults<Eigen::Matrix3f> : public eigen_defaults<Eigen::Matrix3f> {};
template <> struct defaults<Eigen::Matrix4f> : public eigen_defaults<Eigen::Matrix4f> {};

template <> struct defaults<Color>
{
    static Color min() { return Color(0, 0, 0, 0); }
    static Color max() { return Color(1, 1, 1, 1); }
    static Color delta() { return Color(0.05f); }
};
}

struct Option {
    OptionType value;
    OptionDomain domain;
};

class OptionSet : public CallbacksBase<OptionSet>{

public:

    using OnChanged = UI_CALLBACK(void(const OptionSet&));
    using OnDestroyed = UI_CALLBACK(void(const OptionSet&));

    OptionSet& operator[](const std::string& childName);
    const OptionSet& operator[](const std::string& childName) const;

    std::shared_ptr<OptionSet> get_child_ptr(const std::string& childName);

    ~OptionSet() {
        m_callbacks.call<OnDestroyed>(*this);
    }

    template <typename T>
    const T& get(const std::string& optionName) const;

    template <typename T>
    void set(const std::string& optionName, T value, bool suppressCallback = false);

    template <typename T>
    bool add(
        const std::string& optionName,
        T value,
        T min_val = option_detail::defaults<T>::min(),
        T max_val = option_detail::defaults<T>::max(),
        T delta = option_detail::defaults<T>::delta()
    );

    bool add_child(const std::string & child_name, std::shared_ptr<OptionSet> child);

    bool erase(const std::string& optionName);

    bool has(const std::string& optionName) const;

    void clear();

    std::map<std::string, std::shared_ptr<Option>>& get_options();
    const std::map<std::string, std::shared_ptr<Option>>& get_options() const;

    std::map<std::string, std::shared_ptr<OptionSet>>& get_children();
    const std::map<std::string, std::shared_ptr<OptionSet>>& get_children() const;

    size_t num_children() const;

    size_t num_options() const;

    void trigger_change() {
        m_callbacks.call<OnChanged>(*this);
    }

private:
    std::map<std::string, std::shared_ptr<Option>> options;
    std::map<std::string, std::shared_ptr<OptionSet>> children;

    Callbacks<OnChanged, OnDestroyed> m_callbacks;

friend CallbacksBase<OptionSet>;
};

template <typename T>
const T& OptionSet::get(const std::string& optionName) const
{
    auto& optPtr = options.at(optionName);
    return std::get<T>(optPtr->value);
}

template <typename T>
void OptionSet::set(const std::string& optionName, T value, bool suppressCallback)
{
    auto& optPtr = options[optionName];
    if (!optPtr)
        optPtr = std::make_shared<Option>();

    optPtr->value = std::move(value);

    if (!suppressCallback) {
        m_callbacks.call<OnChanged>(*this);
    }
}


template <typename T>
bool OptionSet::add(
    const std::string& optionName,
    T value,
    T min_val /*= Option::defaults<T>::min()*/,
    T max_val /*= Option::defaults<T>::max()*/,
    T delta /*= Option::defaults<T>::delta() */)
{
    auto it = options.find(optionName);
    if (it != options.end())
        return false;

    auto& opt_ptr = options[optionName];
    opt_ptr = std::make_shared<Option>();
    opt_ptr->value = std::move(value);
    opt_ptr->domain.min_value = min_val;
    opt_ptr->domain.max_value = max_val;
    opt_ptr->domain.delta = delta;
    return true;
}

std::ostream& operator << (std::ostream&, const OptionSet& opt);
std::istream& operator >> (std::istream&, OptionSet& opt);

}
}
