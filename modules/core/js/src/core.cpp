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

#include "bind_types.h"

#include <lagrange/Attribute.h>
#include <lagrange/AttributeTypes.h>
#include <lagrange/AttributeValueType.h>
#include <lagrange/IndexedAttribute.h>
#include <lagrange/unify_index_buffer.h>
#include <lagrange/utils/span.h>

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace lagrange::js::bind {
namespace {

using val = emscripten::val;

// Splits vertices at attribute discontinuities so all indexed attributes become per-vertex.
// The returned mesh has a unified index buffer suitable for WebGL / zero-copy views.
MeshType unify_index_buffer_js(const MeshType& mesh)
{
    return lagrange::unify_index_buffer<Scalar, Index>(mesh);
}

void mesh_add_vertex(MeshType& mesh, const emscripten::val& vertex_coords)
{
    const Index dim = mesh.get_dimension();
    const unsigned len = vertex_coords["length"].as<unsigned>();
    if (len != static_cast<unsigned>(dim)) {
        throw std::runtime_error("addVertex: array length must equal mesh dimension");
    }
    std::vector<Scalar> coords(static_cast<size_t>(dim));
    for (Index i = 0; i < dim; ++i) {
        coords[static_cast<size_t>(i)] = vertex_coords[i].as<Scalar>();
    }
    mesh.add_vertex(span<const Scalar>(coords.data(), coords.size()));
}

void mesh_add_triangles(MeshType& mesh, const emscripten::val& triangle_indices)
{
    const unsigned len = triangle_indices["length"].as<unsigned>();
    if (len % 3 != 0) {
        throw std::runtime_error("addTriangles: array length must be divisible by 3");
    }
    const Index num_facets = static_cast<Index>(len / 3);
    std::vector<Index> buf(static_cast<size_t>(len));
    for (unsigned i = 0; i < len; ++i) {
        buf[i] = triangle_indices[i].as<Index>();
    }
    mesh.add_triangles(num_facets, span<const Index>(buf.data(), buf.size()));
}

emscripten::val copy_span_f32(span<const Scalar> s)
{
    if (s.empty()) {
        return emscripten::val::global("Float32Array").new_(0);
    }
    std::vector<Scalar> buf(s.begin(), s.end());
    return emscripten::val::global("Float32Array")
        .new_(emscripten::typed_memory_view(buf.size(), buf.data()));
}

emscripten::val copy_span_u32(span<const Index> s)
{
    if (s.empty()) {
        return emscripten::val::global("Uint32Array").new_(0);
    }
    std::vector<Index> buf(s.begin(), s.end());
    return emscripten::val::global("Uint32Array")
        .new_(emscripten::typed_memory_view(buf.size(), buf.data()));
}

emscripten::val mesh_get_position(const MeshType& mesh, Index vertex_id)
{
    return copy_span_f32(mesh.get_position(vertex_id));
}

emscripten::val mesh_get_facet_vertices(const MeshType& mesh, Index facet_id)
{
    return copy_span_u32(mesh.get_facet_vertices(facet_id));
}

bool mesh_has_attribute(const MeshType& mesh, const std::string& name)
{
    return mesh.has_attribute(name);
}

// --- Attribute element / usage <-> string ---

AttributeElement parse_element(const std::string& s)
{
    if (s == "vertex") return AttributeElement::Vertex;
    if (s == "facet") return AttributeElement::Facet;
    if (s == "edge") return AttributeElement::Edge;
    if (s == "corner") return AttributeElement::Corner;
    if (s == "value") return AttributeElement::Value;
    if (s == "indexed") return AttributeElement::Indexed;
    throw std::runtime_error("Unknown attribute element: " + s);
}

std::string element_name(AttributeElement e)
{
    switch (e) {
    case AttributeElement::Vertex: return "vertex";
    case AttributeElement::Facet: return "facet";
    case AttributeElement::Edge: return "edge";
    case AttributeElement::Corner: return "corner";
    case AttributeElement::Value: return "value";
    case AttributeElement::Indexed: return "indexed";
    default: return "unknown";
    }
}

AttributeUsage parse_usage(const std::string& s)
{
    if (s == "vector") return AttributeUsage::Vector;
    if (s == "scalar") return AttributeUsage::Scalar;
    if (s == "position") return AttributeUsage::Position;
    if (s == "normal") return AttributeUsage::Normal;
    if (s == "tangent") return AttributeUsage::Tangent;
    if (s == "bitangent") return AttributeUsage::Bitangent;
    if (s == "color") return AttributeUsage::Color;
    if (s == "uv") return AttributeUsage::UV;
    if (s == "vertexIndex") return AttributeUsage::VertexIndex;
    if (s == "facetIndex") return AttributeUsage::FacetIndex;
    if (s == "cornerIndex") return AttributeUsage::CornerIndex;
    if (s == "edgeIndex") return AttributeUsage::EdgeIndex;
    if (s == "string") return AttributeUsage::String;
    throw std::runtime_error("Unknown attribute usage: " + s);
}

std::string usage_name(AttributeUsage u)
{
    switch (u) {
    case AttributeUsage::Vector: return "vector";
    case AttributeUsage::Scalar: return "scalar";
    case AttributeUsage::Position: return "position";
    case AttributeUsage::Normal: return "normal";
    case AttributeUsage::Tangent: return "tangent";
    case AttributeUsage::Bitangent: return "bitangent";
    case AttributeUsage::Color: return "color";
    case AttributeUsage::UV: return "uv";
    case AttributeUsage::VertexIndex: return "vertexIndex";
    case AttributeUsage::FacetIndex: return "facetIndex";
    case AttributeUsage::CornerIndex: return "cornerIndex";
    case AttributeUsage::EdgeIndex: return "edgeIndex";
    case AttributeUsage::String: return "string";
    default: return "unknown";
    }
}

template <typename T>
const char* dtype_name();
template <>
const char* dtype_name<int8_t>()
{
    return "int8";
}
template <>
const char* dtype_name<int16_t>()
{
    return "int16";
}
template <>
const char* dtype_name<int32_t>()
{
    return "int32";
}
template <>
const char* dtype_name<int64_t>()
{
    return "int64";
}
template <>
const char* dtype_name<uint8_t>()
{
    return "uint8";
}
template <>
const char* dtype_name<uint16_t>()
{
    return "uint16";
}
template <>
const char* dtype_name<uint32_t>()
{
    return "uint32";
}
template <>
const char* dtype_name<uint64_t>()
{
    return "uint64";
}
template <>
const char* dtype_name<float>()
{
    return "float32";
}
template <>
const char* dtype_name<double>()
{
    return "float64";
}

template <typename T>
const char* typed_array_ctor_name();
template <>
const char* typed_array_ctor_name<int8_t>()
{
    return "Int8Array";
}
template <>
const char* typed_array_ctor_name<int16_t>()
{
    return "Int16Array";
}
template <>
const char* typed_array_ctor_name<int32_t>()
{
    return "Int32Array";
}
template <>
const char* typed_array_ctor_name<uint8_t>()
{
    return "Uint8Array";
}
template <>
const char* typed_array_ctor_name<uint16_t>()
{
    return "Uint16Array";
}
template <>
const char* typed_array_ctor_name<uint32_t>()
{
    return "Uint32Array";
}
template <>
const char* typed_array_ctor_name<float>()
{
    return "Float32Array";
}
template <>
const char* typed_array_ctor_name<double>()
{
    return "Float64Array";
}

// int64_t/uint64_t widen to Float64Array (no safe native JS typed-array counterpart) only if
// every value round-trips exactly; a lossy value throws instead of returning corrupted data.
template <typename T>
val to_typed_array(lagrange::span<const T> data)
{
    if constexpr (std::is_same_v<T, int64_t> || std::is_same_v<T, uint64_t>) {
        std::vector<double> buf(data.size());
        for (size_t i = 0; i < data.size(); ++i) {
            double d = static_cast<double>(data[i]);
            if (static_cast<T>(d) != data[i]) {
                throw std::runtime_error(
                    "Attribute value at position " + std::to_string(i) +
                    " cannot be represented exactly as a JS number");
            }
            buf[i] = d;
        }
        return val::global("Float64Array")
            .new_(emscripten::typed_memory_view(buf.size(), buf.data()));
    } else {
        std::vector<T> buf(data.begin(), data.end());
        return val::global(typed_array_ctor_name<T>())
            .new_(emscripten::typed_memory_view(buf.size(), buf.data()));
    }
}

// Rejects BigInt64Array/BigUint64Array inputs, since TypedArray.prototype.set() otherwise
// throws an opaque native TypeError when mixing BigInt and non-BigInt content types.
void reject_bigint_array(const val& arr)
{
    if (arr.isUndefined() || arr.isNull()) return;
    std::string ctor_name = arr["constructor"]["name"].as<std::string>();
    if (ctor_name == "BigInt64Array" || ctor_name == "BigUint64Array") {
        throw std::runtime_error(
            "BigInt64Array/BigUint64Array are not supported; pass a regular Array or "
            "Float64Array instead");
    }
}

// Bulk-copies a JS array-like into a native buffer via TypedArray.prototype.set() instead of
// one embind property lookup + val::as<T>() round trip per element (which dominates at scale).
template <typename T>
void bulk_copy_into(const val& source, T* dest, size_t count)
{
    if (count == 0) return;
    val view(emscripten::typed_memory_view(count, dest));
    view.call<void>("set", source);
}

// Converts already-read values into mesh Index values, validating each is a non-negative
// integer representable by Index (casting an out-of-range double otherwise is UB).
std::vector<Index> to_index_vector(const std::vector<double>& raw, const char* context)
{
    std::vector<Index> out(raw.size());
    for (size_t i = 0; i < raw.size(); ++i) {
        double v = raw[i];
        if (!(v >= 0) || v > static_cast<double>(std::numeric_limits<Index>::max()) ||
            v != std::floor(v)) {
            throw std::runtime_error(
                std::string(context) + ": value at position " + std::to_string(i) +
                " is not a valid non-negative integer index");
        }
        out[i] = static_cast<Index>(v);
    }
    return out;
}

// Validates a double is finite and in range before casting (UB otherwise). `>=` on the upper
// bound matters: ValueType::max() (2^n - 1) rounds *up* to 2^n as a double, so `>` would admit it.
template <typename ValueType>
ValueType to_wide_int(double v, const char* context, size_t i)
{
    if (!std::isfinite(v) || v != std::floor(v) ||
        v < static_cast<double>(std::numeric_limits<ValueType>::min()) ||
        v >= static_cast<double>(std::numeric_limits<ValueType>::max())) {
        throw std::runtime_error(
            std::string(context) + ": value at position " + std::to_string(i) +
            " is not a valid integer representable by " + dtype_name<ValueType>());
    }
    return static_cast<ValueType>(v);
}

// Mirrors AttributeBase's channel validation (Attribute.cpp) plus SurfaceMesh's
// dimension-specific checks, run before the core library registers the attribute's name.
void validate_usage_channels(AttributeUsage usage, size_t num_channels, Index dim)
{
    switch (usage) {
    case AttributeUsage::Vector:
        if (num_channels < 1) {
            throw std::runtime_error("createAttribute: numChannels must be at least 1");
        }
        break;
    case AttributeUsage::Scalar:
    case AttributeUsage::VertexIndex:
    case AttributeUsage::FacetIndex:
    case AttributeUsage::CornerIndex:
    case AttributeUsage::EdgeIndex:
    case AttributeUsage::String:
        if (num_channels != 1) {
            throw std::runtime_error(
                "createAttribute: usage '" + usage_name(usage) + "' requires numChannels: 1");
        }
        break;
    case AttributeUsage::Position:
        if (num_channels != static_cast<size_t>(dim)) {
            throw std::runtime_error(
                "createAttribute: usage 'position' requires numChannels: " + std::to_string(dim));
        }
        break;
    case AttributeUsage::Normal:
    case AttributeUsage::Tangent:
    case AttributeUsage::Bitangent:
        if (num_channels != static_cast<size_t>(dim) &&
            num_channels != static_cast<size_t>(dim) + 1) {
            throw std::runtime_error(
                "createAttribute: usage '" + usage_name(usage) + "' requires numChannels: " +
                std::to_string(dim) + " or " + std::to_string(dim + 1));
        }
        break;
    case AttributeUsage::Color:
        if (num_channels < 1 || num_channels > 4) {
            throw std::runtime_error(
                "createAttribute: usage 'color' requires numChannels between 1 and 4");
        }
        break;
    case AttributeUsage::UV:
        if (num_channels != 2) {
            throw std::runtime_error("createAttribute: usage 'uv' requires numChannels: 2");
        }
        break;
    }
}

std::string infer_dtype(const val& data)
{
    std::string ctor_name = data["constructor"]["name"].as<std::string>();
    if (ctor_name == "Int8Array") return "int8";
    if (ctor_name == "Int16Array") return "int16";
    if (ctor_name == "Int32Array") return "int32";
    if (ctor_name == "Uint8Array" || ctor_name == "Uint8ClampedArray") return "uint8";
    if (ctor_name == "Uint16Array") return "uint16";
    if (ctor_name == "Uint32Array") return "uint32";
    if (ctor_name == "Float32Array") return "float32";
    if (ctor_name == "Float64Array") return "float64";
    return "float64"; // Plain JS Array (or anything else array-like): default to float64.
}

// Dispatches on the attribute's runtime value type, over all 10 lagrange attribute value types.
template <typename Func>
auto visit_attribute(const MeshType& mesh, AttributeId id, Func&& func)
{
    auto vt = mesh.get_attribute_base(id).get_value_type();
#define LA_X_visit_attribute(_, ValueType)                        \
    if (vt == lagrange::make_attribute_value_type<ValueType>()) { \
        return func(mesh.template get_attribute<ValueType>(id));  \
    }
    LA_ATTRIBUTE_X(visit_attribute, 0)
#undef LA_X_visit_attribute
    throw std::runtime_error("Unsupported attribute value type");
}

template <typename Func>
auto visit_indexed_attribute(const MeshType& mesh, AttributeId id, Func&& func)
{
    auto vt = mesh.get_attribute_base(id).get_value_type();
#define LA_X_visit_indexed_attribute(_, ValueType)                       \
    if (vt == lagrange::make_attribute_value_type<ValueType>()) {        \
        return func(mesh.template get_indexed_attribute<ValueType>(id)); \
    }
    LA_ATTRIBUTE_X(visit_indexed_attribute, 0)
#undef LA_X_visit_indexed_attribute
    throw std::runtime_error("Unsupported attribute value type");
}

AttributeId create_attribute_js(MeshType& mesh, const std::string& name, val opts)
{
    if (opts.isUndefined() || opts.isNull()) {
        throw std::runtime_error("createAttribute: options object with 'element' is required");
    }
    auto element_val = opts["element"];
    if (element_val.isUndefined()) {
        throw std::runtime_error("createAttribute: 'element' is required");
    }
    AttributeElement element = parse_element(element_val.as<std::string>());

    size_t num_channels = 1;
    auto num_channels_val = opts["numChannels"];
    if (!num_channels_val.isUndefined() && !num_channels_val.isNull()) {
        double nc = num_channels_val.as<double>();
        if (!std::isfinite(nc) || nc < 1 || nc != std::floor(nc) ||
            nc > static_cast<double>(std::numeric_limits<uint32_t>::max())) {
            throw std::runtime_error("createAttribute: numChannels must be a positive integer");
        }
        num_channels = static_cast<size_t>(nc);
    }

    AttributeUsage usage = AttributeUsage::Vector;
    auto usage_val = opts["usage"];
    if (!usage_val.isUndefined()) usage = parse_usage(usage_val.as<std::string>());
    validate_usage_channels(usage, num_channels, mesh.get_dimension());

    val data = opts["data"];
    std::string dtype;
    auto dtype_val = opts["dtype"];
    if (!dtype_val.isUndefined()) {
        dtype = dtype_val.as<std::string>();
    } else if (!data.isUndefined() && !data.isNull()) {
        dtype = infer_dtype(data);
    } else {
        dtype = "float64";
    }

    if ((usage == AttributeUsage::VertexIndex || usage == AttributeUsage::FacetIndex ||
         usage == AttributeUsage::CornerIndex || usage == AttributeUsage::EdgeIndex) &&
        dtype != dtype_name<Index>()) {
        throw std::runtime_error(
            "createAttribute: usage '" + usage_name(usage) + "' requires dtype '" +
            dtype_name<Index>() + "'");
    }

    reject_bigint_array(data);
    bool has_data = !data.isUndefined() && !data.isNull();
    size_t data_len = has_data ? data["length"].as<size_t>() : 0;

    val indices_val = opts["indices"];
    reject_bigint_array(indices_val);
    bool has_indices = !indices_val.isUndefined() && !indices_val.isNull();
    size_t indices_len = has_indices ? indices_val["length"].as<size_t>() : 0;

    std::vector<Index> indices_buf;
    if (has_indices) {
        std::vector<double> raw_indices(indices_len);
        bulk_copy_into(indices_val, raw_indices.data(), indices_len);
        indices_buf = to_index_vector(raw_indices, "createAttribute: indices");
    }
    if (element == AttributeElement::Indexed && has_indices) {
        if (!has_data || data_len == 0) {
            throw std::runtime_error(
                "createAttribute: 'indices' requires non-empty 'data' for an indexed attribute");
        }
        if (data_len % num_channels != 0) {
            throw std::runtime_error(
                "createAttribute: data length is not a multiple of numChannels");
        }
        size_t value_count = data_len / num_channels;
        for (size_t i = 0; i < indices_buf.size(); ++i) {
            if (static_cast<size_t>(indices_buf[i]) >= value_count) {
                throw std::runtime_error(
                    "createAttribute: index at position " + std::to_string(i) +
                    " is out of range for " + std::to_string(value_count) + " indexed value(s)");
            }
        }
    }
    lagrange::span<const Index> indices_span(indices_buf.data(), indices_buf.size());

    // int64/uint64 have no safe native JS typed-array counterpart to bulk-copy into directly
    // (see to_typed_array()), so they go through a validated double intermediate instead.
    auto do_create = [&]() -> AttributeId {
#define LA_X_create_attribute(_, ValueType)                                                  \
    if (dtype == dtype_name<ValueType>()) {                                                  \
        std::vector<ValueType> values_buf(data_len);                                         \
        if (has_data) {                                                                      \
            if constexpr (                                                                   \
                std::is_same_v<ValueType, int64_t> || std::is_same_v<ValueType, uint64_t>) { \
                std::vector<double> raw_values(data_len);                                    \
                bulk_copy_into(data, raw_values.data(), data_len);                           \
                for (size_t i = 0; i < data_len; ++i)                                        \
                    values_buf[i] =                                                          \
                        to_wide_int<ValueType>(raw_values[i], "createAttribute: data", i);   \
            } else {                                                                         \
                bulk_copy_into(data, values_buf.data(), data_len);                           \
            }                                                                                \
        }                                                                                    \
        lagrange::span<const ValueType> values_span(values_buf.data(), values_buf.size());   \
        return mesh.template create_attribute<ValueType>(                                    \
            name,                                                                            \
            element,                                                                         \
            num_channels,                                                                    \
            usage,                                                                           \
            values_span,                                                                     \
            indices_span);                                                                   \
    }
        LA_ATTRIBUTE_X(create_attribute, 0)
#undef LA_X_create_attribute
        throw std::runtime_error("createAttribute: unsupported dtype '" + dtype + "'");
    };

    try {
        return do_create();
    } catch (...) {
        // AttributeManager::create/create_indexed reserves the name/id before constructing the
        // Attribute object, so a failure partway through (e.g. a usage/channel mismatch) leaves
        // `name` registered but unusable. Clean that up so the name can be reused.
        if (mesh.has_attribute(name)) mesh.delete_attribute(name);
        throw;
    }
}

val get_attribute_js(const MeshType& mesh, const std::string& name)
{
    AttributeId id = mesh.get_attribute_id(name);
    if (mesh.is_attribute_indexed(id)) {
        throw std::runtime_error(
            "getAttribute: '" + name + "' is an indexed attribute; use getIndexedAttribute");
    }
    return visit_attribute(mesh, id, [&](const auto& attr) -> val {
        using ValueType = typename std::decay_t<decltype(attr)>::ValueType;
        val out = val::object();
        out.set("id", id);
        out.set("name", name);
        out.set("element", element_name(attr.get_element_type()));
        out.set("usage", usage_name(attr.get_usage()));
        out.set("numChannels", static_cast<unsigned>(attr.get_num_channels()));
        out.set("numElements", static_cast<unsigned>(attr.get_num_elements()));
        out.set("dtype", dtype_name<ValueType>());
        out.set("data", to_typed_array(attr.get_all()));
        return out;
    });
}

void set_attribute_js(MeshType& mesh, const std::string& name, val data)
{
    AttributeId id = mesh.get_attribute_id(name);
    if (mesh.is_attribute_indexed(id)) {
        throw std::runtime_error(
            "setAttribute: '" + name +
            "' is an indexed attribute and cannot be overwritten "
            "in place; delete and recreate it instead");
    }
    reject_bigint_array(data);
    size_t data_len = data["length"].as<size_t>();
    auto vt = mesh.get_attribute_base(id).get_value_type();
    // int64/uint64 have no safe native JS typed-array counterpart to bulk-copy into directly
    // (see to_typed_array()), so they go through a double intermediate instead.
#define LA_X_set_attribute(_, ValueType)                                                           \
    if (vt == lagrange::make_attribute_value_type<ValueType>()) {                                  \
        auto& attr = mesh.template ref_attribute<ValueType>(id);                                   \
        auto out = attr.ref_all();                                                                 \
        if (data_len != out.size()) {                                                              \
            throw std::runtime_error(                                                              \
                "setAttribute: data length " + std::to_string(data_len) +                          \
                " does not match attribute size " + std::to_string(out.size()));                   \
        }                                                                                          \
        if constexpr (std::is_same_v<ValueType, int64_t> || std::is_same_v<ValueType, uint64_t>) { \
            std::vector<double> raw(data_len);                                                     \
            bulk_copy_into(data, raw.data(), data_len);                                            \
            std::vector<ValueType> validated(data_len);                                            \
            for (size_t i = 0; i < data_len; ++i)                                                  \
                validated[i] = to_wide_int<ValueType>(raw[i], "setAttribute: data", i);            \
            for (size_t i = 0; i < out.size(); ++i) out[i] = validated[i];                         \
        } else {                                                                                   \
            bulk_copy_into(data, out.data(), out.size());                                          \
        }                                                                                          \
        return;                                                                                    \
    }
    LA_ATTRIBUTE_X(set_attribute, 0)
#undef LA_X_set_attribute
    throw std::runtime_error("setAttribute: unsupported attribute value type");
}

void delete_attribute_js(MeshType& mesh, const std::string& name)
{
    mesh.delete_attribute(name);
}

void rename_attribute_js(MeshType& mesh, const std::string& old_name, const std::string& new_name)
{
    mesh.rename_attribute(old_name, new_name);
}

AttributeId
duplicate_attribute_js(MeshType& mesh, const std::string& old_name, const std::string& new_name)
{
    return mesh.duplicate_attribute(old_name, new_name);
}

AttributeId get_attribute_id_js(const MeshType& mesh, const std::string& name)
{
    return mesh.get_attribute_id(name);
}

std::string get_attribute_name_js(const MeshType& mesh, AttributeId id)
{
    return std::string(mesh.get_attribute_name(id));
}

bool is_attribute_indexed_js(const MeshType& mesh, const std::string& name)
{
    return mesh.is_attribute_indexed(mesh.get_attribute_id(name));
}

val get_indexed_attribute_js(const MeshType& mesh, const std::string& name)
{
    AttributeId id = mesh.get_attribute_id(name);
    if (!mesh.is_attribute_indexed(id)) {
        throw std::runtime_error(
            "getIndexedAttribute: '" + name + "' is not an indexed attribute; use getAttribute");
    }
    return visit_indexed_attribute(mesh, id, [&](const auto& iattr) -> val {
        using ValueType = typename std::decay_t<decltype(iattr)>::ValueType;
        val values_obj = val::object();
        values_obj.set("numElements", static_cast<unsigned>(iattr.values().get_num_elements()));
        values_obj.set("data", to_typed_array(iattr.values().get_all()));

        val out = val::object();
        out.set("id", id);
        out.set("name", name);
        out.set("usage", usage_name(iattr.get_usage()));
        out.set("numChannels", static_cast<unsigned>(iattr.get_num_channels()));
        out.set("dtype", dtype_name<ValueType>());
        out.set("values", values_obj);
        out.set("indices", to_typed_array(iattr.indices().get_all()));
        return out;
    });
}

} // namespace
} // namespace lagrange::js::bind

EMSCRIPTEN_BINDINGS(lagrange_core)
{
    using namespace emscripten;
    using namespace lagrange::js::bind;

    class_<MeshType>("SurfaceMesh")
        .constructor<Index>()
        .function("getNumVertices", &MeshType::get_num_vertices)
        .function("getNumFacets", &MeshType::get_num_facets)
        .function("getNumCorners", &MeshType::get_num_corners)
        .function("getDimension", &MeshType::get_dimension)
        .function("isTriangleMesh", &MeshType::is_triangle_mesh)
        .function(
            "addVertex",
            +[](MeshType& mesh, const emscripten::val& vertex_coords) {
                mesh_add_vertex(mesh, vertex_coords);
            })
        .function("addTriangle", &MeshType::add_triangle)
        .function(
            "addTriangles",
            +[](MeshType& mesh, const emscripten::val& triangle_indices) {
                mesh_add_triangles(mesh, triangle_indices);
            })
        .function("getNumEdges", &MeshType::get_num_edges)
        .function("isQuadMesh", &MeshType::is_quad_mesh)
        .function("isRegular", &MeshType::is_regular)
        .function("isHybrid", &MeshType::is_hybrid)
        .function("getVertexPerFacet", &MeshType::get_vertex_per_facet)
        .function("getFacetSize", &MeshType::get_facet_size)
        .function("getFacetVertex", &MeshType::get_facet_vertex)
        .function("getFacetCornerBegin", &MeshType::get_facet_corner_begin)
        .function("getFacetCornerEnd", &MeshType::get_facet_corner_end)
        .function("getCornerVertex", &MeshType::get_corner_vertex)
        .function("getCornerFacet", &MeshType::get_corner_facet)
        .function("getPosition", &mesh_get_position)
        .function("getFacetVertices", &mesh_get_facet_vertices)
        .function("shrinkToFit", &MeshType::shrink_to_fit)
        .function("clearFacets", &MeshType::clear_facets)
        .function("clearVertices", &MeshType::clear_vertices)
        .function("hasAttribute", &mesh_has_attribute)
        .function(
            "addVertices",
            +[](MeshType& mesh, const emscripten::val& coords) {
                const unsigned len = coords["length"].as<unsigned>();
                const Index dim = mesh.get_dimension();
                if (len % dim != 0) {
                    throw std::runtime_error(
                        "addVertices: array length must be divisible by dimension");
                }
                const Index num_verts = static_cast<Index>(len / dim);
                std::vector<Scalar> buf(len);
                for (unsigned i = 0; i < len; ++i) {
                    buf[i] = coords[i].as<Scalar>();
                }
                mesh.add_vertices(num_verts, buf);
            })
        .function(
            "removeVertices",
            +[](MeshType& mesh, const emscripten::val& indices) {
                const unsigned len = indices["length"].as<unsigned>();
                std::vector<Index> buf(len);
                for (unsigned i = 0; i < len; ++i) {
                    buf[i] = indices[i].as<Index>();
                }
                mesh.remove_vertices(buf);
            })
        .function(
            "removeFacets",
            +[](MeshType& mesh, const emscripten::val& indices) {
                const unsigned len = indices["length"].as<unsigned>();
                std::vector<Index> buf(len);
                for (unsigned i = 0; i < len; ++i) {
                    buf[i] = indices[i].as<Index>();
                }
                mesh.remove_facets(buf);
            })
        .function(
            "clone",
            +[](const MeshType& self) -> MeshType { return MeshType(self); })
        .function(
            "clone",
            +[](const MeshType& self, emscripten::val opts) -> MeshType {
                bool strip = false;
                apply_opt(opts, "strip", strip);
                return strip ? MeshType::stripped_copy(self) : MeshType(self);
            })
        .function(
            "flipFacets",
            +[](MeshType& mesh) { mesh.flip_facets([](Index) { return true; }); })
        .function(
            "flipFacets",
            +[](MeshType& mesh, const emscripten::val& indices) {
                if (indices.isUndefined() || indices.isNull()) {
                    mesh.flip_facets([](Index) { return true; });
                } else {
                    const unsigned len = indices["length"].as<unsigned>();
                    std::vector<Index> buf(len);
                    for (unsigned i = 0; i < len; ++i) {
                        buf[i] = indices[i].as<Index>();
                    }
                    mesh.flip_facets(lagrange::span<const Index>(buf.data(), buf.size()));
                }
            })
        .function("createAttribute", &create_attribute_js)
        .function("getAttribute", &get_attribute_js)
        .function("setAttribute", &set_attribute_js)
        .function("deleteAttribute", &delete_attribute_js)
        .function("renameAttribute", &rename_attribute_js)
        .function("duplicateAttribute", &duplicate_attribute_js)
        .function("getAttributeId", &get_attribute_id_js)
        .function("getAttributeName", &get_attribute_name_js)
        .function("isAttributeIndexed", &is_attribute_indexed_js)
        .function("getIndexedAttribute", &get_indexed_attribute_js);

    function("unifyIndexBuffer", &unify_index_buffer_js);
}
