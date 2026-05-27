/*
 * Copyright 2023 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */

#include <lagrange/image/ImageStorage.h>
#include <lagrange/image/ImageType.h>
#include <lagrange/image/split_grid.h>
#include <lagrange/python/binding.h>
#include <lagrange/python/image_utils.h>
#include <lagrange/python/tensor_utils.h>

namespace lagrange::python {

namespace nb = nanobind;
using namespace nb::literals;

void populate_image_module(nb::module_& m)
{
    using namespace lagrange::image;

    nb::enum_<ImagePrecision>(m, "ImagePrecision", "Image pixel precision")
        .value("uint8", ImagePrecision::uint8)
        .value("int8", ImagePrecision::int8)
        .value("uint32", ImagePrecision::uint32)
        .value("int32", ImagePrecision::int32)
        .value("float32", ImagePrecision::float32)
        .value("float64", ImagePrecision::float64)
        .value("float16", ImagePrecision::float16)
        .value("unknown", ImagePrecision::unknown);

    nb::enum_<ImageChannel>(m, "ImageChannel", "Image channel")
        .value("one", image::ImageChannel::one)
        .value("three", image::ImageChannel::three)
        .value("four", image::ImageChannel::four)
        .value("unknown", image::ImageChannel::unknown);

    // the image class is due for a rework so this is a temporary minimal API to access the data.
    nb::class_<ImageStorage>(m, "ImageStorage", "Image storage class")
        .def(nb::init<size_t, size_t, size_t>(), "width"_a, "height"_a, "alignment"_a)
        .def_prop_ro(
            "width",
            [](const ImageStorage& img) -> size_t { return img.get_full_size().x(); },
            "Image width")
        .def_prop_ro(
            "height",
            [](const ImageStorage& img) -> size_t { return img.get_full_size().y(); },
            "Image height")
        .def_prop_ro("stride", &ImageStorage::get_full_stride, "Image stride")
        .def_prop_ro(
            "data",
            [](ImageStorage& img) {
                span<unsigned char> s(
                    img.data(),
                    img.get_full_size().x() * img.get_full_size().y());
                return span_to_tensor<unsigned char>(s, nb::find(&img));
            },
            "Raw image data");

    m.def(
        "split_grid",
        [](ImageTensor<float> grid, size_t num_cells, size_t rows, size_t cols) {
            image::experimental::SplitGridOptions options;
            options.num_cells = num_cells;
            options.rows = rows;
            options.cols = cols;
            const auto cells = image::experimental::split_grid(tensor_to_image_view(grid), options);

            nb::object owner = nb::cast(grid);
            std::vector<nb::object> tensors;
            tensors.reserve(cells.size());
            for (const auto& cell : cells) {
                const size_t shape[3] = {cell.extent(1), cell.extent(0), cell.extent(2)};
                const int64_t strides[3] = {
                    static_cast<int64_t>(cell.stride(1)),
                    static_cast<int64_t>(cell.stride(0)),
                    static_cast<int64_t>(cell.stride(2)),
                };
                nb::ndarray<nb::numpy, float, ImageShape> tensor(
                    cell.data_handle(),
                    3,
                    shape,
                    owner,
                    strides);
                tensors.emplace_back(nb::cast(tensor));
            }
            return tensors;
        },
        "grid"_a,
        "num_cells"_a,
        "rows"_a = 0,
        "cols"_a = 0,
        R"(Split a grid image into ``num_cells`` row-major sub-images.

The grid is split into ``rows`` x ``cols`` cells. A value of zero on either dimension means
auto-detect:

- ``rows=0, cols=0``: pick the factorization producing cells closest to square.
- ``rows=R, cols=0``: derive ``cols = num_cells / R``.
- ``rows=0, cols=C``: derive ``rows = num_cells / C``.
- ``rows=R, cols=C``: validate ``R * C == num_cells``.

Returned views share memory with the input grid (no copy).

:param grid: HxWxC grid image as a numpy array.
:param num_cells: Number of cells to split the grid into.
:param rows: Number of cell rows in the grid (0 = auto).
:param cols: Number of cell columns in the grid (0 = auto).

:return: List of ``num_cells`` numpy views into the grid, in row-major order.)");
}

} // namespace lagrange::python
