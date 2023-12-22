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

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <nanobind/nanobind.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <lagrange/python/tensor_utils.h>

namespace lagrange::python {

namespace nb = nanobind;

void populate_image_module(nb::module_& m)
{
    using namespace lagrange::image;

    nb::enum_<ImagePrecision>(m, "ImagePrecision")
        .value("uint8", ImagePrecision::uint8)
        .value("int8", ImagePrecision::int8)
        .value("uint32", ImagePrecision::uint32)
        .value("int32", ImagePrecision::int32)
        .value("float32", ImagePrecision::float32)
        .value("float64", ImagePrecision::float64)
        .value("float16", ImagePrecision::float16)
        .value("unknown", ImagePrecision::unknown);

    nb::enum_<ImageChannel>(m, "ImageChannel")
        .value("one", image::ImageChannel::one)
        .value("three", image::ImageChannel::three)
        .value("four", image::ImageChannel::four)
        .value("unknown", image::ImageChannel::unknown);

    // the image class is due for a rework so this is a temporary minimal API to access the data.
    nb::class_<ImageStorage>(m, "ImageStorage")
        .def(nb::init<size_t, size_t, size_t>())
        .def_prop_ro(
            "width",
            [](const ImageStorage& img) -> size_t { return img.get_full_size().x(); })
        .def_prop_ro(
            "height",
            [](const ImageStorage& img) -> size_t { return img.get_full_size().y(); })
        .def_prop_ro("stride", &ImageStorage::get_full_stride)
        .def_prop_ro("data", [](ImageStorage& img) {
            span<unsigned char> s(img.data(), img.get_full_size().x() * img.get_full_size().y());
            return span_to_tensor<unsigned char>(s, nb::find(&img));
        });
}

} // namespace lagrange::python
