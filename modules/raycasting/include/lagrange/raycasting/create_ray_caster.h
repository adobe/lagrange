/*
 * Copyright 2017 Adobe. All rights reserved.
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

#include <lagrange/Mesh.h>
#include <lagrange/raycasting/EmbreeRayCaster.h>

#include <sstream>

namespace lagrange {
namespace raycasting {

enum RayCasterType {
    EMBREE_DEFAULT = 1, ///< Corresponds to RTC_SCENE_FLAG_NONE
    EMBREE_DYNAMIC = 2, ///< Corresponds to RTC_SCENE_FLAG_DYNAMIC
    EMBREE_ROBUST = 4, ///< Corresponds to RTC_SCENE_FLAG_ROBUST
    EMBREE_COMPACT = 8, ///< Corresponds to RTC_SCENE_FLAG_COMPACT
};

enum RayCasterQuality {
    BUILD_QUALITY_LOW, ///< Corresponds to RTC_BUILD_QUALITY_LOW
    BUILD_QUALITY_MEDIUM, ///< Corresponds to RTC_BUILD_QUALITY_MEDIUM
    BUILD_QUALITY_HIGH, ///< Corresponds to RTC_BUILD_QUALITY_HIGH
};

template <typename Scalar>
std::unique_ptr<EmbreeRayCaster<Scalar>> create_ray_caster(
    RayCasterType engine,
    RayCasterQuality quality = BUILD_QUALITY_LOW)
{
    if (engine & 0b1111) {
        // Translate scene flags for embree ray casters
        int flags = static_cast<int>(RTC_SCENE_FLAG_NONE);
        if (engine & EMBREE_DYNAMIC) {
            flags |= static_cast<int>(RTC_SCENE_FLAG_DYNAMIC);
        }
        if (engine & EMBREE_ROBUST) {
            flags |= static_cast<int>(RTC_SCENE_FLAG_ROBUST);
        }
        if (engine & EMBREE_COMPACT) {
            flags |= static_cast<int>(RTC_SCENE_FLAG_COMPACT);
        }

        // Translate build quality settings
        RTCBuildQuality build = RTC_BUILD_QUALITY_LOW;
        switch (quality) {
        case BUILD_QUALITY_LOW: build = RTC_BUILD_QUALITY_LOW; break;
        case BUILD_QUALITY_MEDIUM: build = RTC_BUILD_QUALITY_MEDIUM; break;
        case BUILD_QUALITY_HIGH: build = RTC_BUILD_QUALITY_HIGH; break;
        default: break;
        }

        return std::make_unique<EmbreeRayCaster<Scalar>>(static_cast<RTCSceneFlags>(flags), build);
    } else {
        std::stringstream err_msg;
        err_msg << "Unknown ray caster engine: " << engine;
        throw std::runtime_error(err_msg.str());
    }
}
} // namespace raycasting
} // namespace lagrange
