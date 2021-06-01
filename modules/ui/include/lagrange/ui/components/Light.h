/*
 * Copyright 2020 Adobe. All rights reserved.
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

#include <lagrange/ui/utils/math.h>

namespace lagrange {
namespace ui {

/// @brief Light component
/// By default, light points in the +Z direction, unless transformed
struct LightComponent
{
    enum class Type { POINT, DIRECTIONAL, SPOT } type = Type::DIRECTIONAL;

    bool casts_shadow = true;

    // Cone
    float cone_angle = pi() / 4.0f;

    Eigen::Vector3f intensity = Eigen::Vector3f(30.0f, 30.0f, 30.0f);

    
    
};


} // namespace ui
} // namespace lagrange