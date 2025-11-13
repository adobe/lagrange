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

#include <mdspan/mdspan.hpp>

// Not sure we will keep mdspan, so this is an experimental API
// The plan is to have a low-level API around a multi-dimensional view (equivalent to span<> in core
// module), and multi-dimensional array (in Array3D.h). Then we can build a higher-level Image class
// on top of that (equivalent to the Attribute<> class in core module).
namespace lagrange::image::experimental {

/// @addtogroup group-utils-misc
/// @{

template <class T, typename Extent, typename Layout>
using mdspan = MDSPAN_IMPL_STANDARD_NAMESPACE::mdspan<T, Extent, Layout>;

template <class IndexType, size_t... Extents>
using extents = MDSPAN_IMPL_STANDARD_NAMESPACE::extents<IndexType, Extents...>;

template <typename IndexType, size_t Rank>
using dextents = MDSPAN_IMPL_STANDARD_NAMESPACE::dextents<IndexType, Rank>;

using layout_left = MDSPAN_IMPL_STANDARD_NAMESPACE::layout_left;
using layout_right = MDSPAN_IMPL_STANDARD_NAMESPACE::layout_right;
using layout_stride = MDSPAN_IMPL_STANDARD_NAMESPACE::layout_stride;

using full_extent_t = MDSPAN_IMPL_STANDARD_NAMESPACE::full_extent_t;

constexpr auto dynamic_extent = MDSPAN_IMPL_STANDARD_NAMESPACE::dynamic_extent;

template <typename T>
using View3D = mdspan<T, dextents<size_t, 3>, layout_stride>;

/// @}

} // namespace lagrange::image::experimental
