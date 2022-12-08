/*
 * Copyright 2021 Adobe. All rights reserved.
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

#pragma message("SmallSet has been renamed StackSet. Please update your code accordingly.")

#include <lagrange/utils/StackSet.h>

namespace lagrange {

/// @cond LA_INTERNAL_DOCS

// Has been renamed. Please use the new type name instead.
[[deprecated]] template <typename T, size_t N>
using SmallSet = StackSet<T, N>;

/// @endcond

} // namespace lagrange
