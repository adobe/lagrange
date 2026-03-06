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

#include <lagrange/image/Array3D.h>
#include <lagrange/image/View3D.h>
#include <lagrange/python/binding.h>
#include <lagrange/python/tensor_utils.h>
#include <lagrange/utils/assert.h>

namespace lagrange::python {

namespace nb = nanobind;

using ImageShape = nb::shape<-1, -1, -1>;

template <typename Scalar>
using ImageTensor = nb::ndarray<Scalar, ImageShape, nb::numpy, nb::c_contig, nb::device::cpu>;

// Numpy indexes tensors as (row, col, channel), but our mdspan uses (x, y, channel)
// coordinates, so we need to transpose the first two dimensions.

template <typename Scalar>
auto tensor_to_image_view(const ImageTensor<Scalar>& tensor) -> image::experimental::View3D<Scalar>
{
    const image::experimental::dextents<size_t, 3> shape{
        tensor.shape(1),
        tensor.shape(0),
        tensor.shape(2),
    };
    const std::array<size_t, 3> strides{
        static_cast<size_t>(tensor.stride(1)),
        static_cast<size_t>(tensor.stride(0)),
        static_cast<size_t>(tensor.stride(2)),
    };
    const image::experimental::layout_stride::mapping mapping{shape, strides};
    image::experimental::View3D<Scalar> view{
        static_cast<float*>(tensor.data()),
        mapping,
    };
    return view;
}

template <typename Scalar>
void copy_tensor_to_image_view(
    const ImageTensor<Scalar>& tensor,
    image::experimental::View3D<Scalar> image)
{
    const auto width = static_cast<unsigned int>(tensor.shape(1));
    const auto height = static_cast<unsigned int>(tensor.shape(0));
    const auto num_channels = static_cast<unsigned int>(tensor.shape(2));
    la_runtime_assert(
        image.extent(0) == width && image.extent(1) == height && image.extent(2) == num_channels,
        "Tensor and mdspan dimensions do not match");
    for (unsigned int j = 0; j < height; j++) {
        for (unsigned int i = 0; i < width; i++) {
            for (unsigned int c = 0; c < num_channels; c++) {
                image(i, j, c) = tensor(j, i, c);
            }
        }
    }
}

template <typename Scalar>
nb::object image_array_to_tensor(const image::experimental::Array3D<Scalar>& image_)
{
    auto image = const_cast<image::experimental::Array3D<Scalar>&>(image_);
    auto tensor = Tensor<float>(
        static_cast<float*>(image.data()),
        {
            image.extent(1),
            image.extent(0),
            image.extent(2),
        },
        nb::handle(),
        {
            static_cast<int64_t>(image.stride(1)),
            static_cast<int64_t>(image.stride(0)),
            static_cast<int64_t>(image.stride(2)),
        });
    return tensor.cast();
}

} // namespace lagrange::python
