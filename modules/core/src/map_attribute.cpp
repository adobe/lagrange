/*
 * Copyright 2022 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/map_attribute.h>

#include <lagrange/Attribute.h>
#include <lagrange/AttributeTypes.h>
#include <lagrange/IndexedAttribute.h>
#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/utils/Error.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/invalid.h>
#include <lagrange/utils/range.h>

#include <numeric>

namespace lagrange {

namespace {

template <typename ValueType, typename Func>
void dispatch_values(
    span<const ValueType> src,
    span<ValueType> dst,
    size_t num_elements,
    size_t num_channels,
    Func src_element)
{
    la_debug_assert(dst.size() == num_elements * num_channels);
    for (size_t e = 0; e < num_elements; ++e) {
        const size_t i = src_element(e);
        for (size_t k = 0; k < num_channels; ++k) {
            dst[e * num_channels + k] = src[i * num_channels + k];
        }
    }
}

template <typename ValueType, typename FuncSrc, typename FuncDst>
void average_values(
    span<const ValueType> src,
    span<ValueType> dst,
    size_t num_elements,
    size_t num_channels,
    FuncSrc src_element,
    FuncDst dst_element)
{
    la_debug_assert(dst.size() % num_channels == 0);
    std::fill(dst.begin(), dst.end(), ValueType(0));
    std::vector<ValueType> dst_valence(dst.size() / num_channels, 0);
    for (size_t e = 0; e < num_elements; ++e) {
        const size_t i = dst_element(e);
        for (size_t k = 0; k < num_channels; ++k) {
            dst[i * num_channels + k] += src[src_element(e) * num_channels + k];
        }
        ++dst_valence[i];
    }
    for (size_t i = 0, j = 0; i < dst.size(); i += num_channels, ++j) {
        for (size_t k = 0; k < num_channels; ++k) {
            dst[i + k] /= dst_valence[j];
        }
    }
}

template <typename ValueType, typename Scalar, typename Index>
AttributeId map_attribute_internal(
    SurfaceMesh<Scalar, Index>& mesh,
    AttributeId old_id,
    std::string_view new_name,
    AttributeElement new_element)
{
    const auto& old_attr_base = mesh.get_attribute_base(old_id);
    const AttributeElement old_element = old_attr_base.get_element_type();
    const size_t num_channels = old_attr_base.get_num_channels();

    // Sanity check
    if (old_element == new_element) {
        logger().warn("Mapping an attribute to an attribute of the same type");
        return mesh.duplicate_attribute(mesh.get_attribute_name(old_id), new_name);
    }

    // Allocate new attribute
    const auto new_id = mesh.template create_attribute<ValueType>(
        new_name,
        new_element,
        old_attr_base.get_usage(),
        old_attr_base.get_num_channels());

    size_t num_elements = invalid<size_t>();

    // Compute mapping for source attribute
    span<const ValueType> src_values;
    std::function<size_t(size_t)> src_element;

    if (mesh.is_attribute_indexed(old_id)) {
        const auto& old_attr = mesh.template get_indexed_attribute<ValueType>(old_id);
        span<const Index> src_index = old_attr.indices().get_all();
        src_values = old_attr.values().get_all();
        src_element = [src_index](size_t c) { return src_index[c]; };
        num_elements = mesh.get_num_corners();
    } else {
        const auto& old_attr = mesh.template get_attribute<ValueType>(old_id);
        src_values = old_attr.get_all();
        if (new_element == AttributeElement::Indexed || new_element == AttributeElement::Value) {
            switch (old_element) {
            case AttributeElement::Vertex: num_elements = mesh.get_num_vertices(); break;
            case AttributeElement::Facet: num_elements = mesh.get_num_facets(); break;
            case AttributeElement::Edge: num_elements = mesh.get_num_edges(); break;
            case AttributeElement::Corner:
            case AttributeElement::Value: num_elements = mesh.get_num_corners(); break;
            case AttributeElement::Indexed: la_debug_assert(false);
            }
            src_element = [](size_t i) { return i; };
        } else {
            num_elements = mesh.get_num_corners();
            switch (old_element) {
            case AttributeElement::Vertex:
                src_element = [&](size_t c) { return mesh.get_corner_vertex(c); };
                break;
            case AttributeElement::Facet:
                src_element = [&](size_t c) { return mesh.get_corner_facet(c); };
                break;
            case AttributeElement::Edge:
                src_element = [&](size_t c) { return mesh.get_edge_from_corner(c); };
                break;
            case AttributeElement::Corner: src_element = [](size_t c) { return c; }; break;
            case AttributeElement::Value:
                // Set expected number of elements based on target element type
                switch (new_element) {
                case AttributeElement::Vertex: num_elements = mesh.get_num_vertices(); break;
                case AttributeElement::Facet: num_elements = mesh.get_num_facets(); break;
                case AttributeElement::Edge: num_elements = mesh.get_num_edges(); break;
                case AttributeElement::Corner:
                case AttributeElement::Indexed: num_elements = mesh.get_num_corners(); break;
                case AttributeElement::Value: la_debug_assert(false);
                }
                la_runtime_assert(old_attr.get_num_elements() == num_elements);
                src_element = [](size_t i) { return i; };
                break;
            case AttributeElement::Indexed: la_debug_assert(false);
            }
        }
    }

    // Compute mapping for target attribute
    span<ValueType> dst_values;
    std::function<size_t(size_t)> dst_element;

    if (mesh.is_attribute_indexed(new_id)) {
        auto& new_attr = mesh.template ref_indexed_attribute<ValueType>(new_id);
        new_attr.values().resize_elements(num_elements);
        auto src_index = new_attr.indices().ref_all();
        switch (old_element) {
        case AttributeElement::Vertex:
            for (Index c = 0; c < mesh.get_num_corners(); ++c) {
                src_index[c] = mesh.get_corner_vertex(c);
            }
            break;
        case AttributeElement::Facet:
            for (Index c = 0; c < mesh.get_num_corners(); ++c) {
                src_index[c] = mesh.get_corner_facet(c);
            }
            break;
        case AttributeElement::Edge:
            for (Index c = 0; c < mesh.get_num_corners(); ++c) {
                src_index[c] = mesh.get_edge_from_corner(c);
            }
            break;
        case AttributeElement::Corner:
        case AttributeElement::Value:
            std::iota(src_index.begin(), src_index.end(), Index(0));
            break;
        case AttributeElement::Indexed: la_debug_assert(false);
        }
        dst_values = new_attr.values().ref_all();
    } else {
        auto& new_attr = mesh.template ref_attribute<ValueType>(new_id);
        if (old_element == AttributeElement::Value) {
            // Leave dst_element empty, and simply dispatch the values
        } else {
            switch (new_element) {
            case AttributeElement::Vertex:
                dst_element = [&](size_t c) { return mesh.get_corner_vertex(c); };
                break;
            case AttributeElement::Facet:
                dst_element = [&](size_t c) { return mesh.get_corner_facet(c); };
                break;
            case AttributeElement::Edge:
                dst_element = [&](size_t c) { return mesh.get_edge_from_corner(c); };
                break;
            case AttributeElement::Corner: break;
            case AttributeElement::Value: new_attr.resize_elements(num_elements); break;
            case AttributeElement::Indexed: la_debug_assert(false);
            }
        }
        dst_values = new_attr.ref_all();
    }

    // Map the actual values :)
    if (dst_element) {
        average_values(
            src_values,
            dst_values,
            num_elements,
            num_channels,
            src_element,
            dst_element);
    } else {
        dispatch_values(src_values, dst_values, num_elements, num_channels, src_element);
    }

    return new_id;
}

} // namespace

template <typename Scalar, typename Index>
AttributeId map_attribute(
    SurfaceMesh<Scalar, Index>& mesh,
    AttributeId id,
    std::string_view new_name,
    AttributeElement new_element)
{
    // Dispatch to actual underlying type
#define LA_X_map_attribute_internal(_, T)                                  \
    if (mesh.template is_attribute_type<T>(id)) {                          \
        return map_attribute_internal<T>(mesh, id, new_name, new_element); \
    }
    LA_ATTRIBUTE_X(map_attribute_internal, 0)
#undef LA_X_map_attribute_internal
    throw Error("Invalid attribute type");
}

template <typename Scalar, typename Index>
AttributeId map_attribute(
    SurfaceMesh<Scalar, Index>& mesh,
    std::string_view old_name,
    std::string_view new_name,
    AttributeElement new_element)
{
    return map_attribute(mesh, mesh.get_attribute_id(old_name), new_name, new_element);
}

template <typename Scalar, typename Index>
AttributeId map_attribute_in_place(
    SurfaceMesh<Scalar, Index>& mesh,
    AttributeId id,
    AttributeElement new_element)
{
    auto get_unique_name = [&](auto name) -> std::string {
        if (!mesh.has_attribute(name)) {
            return name;
        } else {
            std::string new_name;
            for (int cnt = 0; cnt < 1000; ++cnt) {
                new_name = fmt::format("{}.{}", name, cnt);
                if (!mesh.has_attribute(new_name)) {
                    return new_name;
                }
            }
            throw Error(fmt::format("Could not assign a unique attribute name for: {}", name));
        }
    };

    auto name = mesh.get_attribute_name(id);
    std::string tmp_name = get_unique_name(std::string(name) + "_");

    auto new_id = map_attribute(mesh, id, tmp_name, new_element);
    mesh.delete_attribute(name);
    mesh.rename_attribute(tmp_name, name);

    return new_id;
}

template <typename Scalar, typename Index>
AttributeId map_attribute_in_place(
    SurfaceMesh<Scalar, Index>& mesh,
    std::string_view name,
    AttributeElement new_element)
{
    return map_attribute_in_place(mesh, mesh.get_attribute_id(name), new_element);
}

#define LA_X_map_attribute(_, Scalar, Index)     \
    template AttributeId map_attribute(          \
        SurfaceMesh<Scalar, Index>& mesh,        \
        AttributeId id,                          \
        std::string_view new_name,               \
        AttributeElement new_element);           \
    template AttributeId map_attribute(          \
        SurfaceMesh<Scalar, Index>& mesh,        \
        std::string_view old_name,               \
        std::string_view new_name,               \
        AttributeElement new_element);           \
    template AttributeId map_attribute_in_place( \
        SurfaceMesh<Scalar, Index>& mesh,        \
        AttributeId id,                          \
        AttributeElement new_element);           \
    template AttributeId map_attribute_in_place( \
        SurfaceMesh<Scalar, Index>& mesh,        \
        std::string_view name,                   \
        AttributeElement new_element);
LA_SURFACE_MESH_X(map_attribute, 0)

} // namespace lagrange
