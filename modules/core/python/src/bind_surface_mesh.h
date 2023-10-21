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
#include <nanobind/stl/array.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/string_view.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include "PyAttribute.h"

#include <lagrange/Attribute.h>
#include <lagrange/AttributeFwd.h>
#include <lagrange/AttributeTypes.h>
#include <lagrange/IndexedAttribute.h>
#include <lagrange/Logger.h>
#include <lagrange/SurfaceMesh.h>
#include <lagrange/foreach_attribute.h>
#include <lagrange/python/tensor_utils.h>
#include <lagrange/utils/BitField.h>
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
    surface_mesh_class.def(
        "add_vertex",
        [](MeshType& self, Tensor<Scalar> b) {
            auto [data, shape, stride] = tensor_to_span(b);
            la_runtime_assert(is_dense(shape, stride));
            la_runtime_assert(check_shape(shape, self.get_dimension()));
            self.add_vertex(data);
        },
        "vertex"_a,
        R"(Add a vertex to the mesh.

:param vertex: vertex coordinates
:type vertex: numpy.ndarray or list)");

    // Handy overload to take python list as argument.
    surface_mesh_class.def("add_vertex", [](MeshType& self, nb::list b) {
        la_runtime_assert(static_cast<Index>(b.size()) == self.get_dimension());
        if (self.get_dimension() == 3) {
            self.add_vertex(
                {nb::cast<Scalar>(b[0]), nb::cast<Scalar>(b[1]), nb::cast<Scalar>(b[2])});
        } else if (self.get_dimension() == 2) {
            self.add_vertex({nb::cast<Scalar>(b[0]), nb::cast<Scalar>(b[1])});
        } else {
            throw std::runtime_error("Dimension mismatch in vertex tensor");
        }
    });

    surface_mesh_class.def("add_vertices", [](MeshType& self, Tensor<Scalar> b) {
        auto [data, shape, stride] = tensor_to_span(b);
        la_runtime_assert(is_dense(shape, stride));
        la_runtime_assert(check_shape(shape, invalid<size_t>(), self.get_dimension()));
        self.add_vertices(static_cast<Index>(shape[0]), data);
    });

    // TODO: combine all add facet functions into `add_facets`.
    surface_mesh_class.def("add_triangle", &MeshType::add_triangle);
    surface_mesh_class.def("add_triangles", [](MeshType& self, Tensor<Index> b) {
        auto [data, shape, stride] = tensor_to_span(b);
        la_runtime_assert(is_dense(shape, stride));
        la_runtime_assert(check_shape(shape, invalid<size_t>(), 3));
        self.add_triangles(static_cast<Index>(shape[0]), data);
    });
    surface_mesh_class.def("add_quad", &MeshType::add_quad);
    surface_mesh_class.def("add_quads", [](MeshType& self, Tensor<Index> b) {
        auto [data, shape, stride] = tensor_to_span(b);
        la_runtime_assert(is_dense(shape, stride));
        la_runtime_assert(check_shape(shape, invalid<size_t>(), 4));
        self.add_quads(static_cast<Index>(shape[0]), data);
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
        self.add_polygons(static_cast<Index>(shape[0]), static_cast<Index>(shape[1]), data);
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
    surface_mesh_class.def(
        "remove_vertices",
        [](MeshType& self, nb::list b) {
            std::vector<Index> indices;
            for (auto i : b) {
                indices.push_back(nb::cast<Index>(i));
            }
            self.remove_vertices(indices);
        },
        "vertices"_a,
        R"(Remove selected vertices from the mesh.

:param vertices: list of vertex indices to remove)");
    surface_mesh_class.def("remove_facets", [](MeshType& self, Tensor<Index> b) {
        auto [data, shape, stride] = tensor_to_span(b);
        la_runtime_assert(is_dense(shape, stride));
        la_runtime_assert(is_vector(shape));
        self.remove_facets(data);
    });
    surface_mesh_class.def(
        "remove_facets",
        [](MeshType& self, nb::list b) {
            std::vector<Index> indices;
            for (auto i : b) {
                indices.push_back(nb::cast<Index>(i));
            }
            self.remove_facets(indices);
        },
        "facets"_a,
        R"(Remove selected facets from the mesh.

:param facets: list of facet indices to remove)");
    surface_mesh_class.def("clear_vertices", &MeshType::clear_vertices);
    surface_mesh_class.def("clear_facets", &MeshType::clear_facets);
    surface_mesh_class.def("shrink_to_fit", &MeshType::shrink_to_fit);
    surface_mesh_class.def("compress_if_regular", &MeshType::compress_if_regular);
    surface_mesh_class.def_prop_ro("is_triangle_mesh", &MeshType::is_triangle_mesh);
    surface_mesh_class.def_prop_ro("is_quad_mesh", &MeshType::is_quad_mesh);
    surface_mesh_class.def_prop_ro("is_regular", &MeshType::is_regular);
    surface_mesh_class.def_prop_ro("is_hybrid", &MeshType::is_hybrid);
    surface_mesh_class.def_prop_ro("dimension", &MeshType::get_dimension);
    surface_mesh_class.def_prop_ro("vertex_per_facet", &MeshType::get_vertex_per_facet);
    surface_mesh_class.def_prop_ro("num_vertices", &MeshType::get_num_vertices);
    surface_mesh_class.def_prop_ro("num_facets", &MeshType::get_num_facets);
    surface_mesh_class.def_prop_ro("num_corners", &MeshType::get_num_corners);
    surface_mesh_class.def_prop_ro("num_edges", &MeshType::get_num_edges);
    surface_mesh_class.def("get_position", [](MeshType& self, Index i) {
        return span_to_tensor(self.get_position(i), nb::find(&self));
    });
    surface_mesh_class.def("ref_position", [](MeshType& self, Index i) {
        return span_to_tensor(self.ref_position(i), nb::find(&self));
    });
    surface_mesh_class.def("get_facet_size", &MeshType::get_facet_size);
    surface_mesh_class.def("get_facet_vertex", &MeshType::get_facet_vertex);
    surface_mesh_class.def("get_facet_corner_begin", &MeshType::get_facet_corner_begin);
    surface_mesh_class.def("get_facet_corner_end", &MeshType::get_facet_corner_end);
    surface_mesh_class.def("get_corner_vertex", &MeshType::get_corner_vertex);
    surface_mesh_class.def("get_corner_facet", &MeshType::get_corner_facet);
    surface_mesh_class.def("get_facet_vertices", [](MeshType& self, Index f) {
        return span_to_tensor(self.get_facet_vertices(f), nb::find(&self));
    });
    surface_mesh_class.def("ref_facet_vertices", [](MeshType& self, Index f) {
        return span_to_tensor(self.ref_facet_vertices(f), nb::find(&self));
    });
    surface_mesh_class.def("get_attribute_id", &MeshType::get_attribute_id);
    surface_mesh_class.def("get_attribute_name", &MeshType::get_attribute_name);
    surface_mesh_class.def(
        "create_attribute",
        [](MeshType& self,
           std::string_view name,
           AttributeElement element,
           AttributeUsage usage,
           std::optional<GenericTensor> initial_values,
           std::optional<Tensor<Index>> initial_indices,
           std::optional<Index> num_channels,
           std::optional<nb::type_object> dtype) {
            auto create_attribute = [&](auto values) {
                using ValueType = typename std::decay_t<decltype(values)>::Scalar;

                span<const ValueType> init_values;
                span<const Index> init_indices;
                Index n = num_channels.value_or(1);

                if (initial_values.has_value()) {
                    auto [value_data, value_shape, value_stride] = tensor_to_span(values);
                    la_runtime_assert(is_dense(value_shape, value_stride));
                    init_values = value_data;
                    Index num_cols =
                        value_shape.size() == 1 ? 1 : static_cast<Index>(value_shape[1]);
                    if (num_channels.has_value()) {
                        la_runtime_assert(
                            num_cols == n,
                            "Number of channels does not match the number of columns in the "
                            "initial values!");
                    } else {
                        n = num_cols;
                    }
                } else {
                    la_runtime_assert(
                        num_channels.has_value(),
                        "Number of channels is required when initial values are not provided!");
                    la_runtime_assert(
                        dtype.has_value(),
                        "dtype is required when initial values are not provided!");
                }
                if (initial_indices.has_value()) {
                    const auto& indices = initial_indices.value();
                    auto [index_data, index_shape, index_stride] = tensor_to_span(indices);
                    la_runtime_assert(is_dense(index_shape, index_stride));
                    init_indices = index_data;
                }
                return self.template create_attribute<ValueType>(
                    name,
                    element,
                    usage,
                    n,
                    init_values,
                    init_indices,
                    AttributeCreatePolicy::ErrorIfReserved);
            };

            if (initial_values.has_value()) {
                const auto& values = initial_values.value();
#define LA_X_create_attribute(_, ValueType)              \
    if (values.dtype() == nb::dtype<ValueType>()) {      \
        Tensor<ValueType> local_values(values.handle()); \
        return create_attribute(local_values);           \
    }
                LA_ATTRIBUTE_X(create_attribute, 0)
#undef LA_X_create_attribute
            } else if (dtype.has_value()) {
                const auto& t = dtype.value();
                auto np = nb::module_::import_("numpy");
                if (t.is(&PyFloat_Type)) {
                    // Native python float is a C double.
                    Tensor<double> local_values;
                    return create_attribute(local_values);
                } else if (t.is(np.attr("float32"))) {
                    Tensor<float> local_values;
                    return create_attribute(local_values);
                } else if (t.is(np.attr("float64"))) {
                    Tensor<double> local_values;
                    return create_attribute(local_values);
                } else if (t.is(np.attr("int8"))) {
                    Tensor<int8_t> local_values;
                    return create_attribute(local_values);
                } else if (t.is(np.attr("int16"))) {
                    Tensor<int16_t> local_values;
                    return create_attribute(local_values);
                } else if (t.is(np.attr("int32"))) {
                    Tensor<int32_t> local_values;
                    return create_attribute(local_values);
                } else if (t.is(np.attr("int64"))) {
                    Tensor<int64_t> local_values;
                    return create_attribute(local_values);
                } else if (t.is(np.attr("uint8"))) {
                    Tensor<uint8_t> local_values;
                    return create_attribute(local_values);
                } else if (t.is(np.attr("uint16"))) {
                    Tensor<uint16_t> local_values;
                    return create_attribute(local_values);
                } else if (t.is(np.attr("uint32"))) {
                    Tensor<uint32_t> local_values;
                    return create_attribute(local_values);
                } else if (t.is(np.attr("uint64"))) {
                    Tensor<uint64_t> local_values;
                    return create_attribute(local_values);
                }
            }
            throw nb::type_error("`initial_values` and `dtype` cannot both be None!");
        },
        "name"_a,
        "element"_a,
        "usage"_a /*= AttributeUsage::Scalar*/,
        "initial_values"_a = nb::none(),
        "initial_indices"_a = nb::none(),
        "num_channels"_a = nb::none(),
        "dtype"_a = nb::none(),
        R"(Create an attribute.

:param name: Name of the attribute.
:type name: str
:param element: Element type of the attribute.
:type element: AttributeElement
:param usage: Usage type of the attribute.
:type usage: AttributeUsage
:param initial_values: Initial values of the attribute.
:type initial_values: numpy.ndarray, optional
:param initial_indices: Initial indices of the attribute (Indexed attribute only).
:type initial_indices: numpy.ndarray, optional
:param num_channels: Number of channels of the attribute.
:type num_channels: int, optional
:param dtype: Data type of the attribute.
:type dtype: valid numpy.dtype, optional

:returns: The id of the created attribute.
)");
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
                Index num_channels = shape.size() == 1 ? 1 : static_cast<Index>(shape[1]);
                AttributeId id;
                auto owner = std::make_shared<nb::object>(nb::find(values));
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
                auto& attr = self.template ref_attribute<ValueType>(id);
                attr.set_growth_policy(AttributeGrowthPolicy::WarnAndCopy);
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
        "values"_a,
        R"(Wrap an existing numpy array as an attribute.

:param name: Name of the attribute.
:type name: str
:param element: Element type of the attribute.
:type element: AttributeElement
:param usage: Usage type of the attribute.
:type usage: AttributeUsage
:param values: Values of the attribute.
:type values: numpy.ndarray

:returns: The id of the created attribute.)");
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
                Index num_values = static_cast<Index>(value_shape[0]);
                Index num_channels =
                    value_shape.size() == 1 ? 1 : static_cast<Index>(value_shape[1]);
                AttributeId id;

                auto value_owner = std::make_shared<nb::object>(nb::find(values));
                auto index_owner = std::make_shared<nb::object>(nb::find(indices));

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
                auto& attr = self.template ref_indexed_attribute<ValueType>(id);
                attr.values().set_growth_policy(AttributeGrowthPolicy::WarnAndCopy);
                attr.indices().set_growth_policy(AttributeGrowthPolicy::WarnAndCopy);
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
        "indices"_a,
        R"(Wrap an existing numpy array as an indexed attribute.

:param name: Name of the attribute.
:type name: str
:param usage: Usage type of the attribute.
:type usage: AttributeUsage
:param values: Values of the attribute.
:type values: numpy.ndarray
:param indices: Indices of the attribute.
:type indices: numpy.ndarray

:returns: The id of the created attribute.)");
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

    // This is a helper method for trigger copy-on-write mechanism for a given attribute.
    auto ensure_attribute_is_not_shared = [](MeshType& mesh, AttributeId id) {
        auto& attr_base = mesh.get_attribute_base(id);
#define LA_X_trigger_copy_on_write(_, ValueType)                                              \
    if (mesh.is_attribute_indexed(id)) {                                                      \
        if (dynamic_cast<const IndexedAttribute<ValueType, Index>*>(&attr_base)) {            \
            [[maybe_unused]] auto& attr = mesh.template ref_indexed_attribute<ValueType>(id); \
        }                                                                                     \
    } else {                                                                                  \
        if (dynamic_cast<const Attribute<ValueType>*>(&attr_base)) {                          \
            [[maybe_unused]] auto& attr = mesh.template ref_attribute<ValueType>(id);         \
        }                                                                                     \
    }
        LA_ATTRIBUTE_X(trigger_copy_on_write, 0)
#undef LA_X_trigger_copy_on_write
    };

    surface_mesh_class.def(
        "attribute",
        [&](MeshType& self, AttributeId id, bool sharing) {
            la_runtime_assert(
                !self.is_attribute_indexed(id),
                fmt::format(
                    "Attribute {} is indexed!  Please use `indexed_attribute` property instead.",
                    id));
            if (!sharing) ensure_attribute_is_not_shared(self, id);
            return PyAttribute(self._ref_attribute_ptr(id));
        },
        "id"_a,
        "sharing"_a = true,
        R"(Get an attribute by id.

:param id: Id of the attribute.
:type id: AttributeId
:param sharing: Whether to allow sharing the attribute with other meshes.
:type sharing: bool

:returns: The attribute.)");
    surface_mesh_class.def(
        "attribute",
        [&](MeshType& self, std::string_view name, bool sharing) {
            la_runtime_assert(
                !self.is_attribute_indexed(name),
                fmt::format(
                    "Attribute \"{}\" is indexed!  Please use `indexed_attribute` property "
                    "instead.",
                    name));
            if (!sharing) ensure_attribute_is_not_shared(self, self.get_attribute_id(name));
            return PyAttribute(self._ref_attribute_ptr(name));
        },
        "name"_a,
        "sharing"_a = true,
        R"(Get an attribute by name.

:param name: Name of the attribute.
:type name: str
:param sharing: Whether to allow sharing the attribute with other meshes.
:type sharing: bool

:return: The attribute.)");
    surface_mesh_class.def(
        "indexed_attribute",
        [&](MeshType& self, AttributeId id, bool sharing) {
            la_runtime_assert(
                self.is_attribute_indexed(id),
                fmt::format(
                    "Attribute {} is not indexed!  Please use `attribute` property instead.",
                    id));
            if (!sharing) ensure_attribute_is_not_shared(self, id);
            return PyIndexedAttribute(self._ref_attribute_ptr(id));
        },
        "id"_a,
        "sharing"_a = true,
        R"(Get an indexed attribute by id.

:param id: Id of the attribute.
:type id: AttributeId
:param sharing: Whether to allow sharing the attribute with other meshes.
:type sharing: bool

:returns: The indexed attribute.)");
    surface_mesh_class.def(
        "indexed_attribute",
        [&](MeshType& self, std::string_view name, bool sharing) {
            la_runtime_assert(
                self.is_attribute_indexed(name),
                fmt::format(
                    "Attribute \"{}\"is not indexed!  Please use `attribute` property instead.",
                    name));
            if (!sharing) ensure_attribute_is_not_shared(self, self.get_attribute_id(name));
            return PyIndexedAttribute(self._ref_attribute_ptr(name));
        },
        "name"_a,
        "sharing"_a = true,
        R"(Get an indexed attribute by name.

:param name: Name of the attribute.
:type name: str
:param sharing: Whether to allow sharing the attribute with other meshes.
:type sharing: bool

:returns: The indexed attribute.)");
    surface_mesh_class.def("__attribute_ref_count__", [](MeshType& self, AttributeId id) {
        auto ptr = self._get_attribute_ptr(id);
        return ptr.use_count();
    });
    surface_mesh_class.def_prop_rw(
        "vertices",
        [](const MeshType& self) {
            const auto& attr = self.get_vertex_to_position();
            return attribute_to_tensor(attr, nb::find(&self));
        },
        [](MeshType& self, Tensor<Scalar> tensor) {
            auto [values, shape, stride] = tensor_to_span(tensor);
            la_runtime_assert(is_dense(shape, stride));
            la_runtime_assert(check_shape(shape, invalid<size_t>(), self.get_dimension()));

            size_t num_vertices = shape.size() == 1 ? 1 : shape[0];
            auto owner = std::make_shared<nb::object>(nb::find(tensor));
            auto id = self.wrap_as_vertices(
                make_shared_span(owner, values.data(), values.size()),
                static_cast<Index>(num_vertices));
            auto& attr = self.template ref_attribute<Scalar>(id);
            attr.set_growth_policy(AttributeGrowthPolicy::WarnAndCopy);
        },
        "Vertices of the mesh.");
    surface_mesh_class.def_prop_rw(
        "facets",
        [](const MeshType& self) {
            if (self.is_regular()) {
                const auto& attr = self.get_corner_to_vertex();
                const size_t shape[2] = {
                    static_cast<size_t>(self.get_num_facets()),
                    static_cast<size_t>(self.get_vertex_per_facet())};
                return attribute_to_tensor(attr, shape, nb::find(&self));
            } else {
                logger().warn("Mesh is not regular, returning the flattened facets.");
                const auto& attr = self.get_corner_to_vertex();
                return attribute_to_tensor(attr, nb::find(&self));
            }
        },
        [](MeshType& self, Tensor<Index> tensor) {
            auto [values, shape, stride] = tensor_to_span(tensor);
            la_runtime_assert(is_dense(shape, stride));

            const size_t num_facets = shape.size() == 1 ? 1 : shape[0];
            const size_t vertex_per_facet = shape.size() == 1 ? shape[0] : shape[1];
            auto owner = std::make_shared<nb::object>(nb::find(tensor));
            auto id = self.wrap_as_facets(
                make_shared_span(owner, values.data(), values.size()),
                static_cast<Index>(num_facets),
                static_cast<Index>(vertex_per_facet));
            auto& attr = self.template ref_attribute<Scalar>(id);
            attr.set_growth_policy(AttributeGrowthPolicy::WarnAndCopy);
        },
        "Facets of the mesh.");
    surface_mesh_class.def(
        "wrap_as_vertices",
        [](MeshType& self, Tensor<Scalar> tensor, Index num_vertices) {
            auto [values, shape, stride] = tensor_to_span(tensor);
            la_runtime_assert(is_dense(shape, stride));
            la_runtime_assert(check_shape(shape, invalid<size_t>(), self.get_dimension()));

            auto owner = std::make_shared<nb::object>(nb::find(tensor));
            auto id = self.wrap_as_vertices(
                make_shared_span(owner, values.data(), values.size()),
                num_vertices);
            auto& attr = self.template ref_attribute<Scalar>(id);
            attr.set_growth_policy(AttributeGrowthPolicy::WarnAndCopy);
            return id;
        },
        "tensor"_a,
        "num_vertices"_a,
        R"(Wrap a tensor as vertices.

:param tensor: The tensor to wrap.
:type tensor: numpy.ndarray
:param num_vertices: Number of vertices.
:type num_vertices: int

:return: The id of the wrapped vertices attribute.)");
    surface_mesh_class.def(
        "wrap_as_facets",
        [](MeshType& self, Tensor<Index> tensor, Index num_facets, Index vertex_per_facet) {
            auto [values, shape, stride] = tensor_to_span(tensor);
            la_runtime_assert(is_dense(shape, stride));

            auto owner = std::make_shared<nb::object>(nb::find(tensor));
            auto id = self.wrap_as_facets(
                make_shared_span(owner, values.data(), values.size()),
                num_facets,
                vertex_per_facet);
            auto& attr = self.template ref_attribute<Scalar>(id);
            attr.set_growth_policy(AttributeGrowthPolicy::WarnAndCopy);
            return id;
        },
        "tensor"_a,
        "num_facets"_a,
        "vertex_per_facet"_a,
        R"(Wrap a tensor as a list of regular facets.

:param tensor: The tensor to wrap.
:type tensor: numpy.ndarray
:param num_facets: Number of facets.
:type num_facets: int
:param vertex_per_facet: Number of vertices per facet.
:type vertex_per_facet: int

:return: The id of the wrapped facet attribute.)");
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

            auto offsets_owner = std::make_shared<nb::object>(nb::find(offsets));
            auto facets_owner = std::make_shared<nb::object>(nb::find(facets));

            auto id = self.wrap_as_facets(
                make_shared_span(offsets_owner, offsets_data.data(), offsets_data.size()),
                num_facets,
                make_shared_span(facets_owner, facets_data.data(), facets_data.size()),
                num_corners);
            auto& attr = self.template ref_attribute<Scalar>(id);
            attr.set_growth_policy(AttributeGrowthPolicy::WarnAndCopy);
            return id;
        },
        "offsets"_a,
        "num_facets"_a,
        "facets"_a,
        "num_corners"_a,
        R"(Wrap a tensor as a list of hybrid facets.

:param offsets: The offset indices into the facets array.
:type offsets: numpy.ndarray
:param num_facets: Number of facets.
:type num_facets: int
:param facets: The indices of the vertices of the facets.
:type facets: numpy.ndarray
:param num_corners: Number of corners.
:type num_corners: int

:return: The id of the wrapped facet attribute.)");
    surface_mesh_class.def_static("attr_name_is_reserved", &MeshType::attr_name_is_reserved);
    surface_mesh_class.def_prop_ro_static(
        "attr_name_vertex_to_position",
        [](nb::handle) { return MeshType::attr_name_vertex_to_position(); },
        "The name of the attribute that stores the vertex positions.");
    surface_mesh_class.def_prop_ro_static(
        "attr_name_corner_to_vertex",
        [](nb::handle) { return MeshType::attr_name_corner_to_vertex(); },
        "The name of the attribute that stores the corner to vertex mapping.");
    surface_mesh_class.def_prop_ro_static(
        "attr_name_facet_to_first_corner",
        [](nb::handle) { return MeshType::attr_name_facet_to_first_corner(); },
        "The name of the attribute that stores the facet to first corner mapping.");
    surface_mesh_class.def_prop_ro_static(
        "attr_name_corner_to_facet",
        [](nb::handle) { return MeshType::attr_name_corner_to_facet(); },
        "The name of the attribute that stores the corner to facet mapping.");
    surface_mesh_class.def_prop_ro_static(
        "attr_name_corner_to_edge",
        [](nb::handle) { return MeshType::attr_name_corner_to_edge(); },
        "The name of the attribute that stores the corner to edge mapping.");
    surface_mesh_class.def_prop_ro_static(
        "attr_name_edge_to_first_corner",
        [](nb::handle) { return MeshType::attr_name_edge_to_first_corner(); },
        "The name of the attribute that stores the edge to first corner mapping.");
    surface_mesh_class.def_prop_ro_static(
        "attr_name_next_corner_around_edge",
        [](nb::handle) { return MeshType::attr_name_next_corner_around_edge(); },
        "The name of the attribute that stores the next corner around edge mapping.");
    surface_mesh_class.def_prop_ro_static(
        "attr_name_vertex_to_first_corner",
        [](nb::handle) { return MeshType::attr_name_vertex_to_first_corner(); },
        "The name of the attribute that stores the vertex to first corner mapping.");
    surface_mesh_class.def_prop_ro_static(
        "attr_name_next_corner_around_vertex",
        [](nb::handle) { return MeshType::attr_name_next_corner_around_vertex(); },
        "The name of the attribute that stores the next corner around vertex mapping.");
    surface_mesh_class.def_prop_ro(
        "attr_id_vertex_to_positions",
        &MeshType::attr_id_vertex_to_positions);
    surface_mesh_class.def_prop_ro("attr_id_corner_to_vertex", &MeshType::attr_id_corner_to_vertex);
    surface_mesh_class.def_prop_ro(
        "attr_id_facet_to_first_corner",
        &MeshType::attr_id_facet_to_first_corner);
    surface_mesh_class.def_prop_ro("attr_id_corner_to_facet", &MeshType::attr_id_corner_to_facet);
    surface_mesh_class.def_prop_ro("attr_id_corner_to_edge", &MeshType::attr_id_corner_to_edge);
    surface_mesh_class.def_prop_ro(
        "attr_id_edge_to_first_corner",
        &MeshType::attr_id_edge_to_first_corner);
    surface_mesh_class.def_prop_ro(
        "attr_id_next_corner_around_edge",
        &MeshType::attr_id_next_corner_around_edge);
    surface_mesh_class.def_prop_ro(
        "attr_id_vertex_to_first_corner",
        &MeshType::attr_id_vertex_to_first_corner);
    surface_mesh_class.def_prop_ro(
        "attr_id_next_corner_around_vertex",
        &MeshType::attr_id_next_corner_around_vertex);
    surface_mesh_class.def(
        "initialize_edges",
        [](MeshType& self, std::optional<Tensor<Index>> tensor) {
            if (tensor.has_value()) {
                auto [edge_data, edge_shape, edge_stride] = tensor_to_span(tensor.value());
                la_runtime_assert(is_dense(edge_shape, edge_stride));
                la_runtime_assert(
                    edge_data.empty() || check_shape(edge_shape, invalid<size_t>(), 2),
                    "Edge tensor mush be of the shape num_edges x 2");
                self.initialize_edges(edge_data);
            } else {
                self.initialize_edges();
            }
        },
        "edges"_a = nb::none(),
        R"(Initialize the edges.

The `edges` tensor provides a predefined ordering of the edges.
If not provided, the edges are initialized in an arbitrary order.

:param edges: M x 2 tensor of predefined edge vertex indices, where M is the number of edges.
:type edges: numpy.ndarray, optional)");
    surface_mesh_class.def("clear_edges", &MeshType::clear_edges);
    surface_mesh_class.def_prop_ro("has_edges", &MeshType::has_edges);
    surface_mesh_class.def("get_edge", &MeshType::get_edge);
    surface_mesh_class.def("get_corner_edge", &MeshType::get_corner_edge);
    surface_mesh_class.def("get_edge_vertices", &MeshType::get_edge_vertices);
    surface_mesh_class.def("find_edge_from_vertices", &MeshType::find_edge_from_vertices);
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

    surface_mesh_class.def(
        "get_matching_attribute_ids",
        [](MeshType& self, AttributeElement* element, AttributeUsage* usage, Index num_channels) {
            std::vector<AttributeId> attr_ids;
            attr_ids.reserve(4);
            self.seq_foreach_attribute_id([&](AttributeId attr_id) {
                const auto name = self.get_attribute_name(attr_id);
                if (self.attr_name_is_reserved(name)) return;
                const auto& attr = self.get_attribute_base(attr_id);
                if (element != nullptr && attr.get_element_type() != *element) return;
                if (usage != nullptr && attr.get_usage() != *usage) return;
                if (num_channels != 0 && attr.get_num_channels() != num_channels) return;
                attr_ids.push_back(attr_id);
            });
            return attr_ids;
        },
        "element"_a = nb::none(),
        "usage"_a = nb::none(),
        "num_channels"_a = 0,
        R"(Get all matching attribute ids with the desired element type, usage and number of channels.

:param element:       The target element type. None matches all element types.
:param usage:         The target usage type.  None matches all usage types.
:param num_channels:  The target number of channels. 0 matches arbitrary number of channels.

:returns: A list of attribute ids matching the target element, usage and number of channels.
)");

    surface_mesh_class.def(
        "__copy__",
        [](MeshType& self) -> MeshType {
            MeshType mesh = self;
            return mesh;
        },
        R"(Create a shallow copy of this mesh.)");

    surface_mesh_class.def(
        "__deepcopy__",
        [](MeshType& self, [[maybe_unused]] std::optional<nb::dict> memo) -> MeshType {
            MeshType mesh = self;
            par_foreach_attribute_write(mesh, [](auto&& attr) {
                // For most of the attributes, just getting a writable reference will trigger a copy
                // of the buffer thanks to the copy-on-write mechanism and the default
                // CopyIfExternal copy policy.

                using AttributeType = std::decay_t<decltype(attr)>;
                if constexpr (AttributeType::IsIndexed) {
                    auto& value_attr = attr.values();
                    if (value_attr.is_external()) {
                        value_attr.create_internal_copy();
                    }
                    auto& index_attr = attr.indices();
                    if (index_attr.is_external()) {
                        index_attr.create_internal_copy();
                    }
                } else {
                    if (attr.is_external()) {
                        attr.create_internal_copy();
                    }
                }
            });
            return mesh;
        },
        "memo"_a = nb::none(),
        R"(Create a deep copy of this mesh.)");

    surface_mesh_class.def(
        "clone",
        [](MeshType& self) {
            auto py_self = nb::find(self);
            return py_self.attr("__deepcopy__")();
        },
        R"(Create a deep copy of this mesh.)");
}

} // namespace lagrange::python
