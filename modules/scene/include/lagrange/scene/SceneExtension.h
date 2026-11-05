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

#include <lagrange/internal/SafeVector.h>
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

///
/// @addtogroup module-scene
/// @{
///

/// JSON-like owning value for scene extensions, with additional binary-buffer support and no null
/// alternative. A default-constructed value is false. Numeric getters require the exact stored
/// type; integers and doubles are not implicitly converted.
class LA_SCENE_API Value
{
public:
    /// Ordered list of values.
    using Array = std::vector<Value>;
    // using Array = SafeVector<Value>; // TODO: Switch to this?
    /// String-keyed map of values.
    using Object = std::map<std::string, Value>;
    /// Owning sequence of raw bytes.
    using Buffer = std::vector<unsigned char>;
    /// Stored alternative: bool, int, double, string, buffer, array, or object.
    using variant_type = std::variant<bool, int, double, std::string, Buffer, Array, Object>;

    /// Checks if the type is in the variant.
    template <typename T>
    static constexpr bool is_variant_type()
    {
        return variant_index<T>() < (std::variant_size_v<variant_type>);
    }

    /// Returns index of the element type in the variant. Returns variant size if not found.
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

    /// Variant index of the boolean alternative.
    static constexpr size_t bool_index() { return variant_index<bool>(); }
    /// Variant index of the integer alternative.
    static constexpr size_t int_index() { return variant_index<int>(); }
    /// Variant index of the real-number alternative.
    static constexpr size_t real_index() { return variant_index<double>(); }
    /// Variant index of the string alternative.
    static constexpr size_t string_index() { return variant_index<std::string>(); }
    /// Variant index of the buffer alternative.
    static constexpr size_t buffer_index() { return variant_index<Buffer>(); }
    /// Variant index of the array alternative.
    static constexpr size_t array_index() { return variant_index<Array>(); }
    /// Variant index of the object alternative.
    static constexpr size_t object_index() { return variant_index<Object>(); }

    /// Creates an empty binary buffer.
    static Value create_buffer() { return Value(Buffer()); }
    /// Creates an empty array.
    static Value create_array() { return Value(Array()); }
    /// Creates an empty object.
    static Value create_object() { return Value(Object()); }

    /// Creates a false boolean value.
    Value() = default;
    /// Stores a boolean.
    explicit Value(bool b) { value = b; }
    /// Stores an integer. Integers are not implicitly converted to or from reals.
    explicit Value(int i) { value = i; }
    /// Stores a real number. Reals are not implicitly converted to or from integers.
    explicit Value(double n) { value = n; }
    /// Stores a string, taking ownership.
    explicit Value(std::string s)
        : value(std::move(s))
    {}
    /// Stores a copy of the string view.
    explicit Value(std::string_view s) { value = std::string(s); }
    /// Stores a copy of the C string.
    explicit Value(const char* s) { value = std::string(s); }
    /// Stores a copy of the byte span as a buffer.
    explicit Value(span<unsigned char> s)
    {
        Buffer vec(s.size());
        std::memcpy(vec.data(), s.data(), s.size());
        value = std::move(vec);
    }
    /// Stores a copy of the buffer.
    explicit Value(const Buffer& v) { value = v; }
    /// Stores the buffer, taking ownership.
    explicit Value(Buffer&& v) { value = std::move(v); }
    /// Stores a copy of the array.
    explicit Value(const Array& a) { value = a; }
    /// Stores the array, taking ownership.
    explicit Value(Array&& a) { value = std::move(a); }
    /// Stores a copy of the object.
    explicit Value(const Object& o) { value = o; }
    /// Stores the object, taking ownership.
    explicit Value(Object&& o) { value = std::move(o); }

    /// Whether the stored value has type T.
    template <typename T>
    bool is_type() const
    {
        return std::holds_alternative<T>(value);
    }
    /// Whether the stored value is a boolean.
    bool is_bool() const { return is_type<bool>(); }
    /// Whether the stored value is an integer.
    bool is_int() const { return is_type<int>(); }
    /// Whether the stored value is a real number.
    bool is_real() const { return is_type<double>(); }
    /// Whether the stored value is an integer or a real number.
    bool is_number() const { return is_int() || is_real(); }
    /// Whether the stored value is a string.
    bool is_string() const { return is_type<std::string>(); }
    /// Whether the stored value is a binary buffer.
    bool is_buffer() const { return is_type<Buffer>(); }
    /// Whether the stored value is an array.
    bool is_array() const { return is_type<Array>(); }
    /// Whether the stored value is an object.
    bool is_object() const { return is_type<Object>(); }
    /// Index of the stored alternative within the variant.
    size_t get_type_index() const { return value.index(); }

    /// Returns the stored value by reference; throws std::bad_variant_access for a different type.
    template <typename T>
    const T& get() const
    {
        return std::get<T>(value);
    }
    /// Returns the stored value by reference; throws std::bad_variant_access for a different type.
    template <typename T>
    T& get()
    {
        return std::get<T>(value);
    }
    /// Returns the stored boolean; throws std::bad_variant_access for a different type.
    bool get_bool() const { return get<bool>(); }
    /// Returns the stored integer; throws std::bad_variant_access for a different type.
    int get_int() const { return get<int>(); }
    /// Returns the stored real number; throws std::bad_variant_access for a different type.
    double get_real() const { return get<double>(); }
    /// Returns the stored string; throws std::bad_variant_access for a different type.
    const std::string& get_string() const { return get<std::string>(); }
    /// Returns a copy of the stored string; use get<std::string>() for mutable reference access.
    std::string get_string() { return get<std::string>(); }
    /// Returns the stored buffer; throws std::bad_variant_access for a different type.
    const Buffer& get_buffer() const { return get<Buffer>(); }
    /// Mutable access to the stored buffer; throws std::bad_variant_access for a different type.
    Buffer& get_buffer() { return get<Buffer>(); }
    /// Returns the stored array; throws std::bad_variant_access for a different type.
    const Array& get_array() const { return get<Array>(); }
    /// Mutable access to the stored array; throws std::bad_variant_access for a different type.
    Array& get_array() { return get<Array>(); }
    /// Returns the stored object; throws std::bad_variant_access for a different type.
    const Object& get_object() const { return get<Object>(); }
    /// Mutable access to the stored object; throws std::bad_variant_access for a different type.
    Object& get_object() { return get<Object>(); }

    /// Replaces the stored value. T must be one of the variant alternatives.
    template <typename T>
    void set(T t)
    {
        value = t;
    }
    /// Stores a boolean.
    void set_bool(bool b) { value = b; }
    /// Stores an integer.
    void set_int(int i) { value = i; }
    /// Stores a real number.
    void set_real(double n) { value = n; }

    /// Array access; requires an array value and idx < size(). Does not grow the array.
    const Value& operator[](size_t idx) const
    {
        la_debug_assert(is_array());
        return get_array()[idx];
    }
    /// Mutable array access; requires an array value and idx < size(). Does not grow the array.
    Value& operator[](size_t idx)
    {
        la_debug_assert(is_array());
        return get_array()[idx];
    }

    /// Tests for a key; requires an object value.
    bool has(const std::string& key) const { return get_object().find(key) != get_object().end(); }
    /// Object access; the key must already exist. Does not insert missing keys.
    const Value& operator[](const std::string& key) const
    {
        la_debug_assert(is_object());
        return get_object().find(key)->second;
    }
    /// Mutable object access; the key must already exist. Does not insert missing keys.
    Value& operator[](const std::string& key)
    {
        la_debug_assert(is_object());
        return get_object().find(key)->second;
    }

    /// Returns the string length, buffer size in bytes, or number of array/object entries.
    /// Returns zero for booleans and numbers.
    size_t size() const
    {
        if (is_string()) return get_string().size();
        if (is_buffer()) return get_buffer().size();
        if (is_array()) return get_array().size();
        if (is_object()) return get_object().size();
        return 0;
    }

protected:
    /// Stored alternative. A default-constructed value holds false.
    variant_type value;
};

/// Converts named scene extensions between generic Value objects and application-defined data
/// during I/O. Register converters through the relevant I/O options.
struct LA_SCENE_API UserDataConverter
{
    /// Whether this converter supports the named extension.
    virtual bool is_supported(const std::string& key) const = 0;
    /// Whether the named extension can be read; defaults to is_supported().
    virtual bool can_read(const std::string& key) const { return is_supported(key); }
    /// Whether the named extension can be written; defaults to is_supported().
    virtual bool can_write(const std::string& key) const { return is_supported(key); }

    /// Converts a supported extension's generic representation to application-defined data.
    virtual std::any read(const Value& value) const = 0;
    /// Converts supported application-defined data to a generic extension representation.
    virtual Value write(const std::any& value) const = 0;
};

/// Named scene extensions, stored either as generic values or application-defined objects.
struct LA_SCENE_API Extensions
{
    /// A map of extensions as json-like Value objects.
    std::unordered_map<std::string, Value> data;

    /// A map of extensions as user-defined objects, stored in an std::any.
    /// Those are converted from/to the default Value with a UserDataConverter during I/O.
    std::unordered_map<std::string, std::any> user_data;

    /// Total number of entries in both maps; a key present in both is counted twice.
    size_t size() const { return data.size() + user_data.size(); }
    /// Whether both maps are empty.
    bool empty() const { return data.empty() && user_data.empty(); }
};

/// @}

} // namespace scene
} // namespace lagrange
