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
#include <lagrange/internal/string_from_scalar.h>

#include <lagrange/AttributeTypes.h>

#include <string_view>

namespace lagrange::internal {

#define LA_X_string_from_scalar(_, ValueType)        \
    template <>                                      \
    std::string_view string_from_scalar<ValueType>() \
    {                                                \
        return #ValueType;                           \
    }
LA_ATTRIBUTE_X(string_from_scalar, 0)

} // namespace lagrange::internal
