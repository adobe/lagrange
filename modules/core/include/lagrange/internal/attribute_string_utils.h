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

#include <lagrange/AttributeFwd.h>

#include <string_view>

namespace lagrange::internal {

///
/// Returns a string representation of an attribute element type.
///
/// @param[in]  element  Attribute element type.
///
/// @return     String representation.
///
std::string_view to_string(AttributeElement element);

///
/// Returns a string representation of an attribute usage.
///
/// @param[in]  usage  Attribute usage.
///
/// @return     String representation.
///
std::string_view to_string(AttributeUsage usage);

} // namespace lagrange::internal
