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

#include <nonstd/span.hpp>

namespace lagrange {

/// @addtogroup group-utils-misc
/// @{

///
/// A bounds-safe view for sequences of objects.
///
/// @note       The current implementation is intended to be compatible with C++20's std::span<>.
///
template <class T, span_CONFIG_EXTENT_TYPE Extent = ::nonstd::dynamic_extent>
using span = ::nonstd::span<T, Extent>;

/// Span extent type.
using extent_t = span_CONFIG_EXTENT_TYPE;

/// @}

} // namespace lagrange
