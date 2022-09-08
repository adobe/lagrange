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

#include <map>
#include <string>

namespace lagrange {
namespace raycasting {

/// Main projection mode
enum class ProjectMode {
    CLOSEST_VERTEX, ///< Copy attribute from the closest vertex on the source mesh.
    CLOSEST_POINT, ///< Interpolate attribute from the closest point on the source mesh.
    RAY_CASTING, ///< Copy attribute by projecting along a prescribed direction on the source mesh.
};

/// Ray-casting mode
enum class CastMode {
    ONE_WAY, ///< Cast a ray forward in the prescribed direction.
    BOTH_WAYS, ///< Cast a ray both forward and backward in the prescribed direction.
};

/// Wraping mode for vertices without a hit
enum class WrapMode {
    CONSTANT, ///< Fill with a constant value (defaults to 0).
    CLOSEST_VERTEX, ///< Copy attribute from the closest vertex on the source mesh.
    CLOSEST_POINT, ///< Interpolate attribute from the closest point on the source mesh.
};

inline const std::map<std::string, ProjectMode>& project_modes()
{
    static std::map<std::string, ProjectMode> _modes = {
        {"CLOSEST_VERTEX", ProjectMode::CLOSEST_VERTEX},
        {"CLOSEST_POINT", ProjectMode::CLOSEST_POINT},
        {"RAY_CASTING", ProjectMode::RAY_CASTING},
    };
    return _modes;
}

inline const std::map<std::string, CastMode>& cast_modes()
{
    static std::map<std::string, CastMode> _modes = {
        {"ONE_WAY", CastMode::ONE_WAY},
        {"BOTH_WAYS", CastMode::BOTH_WAYS},
    };
    return _modes;
}

inline const std::map<std::string, WrapMode>& wrap_modes()
{
    static std::map<std::string, WrapMode> _modes = {
        {"CONSTANT", WrapMode::CONSTANT},
        {"CLOSEST_VERTEX", WrapMode::CLOSEST_VERTEX},
        {"CLOSEST_POINT", WrapMode::CLOSEST_POINT},
    };
    return _modes;
}

} // namespace raycasting
} // namespace lagrange
