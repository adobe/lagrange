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

#include "PyAttribute.h"

#include <lagrange/Attribute.h>
#include <lagrange/AttributeFwd.h>
#include <lagrange/AttributeTypes.h>
#include <lagrange/IndexedAttribute.h>
#include <lagrange/Logger.h>
#include <lagrange/SurfaceMesh.h>
#include <lagrange/find_matching_attributes.h>
#include <lagrange/foreach_attribute.h>
#include <lagrange/python/binding.h>
#include <lagrange/python/tensor_utils.h>
#include <lagrange/utils/BitField.h>
#include <lagrange/utils/Error.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/invalid.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <tbb/parallel_for.h>
#include <lagrange/utils/warnon.h>
// clang-format on

namespace lagrange::python {

template <typename Scalar, typename Index>
void bind_surface_mesh(nanobind::module_& m)
{
    namespace nb = nanobind;
    using namespace nb::literals;
    using namespace lagrange;
    using namespace lagrange::python;

    using MeshType = SurfaceMesh<Scalar, Index>;

    auto surface_mesh_class = nb::class_<MeshType>(m, "SurfaceMesh", "Surface mesh data structure");
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

:param vertex: vertex coordinates)");

    // Handy overload to take python list as argument.
    surface_mesh_class.def(
        "add_vertex",
        [](MeshType& self, nb::list b) {
            la_runtime_assert(static_cast<Index>(b.size()) == self.get_dimension());
            if (self.get_dimension() == 3) {
                self.add_vertex(
                    {nb::cast<Scalar>(b[0]), nb::cast<Scalar>(b[1]), nb::cast<Scalar>(b[2])});
            } else if (self.get_dimension() == 2) {
                self.add_vertex({nb::cast<Scalar>(b[0]), nb::cast<Scalar>(b[1])});
            } else {
                throw std::runtime_error("Dimension mismatch in vertex tensor");
            }
        },
        "vertex"_a,
        R"(Add a vertex to the mesh.

:param vertex: vertex coordinates as a list)");

    surface_mesh_class.def(
        "add_vertices",
        [](MeshType& self, Tensor<Scalar> b) {
            auto [data, shape, stride] = tensor_to_span(b);
            la_runtime_assert(is_dense(shape, stride));
            la_runtime_assert(check_shape(shape, invalid<size_t>(), self.get_dimension()));
            self.add_vertices(static_cast<Index>(shape[0]), data);
        },
        "vertices"_a,
        R"(Add multiple vertices to the mesh.

:param vertices: N x D tensor of vertex coordinates, where N is the number of vertices and D is the dimension)");

    // TODO: combine all add facet functions into `add_facets`.
    surface_mesh_class.def(
        "add_triangle",
        &MeshType::add_triangle,
        "v0"_a,
        "v1"_a,
        "v2"_a,
        R"(Add a triangle to the mesh.

:param v0: first vertex index
:param v1: second vertex index
:param v2: third vertex index

:returns: facet index of the added triangle)");
    surface_mesh_class.def(
        "add_triangles",
        [](MeshType& self, Tensor<Index> b) {
            auto [data, shape, stride] = tensor_to_span(b);
            la_runtime_assert(is_dense(shape, stride));
            la_runtime_assert(check_shape(shape, invalid<size_t>(), 3));
            self.add_triangles(static_cast<Index>(shape[0]), data);
        },
        "triangles"_a,
        R"(Add multiple triangles to the mesh.

:param triangles: N x 3 tensor of vertex indices, where N is the number of triangles)");
    surface_mesh_class.def(
        "add_quad",
        &MeshType::add_quad,
        "v0"_a,
        "v1"_a,
        "v2"_a,
        "v3"_a,
        R"(Add a quad to the mesh.

:param v0: first vertex index
:param v1: second vertex index
:param v2: third vertex index
:param v3: fourth vertex index

:returns: facet index of the added quad)");
    surface_mesh_class.def(
        "add_quads",
        [](MeshType& self, Tensor<Index> b) {
            auto [data, shape, stride] = tensor_to_span(b);
            la_runtime_assert(is_dense(shape, stride));
            la_runtime_assert(check_shape(shape, invalid<size_t>(), 4));
            self.add_quads(static_cast<Index>(shape[0]), data);
        },
        "quads"_a,
        R"(Add multiple quads to the mesh.

:param quads: N x 4 tensor of vertex indices, where N is the number of quads)");
    surface_mesh_class.def(
        "add_polygon",
        [](MeshType& self, Tensor<Index> b) {
            auto [data, shape, stride] = tensor_to_span(b);
            la_runtime_assert(is_dense(shape, stride));
            la_runtime_assert(is_vector(shape));
            self.add_polygon(data);
        },
        "vertices"_a,
        R"(Add a polygon to the mesh.

:param vertices: 1D tensor of vertex indices defining the polygon

:returns: facet index of the added polygon)");
    surface_mesh_class.def(
        "add_polygons",
        [](MeshType& self, Tensor<Index> b) {
            auto [data, shape, stride] = tensor_to_span(b);
            la_runtime_assert(is_dense(shape, stride));
            self.add_polygons(static_cast<Index>(shape[0]), static_cast<Index>(shape[1]), data);
        },
        "polygons"_a,
        R"(Add multiple regular polygons to the mesh.

:param polygons: N x K tensor of vertex indices, where N is the number of polygons and K is the number of vertices per polygon)");
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
        },
        "sizes"_a,
        "indices"_a,
        R"(Add hybrid facets (polygons with varying number of vertices) to the mesh.

:param sizes: 1D tensor specifying the number of vertices for each facet
:param indices: 1D tensor of vertex indices for all facets concatenated together)");
    surface_mesh_class.def(
        "remove_vertices",
        [](MeshType& self, Tensor<Index> b) {
            auto [data, shape, stride] = tensor_to_span(b);
            la_runtime_assert(is_dense(shape, stride));
            la_runtime_assert(is_vector(shape));
            self.remove_vertices(data);
        },
        "vertices"_a,
        R"(Remove selected vertices from the mesh.

:param vertices: 1D tensor of vertex indices to remove)");
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
    surface_mesh_class.def(
        "remove_facets",
        [](MeshType& self, Tensor<Index> b) {
            auto [data, shape, stride] = tensor_to_span(b);
            la_runtime_assert(is_dense(shape, stride));
            la_runtime_assert(is_vector(shape));
            self.remove_facets(data);
        },
        "facets"_a,
        R"(Remove selected facets from the mesh.

:param facets: 1D tensor of facet indices to remove)");
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
    surface_mesh_class.def(
        "clear_vertices",
        &MeshType::clear_vertices,
        R"(Remove all vertices from the mesh.)");
    surface_mesh_class.def(
        "clear_facets",
        &MeshType::clear_facets,
        R"(Remove all facets from the mesh.)");
    surface_mesh_class.def(
        "shrink_to_fit",
        &MeshType::shrink_to_fit,
        R"(Shrink the internal storage to fit the current mesh size.)");
    surface_mesh_class.def(
        "compress_if_regular",
        &MeshType::compress_if_regular,
        R"(Compress the mesh representation if it is regular (all facets have the same number of vertices).

:returns: True if the mesh was compressed, False otherwise)");
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
    surface_mesh_class.def(
        "get_position",
        [](MeshType& self, Index i) {
            return span_to_tensor(self.get_position(i), nb::find(&self));
        },
        "vertex_id"_a,
        R"(Get the position of a vertex.

:param vertex_id: vertex index

:returns: position coordinates as a tensor)");
    surface_mesh_class.def(
        "ref_position",
        [](MeshType& self, Index i) {
            return span_to_tensor(self.ref_position(i), nb::find(&self));
        },
        "vertex_id"_a,
        R"(Get a mutable reference to the position of a vertex.

:param vertex_id: vertex index

:returns: mutable position coordinates as a tensor)");
    surface_mesh_class.def(
        "get_facet_size",
        &MeshType::get_facet_size,
        "facet_id"_a,
        R"(Get the number of vertices in a facet.

:param facet_id: facet index

:returns: number of vertices in the facet)");
    surface_mesh_class.def(
        "get_facet_vertex",
        &MeshType::get_facet_vertex,
        "facet_id"_a,
        "local_vertex_id"_a,
        R"(Get a vertex index from a facet.

:param facet_id: facet index
:param local_vertex_id: local vertex index within the facet (0 to facet_size-1)

:returns: global vertex index)");
    surface_mesh_class.def(
        "get_facet_corner_begin",
        &MeshType::get_facet_corner_begin,
        "facet_id"_a,
        R"(Get the first corner index of a facet.

:param facet_id: facet index

:returns: first corner index of the facet)");
    surface_mesh_class.def(
        "get_facet_corner_end",
        &MeshType::get_facet_corner_end,
        "facet_id"_a,
        R"(Get the end corner index of a facet (one past the last corner).

:param facet_id: facet index

:returns: end corner index of the facet)");
    surface_mesh_class.def(
        "get_corner_vertex",
        &MeshType::get_corner_vertex,
        "corner_id"_a,
        R"(Get the vertex index associated with a corner.

:param corner_id: corner index

:returns: vertex index)");
    surface_mesh_class.def(
        "get_corner_facet",
        &MeshType::get_corner_facet,
        "corner_id"_a,
        R"(Get the facet index associated with a corner.

:param corner_id: corner index

:returns: facet index)");
    surface_mesh_class.def(
        "get_facet_vertices",
        [](MeshType& self, Index f) {
            return span_to_tensor(self.get_facet_vertices(f), nb::find(&self));
        },
        "facet_id"_a,
        R"(Get all vertex indices of a facet.

:param facet_id: facet index

:returns: vertex indices as a tensor)");
    surface_mesh_class.def(
        "ref_facet_vertices",
        [](MeshType& self, Index f) {
            return span_to_tensor(self.ref_facet_vertices(f), nb::find(&self));
        },
        "facet_id"_a,
        R"(Get a mutable reference to all vertex indices of a facet.

:param facet_id: facet index

:returns: mutable vertex indices as a tensor)");
    surface_mesh_class.def(
        "get_attribute_id",
        &MeshType::get_attribute_id,
        "name"_a,
        R"(Get the attribute ID by name.

:param name: attribute name

:returns: attribute ID)");
    surface_mesh_class.def(
        "get_attribute_name",
        &MeshType::get_attribute_name,
        "id"_a,
        R"(Get the attribute name by ID.

:param id: attribute ID

:returns: attribute name)");
    surface_mesh_class.def(
        "create_attribute",
        [](MeshType& self,
           std::string_view name,
           std::variant<std::monostate, AttributeElement, std::string_view> element,
           std::variant<std::monostate, AttributeUsage, std::string_view> usage,
           std::variant<std::monostate, GenericTensor, nb::list> initial_values,
           std::variant<std::monostate, Tensor<Index>, GenericTensor, nb::list> initial_indices,
           std::optional<Index> num_channels,
           std::optional<nb::type_object> dtype) {
            const bool with_initial_values = initial_values.index() != 0;
            const bool with_initial_indices = initial_indices.index() != 0;

            // Infer number of channels.
            Index n = invalid<Index>();
            if (num_channels.has_value()) {
                n = num_channels.value();
            } else if (with_initial_values) {
                if (initial_values.index() == 1) {
                    const auto& values = std::get<GenericTensor>(initial_values);
                    la_runtime_assert(
                        values.ndim() == 1 || values.ndim() == 2,
                        "Only vector or matrix are accepted as initial values.");
                    n = values.ndim() == 1 ? 1 : static_cast<Index>(values.shape(1));
                } else if (initial_values.index() == 2) {
                    n = 1;
                }
            } else {
                throw nb::type_error("Either number of channels or initial values are required!");
            }

            // Infer element type.
            AttributeElement elem_type;
            std::visit(
                [&](auto&& value) {
                    using T = std::decay_t<decltype(value)>;
                    if constexpr (std::is_same_v<T, AttributeElement>) {
                        elem_type = value;
                    } else if (with_initial_indices) {
                        elem_type = AttributeElement::Indexed;
                    } else if constexpr (std::is_same_v<T, std::string_view>) {
                        if (value == "Vertex") {
                            elem_type = AttributeElement::Vertex;
                        } else if (value == "Facet") {
                            elem_type = AttributeElement::Facet;
                        } else if (value == "Edge") {
                            elem_type = AttributeElement::Edge;
                        } else if (value == "Corner") {
                            elem_type = AttributeElement::Corner;
                        } else if (value == "Value") {
                            elem_type = AttributeElement::Value;
                        } else if (value == "Indexed") {
                            elem_type = AttributeElement::Indexed;
                        } else {
                            throw nb::type_error("Invalid element type!");
                        }
                    } else if (with_initial_values) {
                        // TODO guess element type based on the shape of initial values.
                        const Index num_vertices = self.get_num_vertices();
                        const Index num_facets = self.get_num_facets();
                        const Index num_corners = self.get_num_corners();
                        const Index num_edges =
                            self.has_edges() ? self.get_num_edges() : invalid<Index>();

                        Index num_rows = invalid<Index>();
                        if (initial_values.index() == 1) {
                            const auto& values = std::get<GenericTensor>(initial_values);
                            num_rows = values.shape(0);
                        } else if (initial_values.index() == 2) {
                            const auto& values = std::get<nb::list>(initial_values);
                            num_rows = nb::len(values);
                        }
                        la_debug_assert(num_rows != invalid<Index>());

                        if (num_rows == num_vertices) {
                            la_runtime_assert(
                                num_rows != num_facets,
                                "Cannot infer attribute element due to ambiguity: vertices vs "
                                "facets");
                            la_runtime_assert(
                                num_rows != num_edges,
                                "Cannot infer attribute element due to ambiguity: vertices vs "
                                "edges");
                            la_runtime_assert(
                                num_rows != num_corners,
                                "Cannot infer attribute element due to ambiguity: vertices vs "
                                "corners");
                            elem_type = AttributeElement::Vertex;
                        } else if (num_rows == num_facets) {
                            la_runtime_assert(
                                num_rows != num_edges,
                                "Cannot infer attribute element due to ambiguity: facets vs "
                                "edges");
                            la_runtime_assert(
                                num_rows != num_corners,
                                "Cannot infer attribute element due to ambiguity: facets vs "
                                "corners");
                            elem_type = AttributeElement::Facet;
                        } else if (num_rows == num_corners) {
                            la_runtime_assert(
                                num_rows != num_edges,
                                "Cannot infer attribute element due to ambiguity: corners vs "
                                "edges");
                            elem_type = AttributeElement::Corner;
                        } else if (num_rows == num_edges) {
                            elem_type = AttributeElement::Edge;
                        } else {
                            throw nb::type_error(
                                "Cannot infer attribute element type from initial_values!");
                        }
                    } else {
                        throw nb::type_error("Invalid element type!");
                    }
                },
                element);

            // Infer usage.
            AttributeUsage usage_type;
            std::visit(
                [&](auto&& value) {
                    using T = std::decay_t<decltype(value)>;
                    if constexpr (std::is_same_v<T, AttributeUsage>) {
                        usage_type = value;
                    } else if constexpr (std::is_same_v<T, std::string_view>) {
                        if (value == "Vector") {
                            usage_type = AttributeUsage::Vector;
                        } else if (value == "Scalar") {
                            usage_type = AttributeUsage::Scalar;
                        } else if (value == "Position") {
                            usage_type = AttributeUsage::Position;
                        } else if (value == "Normal") {
                            usage_type = AttributeUsage::Normal;
                        } else if (value == "Tangent") {
                            usage_type = AttributeUsage::Tangent;
                        } else if (value == "Bitangent") {
                            usage_type = AttributeUsage::Bitangent;
                        } else if (value == "Color") {
                            usage_type = AttributeUsage::Color;
                        } else if (value == "UV") {
                            usage_type = AttributeUsage::UV;
                        } else if (value == "VertexIndex") {
                            usage_type = AttributeUsage::VertexIndex;
                        } else if (value == "FacetIndex") {
                            usage_type = AttributeUsage::FacetIndex;
                        } else if (value == "CornerIndex") {
                            usage_type = AttributeUsage::CornerIndex;
                        } else if (value == "EdgeIndex") {
                            usage_type = AttributeUsage::EdgeIndex;
                        } else {
                            throw nb::type_error("Invalid usage type!");
                        }
                    } else {
                        if (n == 1) {
                            usage_type = AttributeUsage::Scalar;
                        } else {
                            usage_type = AttributeUsage::Vector;
                        }
                    }
                },
                usage);

            auto create_attribute = [&](auto values) {
                using ValueType = typename std::decay_t<decltype(values)>::element_type;

                span<const ValueType> init_values;
                span<const Index> init_indices;
                std::vector<Index> index_storage;

                // Extract initial values.
                if (with_initial_values) {
                    init_values = values;
                    la_debug_assert(values.size() % n == 0);
                } else {
                    la_runtime_assert(
                        num_channels.has_value(),
                        "Number of channels is required when initial values are not provided!");
                    la_runtime_assert(
                        dtype.has_value(),
                        "dtype is required when initial values are not provided!");
                }

                // Extract initial indices.
                if (const Tensor<Index>* tensor_ptr =
                        std::get_if<Tensor<Index>>(&initial_indices)) {
                    la_debug_assert(with_initial_indices);
                    const auto& indices = *tensor_ptr;
                    auto [index_data, index_shape, index_stride] = tensor_to_span(indices);
                    la_runtime_assert(is_dense(index_shape, index_stride));
                    init_indices = index_data;
                } else if (
                    GenericTensor* generic_tensor_ptr =
                        std::get_if<GenericTensor>(&initial_indices)) {
                    la_debug_assert(with_initial_indices);
                    auto& indices = *generic_tensor_ptr;
                    index_storage.resize(indices.size());

#define LA_X_create_attribute_index(_, IndexType)                                    \
    if (indices.dtype() == nb::dtype<IndexType>()) {                                 \
        auto view = indices.template view<IndexType, nb::ndim<1>>();                 \
        std::copy(view.data(), view.data() + indices.size(), index_storage.begin()); \
    }
                    LA_ATTRIBUTE_INDEX_X(create_attribute_index, 0)
#undef LA_X_create_attribute_index
                    init_indices = span<Index>(index_storage.data(), index_storage.size());
                } else if (const nb::list* list_ptr = std::get_if<nb::list>(&initial_indices)) {
                    la_debug_assert(with_initial_indices);
                    const nb::list& py_list = *list_ptr;
                    index_storage = nb::cast<std::vector<Index>>(py_list);
                    init_indices = span<Index>(index_storage.begin(), index_storage.size());
                }

                return self.template create_attribute<ValueType>(
                    name,
                    elem_type,
                    usage_type,
                    n,
                    init_values,
                    init_indices,
                    AttributeCreatePolicy::ErrorIfReserved);
            };

            if (const GenericTensor* tensor_ptr = std::get_if<GenericTensor>(&initial_values)) {
                const auto& values = *tensor_ptr;
#define LA_X_create_attribute(_, ValueType)                                                 \
    if (values.dtype() == nb::dtype<ValueType>()) {                                         \
        Tensor<ValueType> local_values(values.handle());                                    \
        auto [value_data, value_shape, value_stride] = tensor_to_span(local_values);        \
        la_runtime_assert(is_dense(value_shape, value_stride));                             \
        if (num_channels.has_value()) {                                                     \
            Index nn = value_shape.size() == 1 ? 1 : static_cast<Index>(value_shape[1]);    \
            la_runtime_assert(nn == n, "Number of channels does not match initial_values"); \
        }                                                                                   \
        return create_attribute(value_data);                                                \
    }
                LA_ATTRIBUTE_X(create_attribute, 0)
#undef LA_X_create_attribute
            } else if (const nb::list* list_ptr = std::get_if<nb::list>(&initial_values)) {
                auto values = nb::cast<std::vector<double>>(*list_ptr);
                return create_attribute(span<double>(values.data(), values.size()));
            } else if (dtype.has_value()) {
                const auto& t = dtype.value();
                auto np = nb::module_::import_("numpy");
                if (t.is(&PyFloat_Type)) {
                    // Native python float is a C double.
                    span<double> local_values;
                    return create_attribute(local_values);
                } else if (t.is(np.attr("float32"))) {
                    span<float> local_values;
                    return create_attribute(local_values);
                } else if (t.is(np.attr("float64"))) {
                    span<double> local_values;
                    return create_attribute(local_values);
                } else if (t.is(np.attr("int8"))) {
                    span<int8_t> local_values;
                    return create_attribute(local_values);
                } else if (t.is(np.attr("int16"))) {
                    span<int16_t> local_values;
                    return create_attribute(local_values);
                } else if (t.is(np.attr("int32"))) {
                    span<int32_t> local_values;
                    return create_attribute(local_values);
                } else if (t.is(np.attr("int64"))) {
                    span<int64_t> local_values;
                    return create_attribute(local_values);
                } else if (t.is(np.attr("uint8"))) {
                    span<uint8_t> local_values;
                    return create_attribute(local_values);
                } else if (t.is(np.attr("uint16"))) {
                    span<uint16_t> local_values;
                    return create_attribute(local_values);
                } else if (t.is(np.attr("uint32"))) {
                    span<uint32_t> local_values;
                    return create_attribute(local_values);
                } else if (t.is(np.attr("uint64"))) {
                    span<uint64_t> local_values;
                    return create_attribute(local_values);
                }
            }
            throw nb::type_error("`initial_values` and `dtype` cannot both be None!");
        },
        "name"_a,
        "element"_a = nb::none(),
        "usage"_a = nb::none(),
        "initial_values"_a = nb::none(),
        "initial_indices"_a = nb::none(),
        "num_channels"_a = nb::none(),
        "dtype"_a = nb::none(),
        nb::sig(
            "def create_attribute(self, "
            "name: str, "
            "element: typing.Union[AttributeElement, "
            "typing.Literal["
            "'Vertex', 'Facet', 'Edge', 'Corner', 'Value', 'Indexed'"
            "], None] = None, "
            "usage: typing.Union[AttributeUsage, "
            "typing.Literal["
            "'Vector', 'Scalar', 'Position', 'Normal', 'Tangent', 'Bitangent', 'Color', 'UV', "
            "'VertexIndex', 'FacetIndex', 'CornerIndex', 'EdgeIndex'"
            "], None] = None, "
            "initial_values: typing.Union[numpy.typing.NDArray, typing.List[float], None] = None, "
            "initial_indices: typing.Union[numpy.typing.NDArray, typing.List[int], None] = None, "
            "num_channels: typing.Optional[int] = None, "
            "dtype: typing.Optional[numpy.typing.DTypeLike] = None) -> int"),
        R"(Create an attribute.

:param name: Name of the attribute.
:param element: Element type of the attribute. If None, derive from the shape of initial values.
:param usage: Usage type of the attribute. If None, derive from the shape of initial values or the number of channels.
:param initial_values: Initial values of the attribute.
:param initial_indices: Initial indices of the attribute (Indexed attribute only).
:param num_channels: Number of channels of the attribute.
:param dtype: Data type of the attribute.

:returns: The id of the created attribute.

.. note::
   If `element` is None, it will be derived based on the cardinality of the mesh elements.
   If there is an ambiguity, an exception will be raised.
   In addition, explicit `element` specification is required for value attributes.

.. note::
   If `usage` is None, it will be derived based on the shape of `initial_values` or `num_channels` if specified.)");

    surface_mesh_class.def(
        "create_attribute_from",
        &MeshType::template create_attribute_from<Scalar, Index>,
        "name"_a,
        "source_mesh"_a,
        "source_name"_a = "",
        R"(Shallow copy an attribute from another mesh.

:param name: Name of the attribute.
:param source_mesh: Source mesh.
:param source_name: Name of the attribute in the source mesh. If empty, use the same name as `name`.

:returns: The id of the created attribute.)");

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
:param element: Element type of the attribute.
:param usage: Usage type of the attribute.
:param values: Values of the attribute.

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
:param usage: Usage type of the attribute.
:param values: Values of the attribute.
:param indices: Indices of the attribute.

:returns: The id of the created attribute.)");
    surface_mesh_class.def(
        "duplicate_attribute",
        &MeshType::duplicate_attribute,
        "old_name"_a,
        "new_name"_a,
        R"(Duplicate an attribute with a new name.

:param old_name: name of the attribute to duplicate
:param new_name: name for the new attribute

:returns: attribute ID of the duplicated attribute)");
    surface_mesh_class.def(
        "rename_attribute",
        &MeshType::rename_attribute,
        "old_name"_a,
        "new_name"_a,
        R"(Rename an attribute.

:param old_name: current name of the attribute
:param new_name: new name for the attribute)");
    surface_mesh_class.def(
        "delete_attribute",
        &MeshType::delete_attribute,
        "name"_a,
        "policy"_a /*= AttributeDeletePolicy::ErrorIfReserved*/);
    surface_mesh_class.def(
        "delete_attribute",
        [](MeshType& self, std::string_view name) { self.delete_attribute(name); },
        "name"_a,
        R"(Delete an attribute by name.

:param name: Name of the attribute.)");
    surface_mesh_class.def(
        "delete_attribute",
        [](MeshType& self, AttributeId id) { self.delete_attribute(self.get_attribute_name(id)); },
        "id"_a,
        R"(Delete an attribute by id.

:param id: Id of the attribute.)");
    surface_mesh_class.def(
        "has_attribute",
        &MeshType::has_attribute,
        "name"_a,
        R"(Check if an attribute exists.

:param name: attribute name

:returns: True if the attribute exists, False otherwise)");
    surface_mesh_class.def(
        "is_attribute_indexed",
        static_cast<bool (MeshType::*)(AttributeId) const>(&MeshType::is_attribute_indexed),
        "id"_a,
        R"(Check if an attribute is indexed.

:param id: attribute ID

:returns: True if the attribute is indexed, False otherwise)");
    surface_mesh_class.def(
        "is_attribute_indexed",
        static_cast<bool (MeshType::*)(std::string_view) const>(&MeshType::is_attribute_indexed),
        "name"_a,
        R"(Check if an attribute is indexed.

:param name: attribute name

:returns: True if the attribute is indexed, False otherwise)");

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
                    "Attribute {} is indexed!  Please use `indexed_attribute` property "
                    "instead.",
                    id));
            if (!sharing) ensure_attribute_is_not_shared(self, id);
            return PyAttribute(self._ref_attribute_ptr(id));
        },
        "id"_a,
        "sharing"_a = true,
        R"(Get an attribute by id.

:param id: Id of the attribute.
:param sharing: Whether to allow sharing the attribute with other meshes.

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
:param sharing: Whether to allow sharing the attribute with other meshes.

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
:param sharing: Whether to allow sharing the attribute with other meshes.

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
:param sharing: Whether to allow sharing the attribute with other meshes.

:returns: The indexed attribute.)");
    surface_mesh_class.def(
        "__attribute_ref_count__",
        [](MeshType& self, AttributeId id) {
            auto ptr = self._get_attribute_ptr(id);
            return ptr.use_count();
        },
        "id"_a,
        R"(Get the reference count of an attribute (for debugging purposes).

:param id: attribute ID

:returns: reference count of the attribute)");
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
            auto& attr = self.template ref_attribute<Index>(id);
            attr.set_growth_policy(AttributeGrowthPolicy::WarnAndCopy);
        },
        "Facets of the mesh.");
    surface_mesh_class.def_prop_ro(
        "edges",
        [](MeshType& self) {
            self.initialize_edges();
            auto num_edges = self.get_num_edges();
            std::vector<Index> data(num_edges * 2);
            tbb::parallel_for(Index{0}, num_edges, [&](Index i) {
                auto [v0, v1] = self.get_edge_vertices(i);
                data[i * 2] = v0;
                data[i * 2 + 1] = v1;
            });
            nb::ndarray<Index, nb::shape<-1, 2>, nb::numpy, nb::c_contig, nb::device::cpu> edges(
                data.data(),
                {static_cast<size_t>(num_edges), 2});
            return edges.cast();
        },
        "Edges of the mesh.");
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
:param num_vertices: Number of vertices.

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
            auto& attr = self.template ref_attribute<Index>(id);
            attr.set_growth_policy(AttributeGrowthPolicy::WarnAndCopy);
            return id;
        },
        "tensor"_a,
        "num_facets"_a,
        "vertex_per_facet"_a,
        R"(Wrap a tensor as a list of regular facets.

:param tensor: The tensor to wrap.
:param num_facets: Number of facets.
:param vertex_per_facet: Number of vertices per facet.

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
            auto& attr = self.template ref_attribute<Index>(id);
            attr.set_growth_policy(AttributeGrowthPolicy::WarnAndCopy);
            return id;
        },
        "offsets"_a,
        "num_facets"_a,
        "facets"_a,
        "num_corners"_a,
        R"(Wrap a tensor as a list of hybrid facets.

:param offsets: The offset indices into the facets array.
:param num_facets: Number of facets.
:param facets: The indices of the vertices of the facets.
:param num_corners: Number of corners.

:return: The id of the wrapped facet attribute.)");
    surface_mesh_class.def_static(
        "attr_name_is_reserved",
        &MeshType::attr_name_is_reserved,
        "name"_a,
        R"(Check if an attribute name is reserved.

:param name: attribute name to check

:returns: True if the name is reserved, False otherwise)");
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
        "attr_id_vertex_to_position",
        &MeshType::attr_id_vertex_to_position);
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

:param edges: M x 2 tensor of predefined edge vertex indices, where M is the number of edges.)");
    surface_mesh_class.def(
        "clear_edges",
        &MeshType::clear_edges,
        R"(Clear all edge connectivity information.)");
    surface_mesh_class.def_prop_ro("has_edges", &MeshType::has_edges);
    surface_mesh_class.def(
        "get_edge",
        &MeshType::get_edge,
        "facet_id"_a,
        "lv"_a,
        R"(Get the edge index associated with a local vertex of a facet.

:param facet_id: facet index
:param lv: local vertex index of the facet

:returns: edge index)");
    surface_mesh_class.def(
        "get_corner_edge",
        &MeshType::get_corner_edge,
        "corner_id"_a,
        R"(Get the edge index associated with a corner.

:param corner_id: corner index

:returns: edge index)");
    surface_mesh_class.def(
        "get_edge_vertices",
        &MeshType::get_edge_vertices,
        "edge_id"_a,
        R"(Get the two vertex indices of an edge.

:param edge_id: edge index

:returns: tuple of (vertex1_id, vertex2_id))");
    surface_mesh_class.def(
        "find_edge_from_vertices",
        &MeshType::find_edge_from_vertices,
        "vertex1_id"_a,
        "vertex2_id"_a,
        R"(Find the edge connecting two vertices.

:param vertex1_id: first vertex index
:param vertex2_id: second vertex index

:returns: edge index, or invalid_index if no such edge exists)");
    surface_mesh_class.def(
        "get_first_corner_around_edge",
        &MeshType::get_first_corner_around_edge,
        "edge_id"_a,
        R"(Get the first corner around an edge.

:param edge_id: edge index

:returns: first corner index around the edge)");
    surface_mesh_class.def(
        "get_next_corner_around_edge",
        &MeshType::get_next_corner_around_edge,
        "corner_id"_a,
        R"(Get the next corner around the same edge.

:param corner_id: current corner index

:returns: next corner index around the same edge)");
    surface_mesh_class.def(
        "get_first_corner_around_vertex",
        &MeshType::get_first_corner_around_vertex,
        "vertex_id"_a,
        R"(Get the first corner around a vertex.

:param vertex_id: vertex index

:returns: first corner index around the vertex)");
    surface_mesh_class.def(
        "get_next_corner_around_vertex",
        &MeshType::get_next_corner_around_vertex,
        "corner_id"_a,
        R"(Get the next corner around the same vertex.

:param corner_id: current corner index

:returns: next corner index around the same vertex)");
    surface_mesh_class.def(
        "count_num_corners_around_edge",
        &MeshType::count_num_corners_around_edge,
        "edge_id"_a,
        R"(Count the number of corners around an edge.

:param edge_id: edge index

:returns: number of corners around the edge)");
    surface_mesh_class.def(
        "count_num_corners_around_vertex",
        &MeshType::count_num_corners_around_vertex,
        "vertex_id"_a,
        R"(Count the number of corners around a vertex.

:param vertex_id: vertex index

:returns: number of corners around the vertex)");
    surface_mesh_class.def(
        "get_counterclockwise_corner_around_vertex",
        &MeshType::get_counterclockwise_corner_around_vertex,
        "corner"_a,
        R"(Get the counterclockwise corner around the vertex associated with the input corner.

.. note::
    If the vertex is a non-manifold vertex, only one "umbrella" (a set of connected
    corners based on edge-connectivity) will be visited.

    If the traversal reaches a boundary or a non-manifold edge, the next adjacent corner
    is not well defined. It will return `invalid_index` in this case.

:param corner: The input corner index.

:returns: The counterclockwise corner index or `invalid_index` if none exists.)");
    surface_mesh_class.def(
        "get_clockwise_corner_around_vertex",
        &MeshType::get_clockwise_corner_around_vertex,
        "corner"_a,
        R"(Get the clockwise corner around the vertex associated with the input corner.

.. note::
    If the vertex is a non-manifold vertex, only one "umbrella" (a set of connected
    corners based on edge-connectivity) will be visited.

    If the traversal reaches a boundary or a non-manifold edge, the next adjacent corner
    is not well defined. It will return `invalid_index` in this case.

:param corner: The input corner index.

:returns: The clockwise corner index or `invalid_index` if none exists.)");
    surface_mesh_class.def(
        "get_one_facet_around_edge",
        &MeshType::get_one_facet_around_edge,
        "edge_id"_a,
        R"(Get one facet adjacent to an edge.

:param edge_id: edge index

:returns: facet index adjacent to the edge)");
    surface_mesh_class.def(
        "get_one_corner_around_edge",
        &MeshType::get_one_corner_around_edge,
        "edge_id"_a,
        R"(Get one corner around an edge.

:param edge_id: edge index

:returns: corner index around the edge)");
    surface_mesh_class.def(
        "get_one_corner_around_vertex",
        &MeshType::get_one_corner_around_vertex,
        "vertex_id"_a,
        R"(Get one corner around a vertex.

:param vertex_id: vertex index

:returns: corner index around the vertex)");
    surface_mesh_class.def(
        "is_boundary_edge",
        &MeshType::is_boundary_edge,
        "edge_id"_a,
        R"(Check if an edge is on the boundary.

:param edge_id: edge index

:returns: True if the edge is on the boundary, False otherwise)");

    struct MetaData
    {
        MeshType* mesh = nullptr;

        std::vector<AttributeId> get_metadata() const
        {
            la_runtime_assert(mesh != nullptr, "MetaData is not initialized.");
            lagrange::AttributeMatcher opts;
            opts.usages = AttributeUsage::String;
            opts.element_types = AttributeElement::Value;
            opts.num_channels = 1;
            return lagrange::find_matching_attributes(*mesh, opts);
        }
    };
    auto meta_data_class = nb::class_<MetaData>(m, "MetaData", "Metadata `dict` of the mesh");
    meta_data_class.def("__len__", [](const MetaData& self) {
        auto data = self.get_metadata();
        return data.size();
    });
    meta_data_class.def("__getitem__", [](const MetaData& self, std::string_view key) {
        return self.mesh->get_metadata(key);
    });
    meta_data_class.def(
        "__setitem__",
        [](MetaData& self, std::string_view key, std::string_view value) {
            la_runtime_assert(self.mesh != nullptr, "MetaData is not initialized.");
            if (self.mesh->has_attribute(key)) {
                self.mesh->set_metadata(key, value);
            } else {
                self.mesh->create_metadata(key, value);
            }
        });
    meta_data_class.def("__delitem__", [](MetaData& self, std::string_view key) {
        la_runtime_assert(self.mesh != nullptr, "MetaData is not initialized.");
        self.mesh->delete_attribute(key);
    });
    meta_data_class.def("__repr__", [](const MetaData& self) -> std::string {
        auto data = self.get_metadata();
        if (data.empty()) return "MetaData({})";

        std::string r;
        for (auto id : data) {
            auto name = self.mesh->get_attribute_name(id);
            auto value = self.mesh->get_metadata(id);
            fmt::format_to(std::back_inserter(r), "  {}: {},\n", name, value);
        }
        return fmt::format("MetaData(\n{})", r);
    });

    surface_mesh_class.def_prop_ro(
        "metadata",
        [](MeshType& self) {
            MetaData meta_data;
            meta_data.mesh = &self;
            return meta_data;
        },
        "Metadata of the mesh.");

    surface_mesh_class.def(
        "get_matching_attribute_ids",
        [](MeshType& self,
           std::optional<AttributeElement> element,
           std::optional<AttributeUsage> usage,
           Index num_channels) {
            AttributeMatcher opts;
            if (usage.has_value()) {
                opts.usages = usage.value();
            }
            if (element.has_value()) {
                opts.element_types = element.value();
            }
            opts.num_channels = num_channels;
            return find_matching_attributes(self, opts);
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
        "get_matching_attribute_id",
        [](MeshType& self,
           std::optional<AttributeElement> element,
           std::optional<AttributeUsage> usage,
           Index num_channels) {
            std::optional<AttributeId> result;
            self.seq_foreach_attribute_id([&](AttributeId attr_id) {
                if (result.has_value()) {
                    return;
                }
                const auto name = self.get_attribute_name(attr_id);
                if (self.attr_name_is_reserved(name)) return;
                const auto& attr = self.get_attribute_base(attr_id);
                if (element && attr.get_element_type() != *element) return;
                if (usage && attr.get_usage() != *usage) return;
                if (num_channels != 0 && attr.get_num_channels() != num_channels) return;
                result = attr_id;
            });
            return result;
        },
        "element"_a = nb::none(),
        "usage"_a = nb::none(),
        "num_channels"_a = 0,
        R"(Get one matching attribute id with the desired element type, usage and number of channels.

:param element:       The target element type. None matches all element types.
:param usage:         The target usage type.  None matches all usage types.
:param num_channels:  The target number of channels. 0 matches arbitrary number of channels.

:returns: An attribute id matching the target element, usage and number of channels, if found. None otherwise.
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
                // For most of the attributes, just getting a writable reference will trigger a
                // copy of the buffer thanks to the copy-on-write mechanism and the default
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
        [](MeshType& self, bool strip) -> MeshType {
            if (strip) {
                return MeshType::stripped_copy(self);
            } else {
                auto py_self = nb::find(self);
                return nb::cast<MeshType>(py_self.attr("__deepcopy__")());
            }
        },
        "strip"_a = false,
        R"(Create a deep copy of this mesh.

:param strip: If True, strip the mesh of all attributes except for the reserved attributes.)");
}

} // namespace lagrange::python
