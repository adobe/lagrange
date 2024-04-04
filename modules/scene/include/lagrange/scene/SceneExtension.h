/*
 * Copyright 2024 Adobe. All rights reserved.
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

#include <lagrange/scene/api.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/invalid.h>
#include <lagrange/utils/span.h>
#include <lagrange/utils/value_ptr.h>

#include <any>
#include <cstring>
#include <map>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace lagrange {
namespace scene {

// Json-like value used in scene extensions
class LA_SCENE_API Value
{
public:
    typedef std::vector<Value> Array;
    typedef std::map<std::string, Value> Object;
    typedef std::vector<unsigned char> Buffer;
    using variant_type = std::variant<bool, int, double, std::string, Buffer, Array, Object>;

    /**
     * Checks if the type is in the variant.
     */
    template <typename T>
    static constexpr bool is_variant_type()
    {
        return variant_index<T>() < (std::variant_size_v<variant_type>);
    }

    /**
     * Returns index of the element type in the variant. Returns variant size if not found.
     */
    template <typename T, std::size_t index = 0>
    static constexpr std::size_t variant_index()
    {
        if constexpr (index == std::variant_size_v<variant_type>) {
            return index;
        } else if constexpr (std::is_same_v<std::variant_alternative_t<index, variant_type>, T>) {
            return index;
        } else {
            return variant_index<T, index + 1>();
        }
    }

    static constexpr size_t bool_index() { return variant_index<bool>(); }
    static constexpr size_t int_index() { return variant_index<int>(); }
    static constexpr size_t real_index() { return variant_index<double>(); }
    static constexpr size_t string_index() { return variant_index<std::string>(); }
    static constexpr size_t buffer_index() { return variant_index<Buffer>(); }
    static constexpr size_t array_index() { return variant_index<Array>(); }
    static constexpr size_t object_index() { return variant_index<Object>(); }

    static Value create_buffer() { return Value(Buffer()); }
    static Value create_array() { return Value(Array()); }
    static Value create_object() { return Value(Object()); }

    Value() = default;
    explicit Value(bool b) { value = b; }
    explicit Value(int i) { value = i; }
    explicit Value(double n) { value = n; }
    explicit Value(std::string s) { value = std::move(s); }
    explicit Value(std::string_view s) { value = std::string(s); }
    explicit Value(const char* s) { value = std::string(s); }
    explicit Value(span<unsigned char> s)
    {
        Buffer vec(s.size());
        std::memcpy(vec.data(), s.data(), s.size());
        value = std::move(vec);
    }
    explicit Value(const Buffer& v) { value = v; }
    explicit Value(Buffer&& v) { value = std::move(v); }
    explicit Value(const Array& a) { value = a; }
    explicit Value(Array&& a) { value = std::move(a); }
    explicit Value(const Object& o) { value = o; }
    explicit Value(Object&& o) { value = std::move(o); }

    template <typename T>
    bool is_type() const
    {
        return std::holds_alternative<T>(value);
    }
    bool is_bool() const { return is_type<bool>(); }
    bool is_int() const { return is_type<int>(); }
    bool is_real() const { return is_type<double>(); }
    bool is_number() const { return is_int() || is_real(); }
    bool is_string() const { return is_type<std::string>(); }
    bool is_buffer() const { return is_type<Buffer>(); }
    bool is_array() const { return is_type<Array>(); }
    bool is_object() const { return is_type<Object>(); }
    size_t get_type_index() const { return value.index(); }

    template <typename T>
    const T& get() const
    {
        return std::get<T>(value);
    }
    template <typename T>
    T& get()
    {
        return std::get<T>(value);
    }
    bool get_bool() const { return get<bool>(); }
    int get_int() const { return get<int>(); }
    double get_real() const { return get<double>(); }
    const std::string& get_string() const { return get<std::string>(); }
    std::string get_string() { return get<std::string>(); }
    const Buffer& get_buffer() const { return get<Buffer>(); }
    Buffer& get_buffer() { return get<Buffer>(); }
    const Array& get_array() const { return get<Array>(); }
    Array& get_array() { return get<Array>(); }
    const Object& get_object() const { return get<Object>(); }
    Object& get_object() { return get<Object>(); }

    template <typename T>
    void set(T t)
    {
        value = t;
    }
    void set_bool(bool b) { value = b; }
    void set_int(int i) { value = i; }
    void set_real(double n) { value = n; }

    // Only valid for array values:
    const Value& operator[](size_t idx) const
    {
        la_debug_assert(is_array());
        return get_array()[idx];
    }
    Value& operator[](size_t idx)
    {
        la_debug_assert(is_array());
        return get_array()[idx];
    }

    // Only valid for object values:
    bool has(const std::string& key) const { return get_object().find(key) != get_object().end(); }
    const Value& operator[](const std::string& key) const
    {
        la_debug_assert(is_object());
        return get_object().find(key)->second;
    }
    Value& operator[](const std::string& key)
    {
        la_debug_assert(is_object());
        return get_object().find(key)->second;
    }

    // Only valid for some value types (string, array, object, buffer).
    size_t size() const
    {
        if (is_string()) return get_string().size();
        if (is_buffer()) return get_buffer().size();
        if (is_array()) return get_array().size();
        if (is_object()) return get_object().size();
        return 0;
    }

protected:
    variant_type value;
};

struct LA_SCENE_API UserDataConverter
{
    virtual bool is_supported(const std::string& key) const = 0;
    virtual bool can_read(const std::string& key) const { return is_supported(key); }
    virtual bool can_write(const std::string& key) const { return is_supported(key); }

    virtual std::any read(const Value& value) const = 0;
    virtual Value write(const std::any& value) const = 0;
};

struct LA_SCENE_API Extensions
{
    /**
     * A map of extensions as json-like Value objects.
     */
    std::unordered_map<std::string, Value> data;

    /**
     * A map of extensions as user-defined objects, stored in an std::any.
     * Those are converted from/to the default Value with a UserDataConverter during I/O.
     */
    std::unordered_map<std::string, std::any> user_data;

    size_t size() const { return data.size() + user_data.size(); }
    bool empty() const { return data.empty() && user_data.empty(); }
};

} // namespace scene
} // namespace lagrange
