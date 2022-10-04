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

#include <string_view>

namespace lagrange::internal {

///
/// Returns a human-readable string from any supported attribute value type.
///
/// @tparam     Scalar  Can be any supported attribute value type.
///
/// @return     A human-readable string view of the type name.
///
template <typename Scalar>
std::string_view string_from_scalar();

} // namespace lagrange::internal
