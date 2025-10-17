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

#include <lagrange/ui/Entity.h>
#include <lagrange/ui/api.h>
#include <lagrange/ui/components/Light.h>
#include <lagrange/ui/types/Color.h>


namespace lagrange {
namespace ui {

inline Eigen::Vector3f get_canonical_light_direction()
{
    return Eigen::Vector3f(0, 0, 1);
}


LA_UI_API Entity add_point_light(
    Registry& r,
    Eigen::Vector3f intensity = Eigen::Vector3f::Ones(),
    Eigen::Vector3f position = Eigen::Vector3f::Zero());

LA_UI_API Entity add_directional_light(
    Registry& r,
    Eigen::Vector3f intensity = Eigen::Vector3f::Ones(),
    Eigen::Vector3f direction = -Eigen::Vector3f::UnitY());

LA_UI_API Entity add_spot_light(
    Registry& r,
    Eigen::Vector3f intensity = Eigen::Vector3f::Ones(),
    Eigen::Vector3f position = Eigen::Vector3f::Ones(),
    Eigen::Vector3f direction = -Eigen::Vector3f::Ones(),
    float cone_angle = pi() / 4.0f);


LA_UI_API std::pair<Eigen::Vector3f, Eigen::Vector3f> get_light_position_and_direction(
    const Registry& r,
    Entity e);


LA_UI_API void clear_lights(Registry& r);

} // namespace ui
} // namespace lagrange
