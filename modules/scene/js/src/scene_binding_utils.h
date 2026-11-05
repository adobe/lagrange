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

#include "bind_types.h"

#include <emscripten/val.h>

#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

namespace lagrange::js::bind {

using emscripten::val;

inline bool has_scene_field(const val& object, const char* key)
{
    return !object.isUndefined() && !object.isNull() && !object[key].isUndefined();
}

inline scene::ElementId read_scene_id(const val& value, size_t count, std::string_view kind)
{
    if (value.typeOf().as<std::string>() != "number") {
        throw std::invalid_argument(std::string(kind) + " id must be a number");
    }
    const double id = value.as<double>();
    if (!std::isfinite(id) || id < 0 || std::floor(id) != id || id >= double(count)) {
        throw std::out_of_range(std::string(kind) + " id is out of range");
    }
    return static_cast<scene::ElementId>(id);
}

inline float read_scene_float(const val& value, const char* field)
{
    if (value.typeOf().as<std::string>() != "number") {
        throw std::invalid_argument(std::string(field) + " must be a number");
    }
    const double number = value.as<double>();
    if (!std::isfinite(number) || std::abs(number) > std::numeric_limits<float>::max()) {
        throw std::invalid_argument(std::string(field) + " must be a finite float");
    }
    return static_cast<float>(number);
}

inline unsigned read_scene_array_length(const val& input, const char* field)
{
    if (input.isNull() || input.isUndefined() ||
        input["length"].typeOf().as<std::string>() != "number") {
        throw std::invalid_argument(std::string(field) + " must be array-like");
    }
    const double length = input["length"].as<double>();
    if (!std::isfinite(length) || length < 0 || std::floor(length) != length ||
        length > std::numeric_limits<unsigned>::max()) {
        throw std::invalid_argument(std::string(field) + " has an invalid length");
    }
    return static_cast<unsigned>(length);
}

template <typename Derived>
void read_scene_vector(const val& input, Eigen::MatrixBase<Derived>& output, const char* field)
{
    if (read_scene_array_length(input, field) != unsigned(output.size())) {
        throw std::invalid_argument(std::string(field) + " has the wrong length");
    }
    for (Eigen::Index i = 0; i < output.size(); ++i) {
        output(i) = read_scene_float(input[unsigned(i)], field);
    }
}

template <typename Derived>
val copy_scene_vector(const Eigen::MatrixBase<Derived>& vector)
{
    static_assert(std::is_same_v<typename Derived::Scalar, float>);
    return val::global("Float32Array")
        .new_(emscripten::typed_memory_view(size_t(vector.size()), vector.derived().data()));
}

template <typename Container>
val copy_scene_ids(const Container& ids)
{
    val result = val::array();
    for (const auto id : ids) result.call<void>("push", id);
    return result;
}

} // namespace lagrange::js::bind
