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

#include <lagrange/CameraTransforms.h>
#include <lagrange/python/binding.h>
#include <lagrange/utils/assert.h>

namespace lagrange::python {

namespace nb = nanobind;

inline void bind_camera_transforms(nb::module_& m)
{
    nb::class_<CameraTransforms>(
        m,
        "CameraTransforms",
        "View and projection matrices defining a camera in world space.")
        .def(nb::init<>())
        .def_prop_rw(
            "view",
            [](const CameraTransforms& self) -> Eigen::Matrix4f { return self.view.matrix(); },
            [](CameraTransforms& self,
               std::variant<Eigen::Matrix4f, Eigen::Matrix<float, 3, 4>> mat_var) {
                Eigen::Matrix4f full = Eigen::Matrix4f::Identity();
                if (auto* mat = std::get_if<Eigen::Matrix4f>(&mat_var)) {
                    la_runtime_assert(
                        (mat->row(3).array() == Eigen::RowVector4f(0.f, 0.f, 0.f, 1.f).array())
                            .all(),
                        "Last row of 4x4 view matrix must be [0, 0, 0, 1]");
                    full = *mat;
                } else {
                    full.topRows<3>() = std::get<Eigen::Matrix<float, 3, 4>>(mat_var);
                }
                self.view = Eigen::Affine3f(full);
            },
            R"(4×4 view transform (world space -> view space).

Accepts a ``(4, 4)`` or ``(3, 4)`` numpy array:

- ``(4, 4)``: full homogeneous matrix; last row must be ``[0, 0, 0, 1]``.
- ``(3, 4)``: compact ``[R | t]`` form; the implicit last row ``[0, 0, 0, 1]`` is appended
  automatically.

The getter always returns a ``(4, 4)`` numpy array.)")
        .def_prop_rw(
            "projection",
            [](const CameraTransforms& self) -> Eigen::Matrix4f {
                return self.projection.matrix();
            },
            [](CameraTransforms& self, const Eigen::Matrix4f& mat) {
                self.projection = Eigen::Projective3f(mat);
            },
            "4x4 projection transform (view space -> NDC space, depth in [-1, 1]).");
}

} // namespace lagrange::python
