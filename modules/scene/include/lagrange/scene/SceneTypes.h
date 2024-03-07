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
#pragma once

// clang-format off

///
/// [X Macro](https://en.wikipedia.org/wiki/X_Macro) arguments for the Scene<> class.
///
/// Since other modules might need to explicitly instantiate their own functions, this file is a
/// public header.
///
/// Use in a .cpp as follows:
///
/// @code
/// #include <lagrange/scene/SceneTypes.h>
/// #define LA_X_foo(_, Scalar, Index) template void my_function(const Scene<Scalar, Index> &);
/// LA_SCENE_X(foo, 0)
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
#define LA_SCENE_X(mode, data) \
    LA_X_##mode(data, float, uint32_t) \
    LA_X_##mode(data, double, uint32_t) \
    LA_X_##mode(data, float, uint64_t) \
    LA_X_##mode(data, double, uint64_t)

// clang-format on
