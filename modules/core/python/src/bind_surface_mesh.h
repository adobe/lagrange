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
#pragma once

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/string_view.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include "PyAttribute.h"

#include <lagrange/Attribute.h>
#include <lagrange/AttributeFwd.h>
#include <lagrange/AttributeTypes.h>
#include <lagrange/IndexedAttribute.h>
#include <lagrange/SurfaceMesh.h>
#include <lagrange/python/tensor_utils.h>
#include <lagrange/utils/Error.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/invalid.h>

namespace lagrange::python {

template <typename Scalar, typename Index>
void bind_surface_mesh(nanobind::module_& m)
{
    namespace nb = nanobind;
    using namespace nb::literals;
    using namespace lagrange;
    using namespace lagrange::python;

    using MeshType = SurfaceMesh<Scalar, Index>;

    auto surface_mesh_class = nb::class_<MeshType>(m, "SurfaceMesh");
    surface_mesh_class.def(nb::init<Index>(), nb::arg("dimension") = 3);
    surface_mesh_class.def("add_vertex", [](MeshType& self, Tensor<Scalar> b) {
        auto [data, shape, stride] = tensor_to_span(b);
        la_runtime_assert(is_dense(shape, stride));
        la_runtime_assert(check_shape(shape, self.get_dimension()));
        self.add_vertex(data);
    });
    surface_mesh_class.def("add_vertices", [](MeshType& self, Tensor<Scalar> b) {
        auto [data, shape, stride] = tensor_to_span(b);
        la_runtime_assert(is_dense(shape, stride));
        la_runtime_assert(check_shape(shape, invalid<size_t>(), self.get_dimension()));
        self.add_vertices(shape[0], data);
    });
    // TODO: combine all add facet functions into `add_facets`.
    surface_mesh_class.def("add_triangle", &MeshType::add_triangle);
    surface_mesh_class.def("add_triangles", [](MeshType& self, Tensor<Index> b) {
        auto [data, shape, stride] = tensor_to_span(b);
        la_runtime_assert(is_dense(shape, stride));
        la_runtime_assert(check_shape(shape, invalid<size_t>(), 3));
        self.add_triangles(shape[0], data);
    });
    surface_mesh_class.def("add_quad", &MeshType::add_quad);
    surface_mesh_class.def("add_quads", [](MeshType& self, Tensor<Index> b) {
        auto [data, shape, stride] = tensor_to_span(b);
        la_runtime_assert(is_dense(shape, stride));
        la_runtime_assert(check_shape(shape, invalid<size_t>(), 4));
        self.add_quads(shape[0], data);
    });
    surface_mesh_class.def("add_polygon", [](MeshType& self, Tensor<Index> b) {
        auto [data, shape, stride] = tensor_to_span(b);
        la_runtime_assert(is_dense(shape, stride));
        la_runtime_assert(is_vector(shape));
        self.add_polygon(data);
    });
    surface_mesh_class.def("add_polygons", [](MeshType& self, Tensor<Index> b) {
        auto [data, shape, stride] = tensor_to_span(b);
        la_runtime_assert(is_dense(shape, stride));
        self.add_polygons(shape[0], shape[1], data);
    });
    surface_mesh_class.def(
        "add_hybrid",
        [](MeshType& self, Tensor<Index> sizes, Tensor<Index> indices) {
            auto [size_data, size_shape, size_stride] = tensor_to_span(sizes);
            la_runtime_assert(is_dense(size_shape, size_stride));
            la_runtime_assert(is_vector(size_shape));

            auto [index_data, index_shape, index_stride] = tensor_to_span(indices);
            la_runtime_assert(is_dense(index_shape, index_stride));
            la_runtime_assert(is_vector(index_shape));

            self.add_hybrid(size_data, index_data);
        });
    surface_mesh_class.def("remove_vertices", [](MeshType& self, Tensor<Index> b) {
        auto [data, shape, stride] = tensor_to_span(b);
        la_runtime_assert(is_dense(shape, stride));
        la_runtime_assert(is_vector(shape));
        self.remove_vertices(data);
    });
    surface_mesh_class.def("remove_facets", [](MeshType& self, Tensor<Index> b) {
        auto [data, shape, stride] = tensor_to_span(b);
        la_runtime_assert(is_dense(shape, stride));
        la_runtime_assert(is_vector(shape));
        self.remove_facets(data);
    });
    surface_mesh_class.def("clear_vertices", &MeshType::clear_vertices);
    surface_mesh_class.def("clear_facets", &MeshType::clear_facets);
    surface_mesh_class.def("shrink_to_fit", &MeshType::shrink_to_fit);
    surface_mesh_class.def("compress_if_regular", &MeshType::compress_if_regular);
    surface_mesh_class.def_property_readonly("is_triangle_mesh", &MeshType::is_triangle_mesh);
    surface_mesh_class.def_property_readonly("is_quad_mesh", &MeshType::is_quad_mesh);
    surface_mesh_class.def_property_readonly("is_regular", &MeshType::is_regular);
    surface_mesh_class.def_property_readonly("is_hybrid", &MeshType::is_hybrid);
    surface_mesh_class.def_property_readonly("dimension", &MeshType::get_dimension);
    surface_mesh_class.def_property_readonly("vertex_per_facet", &MeshType::get_vertex_per_facet);
    surface_mesh_class.def_property_readonly("num_vertices", &MeshType::get_num_vertices);
    surface_mesh_class.def_property_readonly("num_facets", &MeshType::get_num_facets);
    surface_mesh_class.def_property_readonly("num_corners", &MeshType::get_num_corners);
    surface_mesh_class.def_property_readonly("num_edges", &MeshType::get_num_edges);
    surface_mesh_class.def("get_position", [](MeshType& self, Index i) {
        return span_to_tensor(self.get_position(i), nb::cast(&self));
    });
    surface_mesh_class.def("ref_position", [](MeshType& self, Index i) {
        return span_to_tensor(self.ref_position(i), nb::cast(&self));
    });
    surface_mesh_class.def("get_facet_size", &MeshType::get_facet_size);
    surface_mesh_class.def("get_facet_vertex", &MeshType::get_facet_vertex);
    surface_mesh_class.def("get_facet_corner_begin", &MeshType::get_facet_corner_begin);
    surface_mesh_class.def("get_facet_corner_end", &MeshType::get_facet_corner_end);
    surface_mesh_class.def("get_corner_vertex", &MeshType::get_corner_vertex);
    surface_mesh_class.def("get_corner_facet", &MeshType::get_corner_facet);
    surface_mesh_class.def("get_facet_vertices", [](MeshType& self, Index f) {
        return span_to_tensor(self.get_facet_vertices(f), nb::cast(&self));
    });
    surface_mesh_class.def("ref_facet_vertices", [](MeshType& self, Index f) {
        return span_to_tensor(self.ref_facet_vertices(f), nb::cast(&self));
    });
    surface_mesh_class.def("get_attribute_id", &MeshType::get_attribute_id);
    surface_mesh_class.def("get_attribute_name", &MeshType::get_attribute_name);
    surface_mesh_class.def(
        "create_attribute",
        [](MeshType& self,
           std::string_view name,
           AttributeElement element,
           AttributeUsage usage,
           GenericTensor initial_values,
           Tensor<Index> initial_indices) {
            auto create_attribute = [&](auto values) {
                using ValueType = typename std::decay_t<decltype(values)>::Scalar;
                auto [value_data, value_shape, value_stride] = tensor_to_span(values);
                auto [index_data, index_shape, index_stride] = tensor_to_span(initial_indices);
                la_runtime_assert(is_dense(value_shape, value_stride));
                la_runtime_assert(is_dense(index_shape, index_stride));
                Index num_channels = value_shape.size() == 1 ? 1 : value_shape[1];
                return self.template create_attribute<ValueType>(
                    name,
                    element,
                    usage,
                    num_channels,
                    value_data,
                    index_data,
                    AttributeCreatePolicy::ErrorIfReserved);
            };

#define LA_X_create_attribute(_, ValueType)                      \
    if (initial_values.dtype() == nb::dtype<ValueType>()) {      \
        Tensor<ValueType> local_values(initial_values.handle()); \
        return create_attribute(local_values);                   \
    }
            LA_ATTRIBUTE_X(create_attribute, 0)
#undef LA_X_create_attribute
            throw nb::type_error("Unsupported value type!");
        },
        "name"_a,
        "element"_a,
        "usage"_a /*= AttributeUsage::Scalar*/,
        "initial_values"_a = create_empty_tensor<Scalar>(),
        "initial_indices"_a = create_empty_tensor<Index>());
    surface_mesh_class.def(
        "wrap_as_attribute",
        [](MeshType& self,
           std::string_view name,
           AttributeElement element,
           AttributeUsage usage,
           GenericTensor values) {
            auto wrap_as_attribute = [&](auto tensor) {
                using ValueType = typename std::decay_t<decltype(tensor)>::Scalar;
                auto [data, shape, stride] = tensor_to_span(tensor);
                la_runtime_assert(is_dense(shape, stride));
                Index num_channels = shape.size() == 1 ? 1 : shape[1];
                AttributeId id;
                auto owner = std::make_shared<nb::object>(nb::cast(values));
                if constexpr (std::is_const_v<ValueType>) {
                    id = self.wrap_as_const_attribute(
                        name,
                        element,
                        usage,
                        num_channels,
                        make_shared_span(owner, data.data(), data.size()));
                } else {
                    id = self.wrap_as_attribute(
                        name,
                        element,
                        usage,
                        num_channels,
                        make_shared_span(owner, data.data(), data.size()));
                }
                return id;
            };

#define LA_X_wrap_as_attribute(_, ValueType)             \
    if (values.dtype() == nb::dtype<ValueType>()) {      \
        Tensor<ValueType> local_values(values.handle()); \
        return wrap_as_attribute(local_values);          \
    }
            LA_ATTRIBUTE_X(wrap_as_attribute, 0)
#undef LA_X_wrap_as_attribute
            throw nb::type_error("Unsupported value type!");
        },
        "name"_a,
        "element"_a,
        "usage"_a,
        "values"_a);
    surface_mesh_class.def(
        "wrap_as_indexed_attribute",
        [](MeshType& self,
           std::string_view name,
           AttributeUsage usage,
           GenericTensor values,
           Tensor<Index> indices) {
            auto wrap_as_indexed_attribute = [&](auto value_tensor, auto index_tensor) {
                using ValueType = typename std::decay_t<decltype(value_tensor)>::Scalar;
                auto [value_data, value_shape, value_stride] = tensor_to_span(value_tensor);
                auto [index_data, index_shape, index_stride] = tensor_to_span(index_tensor);
                la_runtime_assert(is_dense(value_shape, value_stride));
                la_runtime_assert(is_dense(index_shape, index_stride));
                Index num_values = value_shape[0];
                Index num_channels = value_shape.size() == 1 ? 1 : value_shape[1];
                AttributeId id;

                auto value_owner = std::make_shared<nb::object>(nb::cast(values));
                auto index_owner = std::make_shared<nb::object>(nb::cast(indices));

                if constexpr (std::is_const_v<ValueType>) {
                    id = self.wrap_as_const_indexed_attribute(
                        name,
                        usage,
                        num_values,
                        num_channels,
                        make_shared_span(value_owner, value_data.data(), value_data.size()),
                        make_shared_span(index_owner, index_data.data(), index_data.size()));
                } else {
                    id = self.wrap_as_indexed_attribute(
                        name,
                        usage,
                        num_values,
                        num_channels,
                        make_shared_span(value_owner, value_data.data(), value_data.size()),
                        make_shared_span(index_owner, index_data.data(), index_data.size()));
                }
                return id;
            };

#define LA_X_wrap_as_indexed_attribute(_, ValueType)             \
    if (values.dtype() == nb::dtype<ValueType>()) {              \
        Tensor<ValueType> local_values(values.handle());         \
        return wrap_as_indexed_attribute(local_values, indices); \
    }
            LA_ATTRIBUTE_X(wrap_as_indexed_attribute, 0)
#undef LA_X_wrap_as_indexed_attribute
            throw nb::type_error("Unsupported value type!");
        },
        "name"_a,
        "usage"_a,
        "values"_a,
        "indices"_a);
    surface_mesh_class.def("duplicate_attribute", &MeshType::duplicate_attribute);
    surface_mesh_class.def("rename_attribute", &MeshType::rename_attribute);
    surface_mesh_class.def(
        "delete_attribute",
        &MeshType::delete_attribute,
        "name"_a,
        "policy"_a /*= AttributeDeletePolicy::ErrorIfReserved*/);
    surface_mesh_class.def(
        "delete_attribute",
        [](MeshType& self, std::string_view name) { self.delete_attribute(name); },
        "name"_a);
    surface_mesh_class.def("has_attribute", &MeshType::has_attribute);
    surface_mesh_class.def(
        "is_attribute_indexed",
        static_cast<bool (MeshType::*)(AttributeId) const>(&MeshType::is_attribute_indexed));
    surface_mesh_class.def(
        "is_attribute_indexed",
        static_cast<bool (MeshType::*)(std::string_view) const>(&MeshType::is_attribute_indexed));
    surface_mesh_class.def("attribute", [](MeshType& self, AttributeId id) {
        return PyAttribute(self._ref_attribute_ptr(id));
    });
    surface_mesh_class.def("attribute", [](MeshType& self, std::string_view name) {
        return PyAttribute(self._ref_attribute_ptr(name));
    });
    surface_mesh_class.def("indexed_attribute", [](MeshType& self, AttributeId id) {
        return PyIndexedAttribute(self._ref_attribute_ptr(id));
    });
    surface_mesh_class.def("indexed_attribute", [](MeshType& self, std::string_view name) {
        return PyIndexedAttribute(self._ref_attribute_ptr(name));
    });
    surface_mesh_class.def("__attribute_ref_count__", [](MeshType& self, AttributeId id) {
        auto ptr = self._get_attribute_ptr(id);
        return ptr.use_count();
    });
    surface_mesh_class.def_property(
        "vertices",
        [](const MeshType& self) {
            const auto& attr = self.get_vertex_to_position();
            return attribute_to_tensor(attr, nb::cast(&self));
        },
        [](MeshType& self, Tensor<Scalar> tensor) {
            auto [values, shape, stride] = tensor_to_span(tensor);
            la_runtime_assert(is_dense(shape, stride));
            la_runtime_assert(check_shape(shape, invalid<size_t>(), self.get_dimension()));

            size_t num_vertices = shape.size() == 1 ? 1 : shape[0];
            auto owner = std::make_shared<nb::object>(nb::cast(tensor));
            self.wrap_as_vertices(
                make_shared_span(owner, values.data(), values.size()),
                num_vertices);
        });
    surface_mesh_class.def_property(
        "facets",
        [](const MeshType& self) {
            if (self.is_regular()) {
                const auto& attr = self.get_corner_to_vertex();
                const size_t shape[2] = {
                    static_cast<size_t>(self.get_num_facets()),
                    static_cast<size_t>(self.get_vertex_per_facet())};
                return attribute_to_tensor(attr, shape, nb::cast(&self));
            } else {
                logger().warn("Mesh is not regular, returning the flattened facets.");
                const auto& attr = self.get_corner_to_vertex();
                return attribute_to_tensor(attr, nb::cast(&self));
            }
        },
        [](MeshType& self, Tensor<Index> tensor) {
            auto [values, shape, stride] = tensor_to_span(tensor);
            la_runtime_assert(is_dense(shape, stride));

            const size_t num_facets = shape.size() == 1 ? 1 : shape[0];
            const size_t vertex_per_facet = shape.size() == 1 ? shape[0] : shape[1];
            auto owner = std::make_shared<nb::object>(nb::cast(tensor));
            self.wrap_as_facets(
                make_shared_span(owner, values.data(), values.size()),
                num_facets,
                vertex_per_facet);
        });
    surface_mesh_class.def(
        "wrap_as_vertices",
        [](MeshType& self, Tensor<Scalar> tensor, Index num_vertices) {
            auto [values, shape, stride] = tensor_to_span(tensor);
            la_runtime_assert(is_dense(shape, stride));
            la_runtime_assert(check_shape(shape, invalid<size_t>(), self.get_dimension()));

            auto owner = std::make_shared<nb::object>(nb::cast(tensor));
            return self.wrap_as_vertices(
                make_shared_span(owner, values.data(), values.size()),
                num_vertices);
        });
    surface_mesh_class.def(
        "wrap_as_facets",
        [](MeshType& self, Tensor<Index> tensor, Index num_facets, Index vertex_per_facet) {
            auto [values, shape, stride] = tensor_to_span(tensor);
            la_runtime_assert(is_dense(shape, stride));

            auto owner = std::make_shared<nb::object>(nb::cast(tensor));
            return self.wrap_as_facets(
                make_shared_span(owner, values.data(), values.size()),
                num_facets,
                vertex_per_facet);
        });
    surface_mesh_class.def(
        "wrap_as_facets",
        [](MeshType& self,
           Tensor<Index> offsets,
           Index num_facets,
           Tensor<Index> facets,
           Index num_corners) {
            auto [offsets_data, offsets_shape, offsets_stride] = tensor_to_span(offsets);
            auto [facets_data, facets_shape, facets_stride] = tensor_to_span(facets);
            la_runtime_assert(is_dense(offsets_shape, offsets_stride));
            la_runtime_assert(is_dense(facets_shape, facets_stride));

            auto offsets_owner = std::make_shared<nb::object>(nb::cast(offsets));
            auto facets_owner = std::make_shared<nb::object>(nb::cast(facets));

            return self.wrap_as_facets(
                make_shared_span(offsets_owner, offsets_data.data(), offsets_data.size()),
                num_facets,
                make_shared_span(facets_owner, facets_data.data(), facets_data.size()),
                num_corners);
        });
    surface_mesh_class.def_static("attr_name_is_reserved", &MeshType::attr_name_is_reserved);
    surface_mesh_class.def_property_readonly_static(
        "attr_name_vertex_to_position",
        &MeshType::attr_name_vertex_to_position);
    surface_mesh_class.def_property_readonly_static(
        "attr_name_corner_to_vertex",
        &MeshType::attr_name_corner_to_vertex);
    surface_mesh_class.def_property_readonly_static(
        "attr_name_facet_to_first_corner",
        &MeshType::attr_name_facet_to_first_corner);
    surface_mesh_class.def_property_readonly_static(
        "attr_name_corner_to_facet",
        &MeshType::attr_name_corner_to_facet);
    surface_mesh_class.def_property_readonly_static(
        "attr_name_corner_to_edge",
        &MeshType::attr_name_corner_to_edge);
    surface_mesh_class.def_property_readonly_static(
        "attr_name_edge_to_first_corner",
        &MeshType::attr_name_edge_to_first_corner);
    surface_mesh_class.def_property_readonly_static(
        "attr_name_next_corner_around_edge",
        &MeshType::attr_name_next_corner_around_edge);
    surface_mesh_class.def_property_readonly_static(
        "attr_name_vertex_to_first_corner",
        &MeshType::attr_name_vertex_to_first_corner);
    surface_mesh_class.def_property_readonly_static(
        "attr_name_next_corner_around_vertex",
        &MeshType::attr_name_next_corner_around_vertex);
    surface_mesh_class.def_property_readonly(
        "attr_id_vertex_to_positions",
        &MeshType::attr_id_vertex_to_positions);
    surface_mesh_class.def_property_readonly(
        "attr_id_corner_to_vertex",
        &MeshType::attr_id_corner_to_vertex);
    surface_mesh_class.def_property_readonly(
        "attr_id_facet_to_first_corner",
        &MeshType::attr_id_facet_to_first_corner);
    surface_mesh_class.def_property_readonly(
        "attr_id_corner_to_facet",
        &MeshType::attr_id_corner_to_facet);
    surface_mesh_class.def_property_readonly(
        "attr_id_corner_to_edge",
        &MeshType::attr_id_corner_to_edge);
    surface_mesh_class.def_property_readonly(
        "attr_id_edge_to_first_corner",
        &MeshType::attr_id_edge_to_first_corner);
    surface_mesh_class.def_property_readonly(
        "attr_id_next_corner_around_edge",
        &MeshType::attr_id_next_corner_around_edge);
    surface_mesh_class.def_property_readonly(
        "attr_id_vertex_to_first_corner",
        &MeshType::attr_id_vertex_to_first_corner);
    surface_mesh_class.def_property_readonly(
        "attr_id_next_corner_around_vertex",
        &MeshType::attr_id_next_corner_around_vertex);
    surface_mesh_class.def("initialize_edges", [](MeshType& self) { self.initialize_edges(); });
    surface_mesh_class.def(
        "initialize_edges",
        [](MeshType& self, Tensor<Index> tensor) {
            auto [edge_data, edge_shape, edge_stride] = tensor_to_span(tensor);
            la_runtime_assert(is_dense(edge_shape, edge_stride));
            la_runtime_assert(
                edge_data.empty() || check_shape(edge_shape, invalid<size_t>(), 2),
                "Edge tensor mush be of the shape num_edges x 2");
            self.initialize_edges(edge_data);
        },
        "edges"_a = create_empty_tensor<Index>());
    surface_mesh_class.def("clear_edges", &MeshType::clear_edges);
    surface_mesh_class.def_property_readonly("has_edges", &MeshType::has_edges);
    surface_mesh_class.def("get_edge", &MeshType::get_edge);
    surface_mesh_class.def("get_corner_edge", &MeshType::get_corner_edge);
    surface_mesh_class.def("get_edge_vertices", &MeshType::get_edge_vertices);
    surface_mesh_class.def("get_first_corner_around_edge", &MeshType::get_first_corner_around_edge);
    surface_mesh_class.def("get_next_corner_around_edge", &MeshType::get_next_corner_around_edge);
    surface_mesh_class.def(
        "get_first_corner_around_vertex",
        &MeshType::get_first_corner_around_vertex);
    surface_mesh_class.def(
        "get_next_corner_around_vertex",
        &MeshType::get_next_corner_around_vertex);
    surface_mesh_class.def(
        "count_num_corners_around_edge",
        &MeshType::count_num_corners_around_edge);
    surface_mesh_class.def(
        "count_num_corners_around_vertex",
        &MeshType::count_num_corners_around_vertex);
    surface_mesh_class.def("get_one_facet_around_edge", &MeshType::get_one_facet_around_edge);
    surface_mesh_class.def("get_one_corner_around_edge", &MeshType::get_one_corner_around_edge);
    surface_mesh_class.def("get_one_corner_around_vertex", &MeshType::get_one_corner_around_vertex);
    surface_mesh_class.def("is_boundary_edge", &MeshType::is_boundary_edge);
}

} // namespace lagrange::python
