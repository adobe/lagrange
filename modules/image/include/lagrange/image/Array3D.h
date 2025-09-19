/*
 * Copyright 2025 Adobe. All rights reserved.
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

#include <mdspan/mdarray.hpp>

// Not sure we will keep mdspan, so this is an experimental API
// The plan is to have a low-level API around a multi-dimensional view (equivalent to span<> in core
// module), and multi-dimensional array (in Array3D.h). Then we can build a higher-level Image class
// on top of that (equivalent to the Attribute<> class in core module).
namespace lagrange::image::experimental {

/// @addtogroup group-utils-misc
/// @{

template <class T, typename Extent, typename Layout>
using mdarray = MDSPAN_IMPL_STANDARD_NAMESPACE::Experimental::mdarray<T, Extent, Layout>;

template <typename T>
using Array3D = mdarray<
    T,
    MDSPAN_IMPL_STANDARD_NAMESPACE::dextents<size_t, 3>,
    MDSPAN_IMPL_STANDARD_NAMESPACE::layout_stride>;

///
/// Create an image with the given dimensions and number of channels.
///
/// @param[in]  width         Image width.
/// @param[in]  height        Image height.
/// @param[in]  num_channels  Number of channels.
///
/// @tparam     T             Image value type.
///
/// @return     3D array matching the prescribed shape.
///
template <typename T>
Array3D<T> create_image(size_t width, size_t height, size_t num_channels)
{
    const MDSPAN_IMPL_STANDARD_NAMESPACE::dextents<size_t, 3> shape{width, height, num_channels};
    const std::array<size_t, 3> strides{shape.extent(1) * shape.extent(2), shape.extent(2), 1};
    const MDSPAN_IMPL_STANDARD_NAMESPACE::layout_stride::mapping mapping{shape, strides};
    return Array3D<T>(mapping, std::vector<T>(width * height * num_channels));
}

/// @}

} // namespace lagrange::image::experimental
