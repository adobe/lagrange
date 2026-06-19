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
#include <lagrange/python/winding.h>

#include <lagrange/SurfaceMesh.h>
#include <lagrange/python/binding.h>
#include <lagrange/winding/FastWindingNumber.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <tbb/parallel_for.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <array>
#include <cstdint>

namespace nb = nanobind;
using namespace nb::literals;

namespace lagrange::python {

void populate_winding_module(nb::module_& m)
{
    using Scalar = double;
    using Index = uint32_t;
    using MeshType = SurfaceMesh<Scalar, Index>;

    // Query points provided as an (N, 3) array.
    using ConstPoints = nb::ndarray<const double, nb::shape<-1, 3>, nb::c_contig, nb::device::cpu>;

    nb::class_<winding::FastWindingNumber>(
        m,
        "FastWindingNumber",
        R"(Fast winding number engine for inside/outside queries on triangle soups.

Builds an acceleration structure over a triangle mesh to answer winding-number
based queries, following the fast winding number method of [Barill et al. 2018].

.. note::
   Internally, point coordinates are converted to single precision and vertex
   indices are converted to ``int``.)")
        .def(
            nb::init<const MeshType&>(),
            "mesh"_a,
            R"(Construct an acceleration structure for fast winding number queries.

:param mesh: Input triangle mesh. Must be a 3D triangle mesh.)")
        .def(
            "is_inside",
            [](const winding::FastWindingNumber& self, const std::array<float, 3>& point) {
                return self.is_inside(point);
            },
            "point"_a,
            R"(Determine whether a single query point is inside the volume.

:param point: Query position as a length-3 sequence.

:return: True if the point is inside, False otherwise.)")
        .def(
            "is_inside",
            [](const winding::FastWindingNumber& self, ConstPoints points) {
                const size_t n = points.shape(0);
                const double* data = points.data();
                auto arr = nb::cast<nb::ndarray<bool, nb::numpy, nb::shape<-1>>>(
                    nb::module_::import_("numpy").attr("empty")(n, "dtype"_a = "bool"));
                auto v = arr.view();
                {
                    nb::gil_scoped_release release;
                    tbb::parallel_for(size_t(0), n, [&](size_t i) {
                        v(i) = self.is_inside(
                            {static_cast<float>(data[3 * i + 0]),
                             static_cast<float>(data[3 * i + 1]),
                             static_cast<float>(data[3 * i + 2])});
                    });
                }
                return arr;
            },
            "points"_a,
            R"(Determine whether each of a batch of query points is inside the volume.

:param points: Query positions as a NumPy array of shape (N, 3).

:return: A NumPy array of shape (N,) of booleans, True where the point is inside.)")
        .def(
            "solid_angle",
            [](const winding::FastWindingNumber& self, const std::array<float, 3>& point) {
                return self.solid_angle(point);
            },
            "point"_a,
            R"(Compute the solid angle at a single query point.

:param point: Query position as a length-3 sequence.

:return: Solid angle at the query point.)")
        .def(
            "solid_angle",
            [](const winding::FastWindingNumber& self, ConstPoints points) {
                const size_t n = points.shape(0);
                const double* data = points.data();
                auto arr = nb::cast<nb::ndarray<float, nb::numpy, nb::shape<-1>>>(
                    nb::module_::import_("numpy").attr("empty")(n, "dtype"_a = "float32"));
                auto v = arr.view();
                {
                    nb::gil_scoped_release release;
                    tbb::parallel_for(size_t(0), n, [&](size_t i) {
                        v(i) = self.solid_angle(
                            {static_cast<float>(data[3 * i + 0]),
                             static_cast<float>(data[3 * i + 1]),
                             static_cast<float>(data[3 * i + 2])});
                    });
                }
                return arr;
            },
            "points"_a,
            R"(Compute the solid angle at a batch of query points.

:param points: Query positions as a NumPy array of shape (N, 3).

:return: A NumPy array of shape (N,) of solid angles at each query point.)");
}

} // namespace lagrange::python
