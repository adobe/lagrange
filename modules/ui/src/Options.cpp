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
#include <nlohmann/json.hpp>

#include <lagrange/ui/Options.h>

namespace lagrange {
namespace ui {

using json = nlohmann::json;

OptionSet& OptionSet::operator[](const std::string& childName)
{
    return *get_child_ptr(childName);
}

const OptionSet& OptionSet::operator[](const std::string& childName) const
{
    return *children.at(childName);
}

std::shared_ptr<OptionSet> OptionSet::get_child_ptr(const std::string& childName)
{
    auto& ptr = children[childName];
    if (!ptr) ptr = std::make_shared<OptionSet>();
    return ptr;
}

bool OptionSet::add_child(const std::string& child_name, std::shared_ptr<OptionSet> child)
{
    auto it = children.find(child_name);
    if (it != children.end()) return false;

    children[child_name] = child;
    return true;
}

bool OptionSet::erase(const std::string& optionName)
{
    auto it = options.find(optionName);
    if (it == options.end()) return false;

    options.erase(it);
    return true;
}

bool OptionSet::has(const std::string& optionName) const
{
    auto it = options.find(optionName);
    return (it != options.end());
}

void OptionSet::clear()
{
    options.clear();
    children.clear();
}

std::map<std::string, std::shared_ptr<lagrange::ui::Option>>& OptionSet::get_options()
{
    return options;
}

const std::map<std::string, std::shared_ptr<lagrange::ui::Option>>& OptionSet::get_options() const
{
    return options;
}

std::map<std::string, std::shared_ptr<lagrange::ui::OptionSet>>& OptionSet::get_children()
{
    return children;
}

const std::map<std::string, std::shared_ptr<lagrange::ui::OptionSet>>& OptionSet::get_children()
    const
{
    return children;
}

size_t OptionSet::num_children() const
{
    return children.size();
}

size_t OptionSet::num_options() const
{
    return options.size();
}

template <typename T>
json eigenJson(const T& v)
{
    json j;
    for (auto i = 0; i < v.size(); i++) {
        j.push_back(v(i));
    }
    return j;
}

template <typename T>
json toJson(const T& v)
{
    return json(v);
}

json toJson(const Eigen::Vector2f& v)
{
    return eigenJson(v);
}
json toJson(const Eigen::Vector3f& v)
{
    return eigenJson(v);
}
json toJson(const Eigen::Vector4f& v)
{
    return eigenJson(v);
}
json toJson(const Eigen::Vector2i& v)
{
    return eigenJson(v);
}
json toJson(const Eigen::Vector3i& v)
{
    return eigenJson(v);
}
json toJson(const Eigen::Vector4i& v)
{
    return eigenJson(v);
}
json toJson(const Eigen::Matrix2f& v)
{
    return eigenJson(v);
}
json toJson(const Eigen::Matrix3f& v)
{
    return eigenJson(v);
}
json toJson(const Eigen::Matrix4f& v)
{
    return eigenJson(v);
}


json toJson(const std::string& v)
{
    return json(v);
}
json toJson(const Color& v)
{
    return toJson(v.to_vec4());
}

json toJson(const OptionSet& optSet)
{
    json j;
    for (auto& it : optSet.get_children()) {
        j[it.first] = toJson(*it.second);
    }

    for (auto& it : optSet.get_options()) {
        std::visit(
            [&](auto&& arg) {
                j[it.first] = {{"type", it.second->value.index()}, {"data", toJson(arg)}};
            },
            it.second->value);
    }
    return j;
}
json toJson(const std::shared_ptr<OptionSet>& optSet)
{
    return toJson(*optSet);
}

std::ostream& operator<<(std::ostream& os, const OptionSet& optSet)
{
    os << toJson(optSet).dump(4);
    return os;
}


template <typename T>
T jsonToEigen(const json& j)
{
    T v;
    for (auto i = 0; i < T::ColsAtCompileTime * T::RowsAtCompileTime; i++) v(i) = j[i];
    return v;
}

template <typename T>
void fromJson(const json& j, T& v)
{
    v = j.get<T>();
}

void fromJson(const json& j, Eigen::Vector2f& v)
{
    v = jsonToEigen<Eigen::Vector2f>(j);
}
void fromJson(const json& j, Eigen::Vector3f& v)
{
    v = jsonToEigen<Eigen::Vector3f>(j);
}
void fromJson(const json& j, Eigen::Vector4f& v)
{
    v = jsonToEigen<Eigen::Vector4f>(j);
}
void fromJson(const json& j, Eigen::Vector2i& v)
{
    v = jsonToEigen<Eigen::Vector2i>(j);
}
void fromJson(const json& j, Eigen::Vector3i& v)
{
    v = jsonToEigen<Eigen::Vector3i>(j);
}
void fromJson(const json& j, Eigen::Vector4i& v)
{
    v = jsonToEigen<Eigen::Vector4i>(j);
}
void fromJson(const json& j, Eigen::Matrix2f& v)
{
    v = jsonToEigen<Eigen::Matrix2f>(j);
}
void fromJson(const json& j, Eigen::Matrix3f& v)
{
    v = jsonToEigen<Eigen::Matrix3f>(j);
}
void fromJson(const json& j, Eigen::Matrix4f& v)
{
    v = jsonToEigen<Eigen::Matrix4f>(j);
}


void fromJson(const json& j, Color& v)
{
    auto tmp = jsonToEigen<Eigen::Vector4f>(j);
    v = Color(tmp);
}

template <size_t idx>
void set(OptionSet& opt, json::const_iterator& it)
{
    using T = std::variant_alternative_t<idx, OptionType>;
    T val;
    fromJson((*it)["data"], val);
    opt.set<T>(it.key(), val);
};

OptionSet fromJson(const json& j)
{
    OptionSet opt;

    for (auto it = j.begin(); it != j.end(); ++it) {
        if ((*it).find("type") == (*it).end()) {
            opt[it.key()] = fromJson(*it);
        } else {
            switch ((*it)["type"].get<int>()) {
            case 0: set<0>(opt, it); break;
            case 1: set<1>(opt, it); break;
            case 2: set<2>(opt, it); break;
            case 3: set<3>(opt, it); break;
            case 4: set<4>(opt, it); break;
            case 5: set<5>(opt, it); break;
            case 6: set<6>(opt, it); break;
            case 7: set<7>(opt, it); break;
            case 8: set<8>(opt, it); break;
            case 9: set<9>(opt, it); break;
            case 10: set<10>(opt, it); break;
            case 11: set<11>(opt, it); break;
            case 12: set<12>(opt, it); break;
            case 13: set<13>(opt, it); break;
            case 14: set<14>(opt, it); break;
            case 15: set<15>(opt, it); break;
            };
        }
    }

    return opt;
}

std::istream& operator>>(std::istream& is, OptionSet& opt)
{
    json j;
    is >> j;
    opt = fromJson(j);
    return is;
}
} // namespace ui
} // namespace lagrange
