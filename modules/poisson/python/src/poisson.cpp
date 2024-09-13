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

#include <lagrange/poisson/mesh_from_oriented_points.h>

#include <lagrange/AttributeTypes.h>
#include <lagrange/python/tensor_utils.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/invalid.h>
#include <lagrange/Logger.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <nanobind/nanobind.h>
#include <nanobind/eigen/dense.h>
#include <nanobind/stl/array.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/string_view.h>
#include <lagrange/utils/warnon.h>
// clang-format on

namespace lagrange::python {

namespace nb = nanobind;
using namespace nb::literals;

void populate_poisson_module(nb::module_& m)
{
    using Scalar = double;
    using Index = uint32_t;

    using Options = lagrange::poisson::ReconstructionOptions;

    m.def(
        "mesh_from_oriented_points",
        [](Tensor<Scalar> positions,
           Tensor<Scalar> normals,
           unsigned octree_depth,
           float interpolation_weight,
           bool use_normal_length_as_confidence,
           bool use_dirichlet_boundary,
           std::optional<GenericTensor> colors,
           std::string_view output_vertex_depth_attribute_name,
           bool verbose) {
            auto mesh = lagrange::SurfaceMesh<Scalar, Index>();

            auto [positions_data, positions_shape, positions_stride] = tensor_to_span(positions);
            auto [normals_data, normals_shape, normals_stride] = tensor_to_span(normals);

            la_runtime_assert(
                check_shape(positions_shape, invalid<size_t>(), 3) &&
                    is_dense(positions_shape, positions_stride),
                "Input positions should be a N x 3 matrix");
            la_runtime_assert(
                check_shape(normals_shape, invalid<size_t>(), 3) &&
                    is_dense(normals_shape, normals_stride),
                "Input normals should be a N x 3 matrix");

            mesh.wrap_as_vertices(positions_data, positions_shape[0]);
            mesh.wrap_as_attribute<Scalar>(
                "normals",
                AttributeElement::Vertex,
                AttributeUsage::Normal,
                3,
                normals_data);

            Options options;
            options.input_normals = "normals";
            options.octree_depth = octree_depth;
            options.interpolation_weight = interpolation_weight;
            options.use_normal_length_as_confidence = use_normal_length_as_confidence;
            options.use_dirichlet_boundary = use_dirichlet_boundary;
            options.output_vertex_depth_attribute_name = output_vertex_depth_attribute_name;
            options.verbose = verbose;

            if (colors.has_value()) {
                bool unsupported_type = true;
#define LA_X_assign_colors(_, ValueType)                                                \
    if (colors.value().dtype() == nb::dtype<ValueType>()) {                             \
        Tensor<ValueType> local_colors(colors.value().handle());                        \
        auto [colors_data, colors_shape, colors_stride] = tensor_to_span(local_colors); \
        la_runtime_assert(                                                              \
            check_shape(colors_shape, invalid<size_t>(), invalid<size_t>()) &&          \
                is_dense(colors_shape, colors_stride),                                  \
            "Input colors should be a N x K matrix");                                   \
        mesh.wrap_as_attribute<ValueType>(                                              \
            "colors",                                                                   \
            AttributeElement::Vertex,                                                   \
            AttributeUsage::Color,                                                      \
            colors_shape[1],                                                            \
            colors_data);                                                               \
        options.interpolated_attribute_name = "colors";                                 \
        unsupported_type = false;                                                       \
    }
                LA_ATTRIBUTE_X(assign_colors, 0)
#undef LA_X_assign_colors

                if (unsupported_type) {
                    throw std::runtime_error("Unsupported color attribute type.");
                } else {
                    logger().info("Interpolating color attribute");
                }
            }

            return lagrange::poisson::mesh_from_oriented_points<Scalar, Index>(mesh, options);
        },
        "points"_a,
        "normals"_a,
        "octree_depth"_a = Options().octree_depth,
        "interpolation_weight"_a = Options().interpolation_weight,
        "use_normal_length_as_confidence"_a = Options().use_normal_length_as_confidence,
        "use_dirichlet_boundary"_a = Options().use_dirichlet_boundary,
        "colors"_a = nb::none(),
        "output_vertex_depth_attribute_name"_a = Options().output_vertex_depth_attribute_name,
        "verbose"_a = Options().verbose,
        R"(Creates a triangle mesh from an oriented point cloud using Poisson surface reconstruction.

:param points: Input point cloud positions (N x 3 matrix).
:param normals: Input point cloud normals (N x 3 matrix).
:param octree_depth: Maximum octree depth. (If the value is zero then log base 4 of the point count is used.)
:param interpolation_weight: Point interpolation weight (lambda).
:param use_normal_length_as_confidence: Use normal length as confidence.
:param use_dirichlet_boundary: Use Dirichlet boundary conditions.
:param colors: Optional color attribute to interpolate (N x K matrix).
:param output_vertex_depth_attribute_name: Output density attribute name. We use a point's target octree depth as a measure of the sampling density. A lower number means a low sampling density, and can be used to prune low-confidence regions as a post-process.
:param verbose: Output logging information (directly printed to std::cout).)",
        nb::sig("def mesh_from_oriented_points("
                "numpy.typing.NDArray[np.float64], "
                "numpy.typing.NDArray[np.float64], "
                "int = 0, "
                "float = 2.0, "
                "bool = False, "
                "bool = False, "
                "typing.Optional[numpy.typing.NDArray] = None, "
                "str = '', "
                "bool = False) -> lagrange.SurfaceMesh32f")

    );

    // TODO: Fill-in later
}

} // namespace lagrange::python
