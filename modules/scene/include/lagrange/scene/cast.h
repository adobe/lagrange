/*
 * Copyright 2024 Adobe. All rights reserved.
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

#include <lagrange/cast.h>
#include <lagrange/scene/SimpleScene.h>

namespace lagrange::scene {

///
/// Cast a scene to a scene of different scalar and/or index type.
///
/// @param[in]  source_scene                Input scene.
/// @param[in]  convertible_attributes      Filter to determine which attribute are convertible.
/// @param[out] converted_attributes_names  Optional output arg storing the list of non-reserved
///                                         attribute names that were actually converted to a
///                                         different type.
///
/// @tparam     ToScalar                    Scalar type of the output scene.
/// @tparam     ToIndex                     Index type of the output scene.
/// @tparam     FromScalar                  Scalar type of the input scene.
/// @tparam     FromIndex                   Index type of the input scene.
/// @tparam     Dimension                   Scene dimension.
///
/// @return     Output scene.
///
template <
    typename ToScalar,
    typename ToIndex,
    typename FromScalar,
    typename FromIndex,
    size_t Dimension>
SimpleScene<ToScalar, ToIndex, Dimension> cast(
    const scene::SimpleScene<FromScalar, FromIndex, Dimension>& source_scene,
    const AttributeFilter& convertible_attributes = {},
    std::vector<std::string>* converted_attributes_names = nullptr);

} // namespace lagrange::scene
