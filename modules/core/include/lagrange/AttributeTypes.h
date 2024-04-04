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
///
/// @file
///

#pragma once

#include <lagrange/api.h>

#include <cstdint>

// clang-format off

/// @addtogroup group-surfacemesh-attr
/// @{

///
/// [X Macro](https://en.wikipedia.org/wiki/X_Macro) arguments for the Attribute<> class.
/// Since other modules might need to explicitly instantiate their own functions, this file is a
/// public header.
///
/// Use in a .cpp as follows:
///
/// @code
/// #include <lagrange/AttributeTypes.h>
/// #define LA_X_foo(_, ValueType) template void my_function(const Attribute<ValueType> &);
/// LA_ATTRIBUTE_X(foo, 0)
/// @endcode
///
/// The optional `data` argument can forwarded to other macros, in order to implement cartesian
/// products when instantiating nested types.
///
/// @param      mode  Suffix to apply to the LA_X_* macro.
/// @param      data  Data to be passed around as the first argument of the X macro.
///
/// @return     Expansion of LA_X_##mode() for each type to be instantiated.
///
#define LA_ATTRIBUTE_X(mode, data) \
    LA_X_##mode(data, int8_t) \
    LA_X_##mode(data, int16_t) \
    LA_X_##mode(data, int32_t) \
    LA_X_##mode(data, int64_t) \
    LA_X_##mode(data, uint8_t) \
    LA_X_##mode(data, uint16_t) \
    LA_X_##mode(data, uint32_t) \
    LA_X_##mode(data, uint64_t) \
    LA_X_##mode(data, float) \
    LA_X_##mode(data, double)

///
/// [X Macro](https://en.wikipedia.org/wiki/X_Macro) arguments for the Attribute<> class.
/// Usage is similar to LA_ATTRIBUTE_X, but it will only iterate over integral value types.
///
/// Use in a .cpp as follows:
///
/// @code
/// #include <lagrange/AttributeTypes.h>
/// #define LA_X_foo(_, IndexType) template void my_function(const Attribute<IndexType> &);
/// LA_ATTRIBUTE_INDEX_X(foo, 0)
/// @endcode
///
/// The optional `data` argument can forwarded to other macros, in order to implement cartesian
/// products when instantiating nested types.
///
/// @param      mode  Suffix to apply to the LA_X_* macro.
/// @param      data  Data to be passed around as the first argument of the X macro.
///
/// @return     Expansion of LA_X_##mode() for each type to be instantiated.
///
#define LA_ATTRIBUTE_INDEX_X(mode, data) \
    LA_X_##mode(data, int8_t) \
    LA_X_##mode(data, int16_t) \
    LA_X_##mode(data, int32_t) \
    LA_X_##mode(data, int64_t) \
    LA_X_##mode(data, uint8_t) \
    LA_X_##mode(data, uint16_t) \
    LA_X_##mode(data, uint32_t) \
    LA_X_##mode(data, uint64_t)

///
/// [X Macro](https://en.wikipedia.org/wiki/X_Macro) arguments for the Attribute<> class.
/// Usage is similar to LA_ATTRIBUTE_X, but it will only iterate over floating points value types.
///
/// Use in a .cpp as follows:
///
/// @code
/// #include <lagrange/AttributeTypes.h>
/// #define LA_X_foo(_, IndexType) template void my_function(const Attribute<IndexType> &);
/// LA_ATTRIBUTE_SCALAR_X(foo, 0)
/// @endcode
///
/// The optional `data` argument can forwarded to other macros, in order to implement cartesian
/// products when instantiating nested types.
///
/// @param      mode  Suffix to apply to the LA_X_* macro.
/// @param      data  Data to be passed around as the first argument of the X macro.
///
/// @return     Expansion of LA_X_##mode() for each type to be instantiated.
///
#define LA_ATTRIBUTE_SCALAR_X(mode, data) \
    LA_X_##mode(data, float) \
    LA_X_##mode(data, double)

/// @}

// clang-format on
