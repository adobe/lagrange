/*
 * Copyright 2026 Adobe. All rights reserved.
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
#include <lagrange/utils/warning.h>

#include <Eigen/Core>

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <variant>
#include <vector>

namespace lagrange {
namespace raycasting {

/// Main projection mode
enum class ProjectMode {
    CLOSEST_VERTEX [[deprecated]] = 0, ///< @deprecated Use ClosestVertex instead.
    CLOSEST_POINT [[deprecated]] = 1, ///< @deprecated Use ClosestPoint instead.
    RAY_CASTING [[deprecated]] = 2, ///< @deprecated Use RayCasting instead.
    ClosestVertex = 0, ///< Copy attribute from the closest vertex on the source mesh.
    ClosestPoint = 1, ///< Interpolate attribute from the closest point on the source mesh.
    RayCasting =
        2, ///< Copy attribute by projecting along a prescribed direction on the source mesh.
};

/// Ray-casting mode
enum class CastMode {
    ONE_WAY [[deprecated]] = 0, ///< @deprecated Use OneWay instead.
    BOTH_WAYS [[deprecated]] = 1, ///< @deprecated Use BothWays instead.
    OneWay = 0, ///< Cast a ray forward in the prescribed direction.
    BothWays = 1, ///< Cast a ray both forward and backward in the prescribed direction.
};

/// Fallback mode for vertices without a hit
enum class FallbackMode {
    CONSTANT [[deprecated]] = 0, ///< @deprecated Use Constant instead.
    CLOSEST_VERTEX [[deprecated]] = 1, ///< @deprecated Use ClosestVertex instead.
    CLOSEST_POINT [[deprecated]] = 2, ///< @deprecated Use ClosestPoint instead.
    Constant = 0, ///< Fill with a constant value (defaults to 0).
    ClosestVertex = 1, ///< Copy attribute from the closest vertex on the source mesh.
    ClosestPoint = 2, ///< Interpolate attribute from the closest point on the source mesh.
};

using WrapMode [[deprecated]] = FallbackMode; ///< @deprecated Use FallbackMode instead.

inline const std::map<std::string, ProjectMode>& project_modes()
{
    LA_IGNORE_DEPRECATION_WARNING_BEGIN
    static std::map<std::string, ProjectMode> _modes = {
        {"CLOSEST_VERTEX", ProjectMode::CLOSEST_VERTEX},
        {"CLOSEST_POINT", ProjectMode::CLOSEST_POINT},
        {"RAY_CASTING", ProjectMode::RAY_CASTING},
        {"ClosestVertex", ProjectMode::ClosestVertex},
        {"ClosestPoint", ProjectMode::ClosestPoint},
        {"RayCasting", ProjectMode::RayCasting},
    };
    LA_IGNORE_DEPRECATION_WARNING_END
    return _modes;
}

inline const std::map<std::string, CastMode>& cast_modes()
{
    LA_IGNORE_DEPRECATION_WARNING_BEGIN
    static std::map<std::string, CastMode> _modes = {
        {"ONE_WAY", CastMode::ONE_WAY},
        {"BOTH_WAYS", CastMode::BOTH_WAYS},
        {"OneWay", CastMode::OneWay},
        {"BothWays", CastMode::BothWays},
    };
    LA_IGNORE_DEPRECATION_WARNING_END
    return _modes;
}

inline const std::map<std::string, FallbackMode>& fallback_modes()
{
    LA_IGNORE_DEPRECATION_WARNING_BEGIN
    static std::map<std::string, FallbackMode> _modes = {
        {"CONSTANT", FallbackMode::CONSTANT},
        {"CLOSEST_VERTEX", FallbackMode::CLOSEST_VERTEX},
        {"CLOSEST_POINT", FallbackMode::CLOSEST_POINT},
        {"Constant", FallbackMode::Constant},
        {"ClosestVertex", FallbackMode::ClosestVertex},
        {"ClosestPoint", FallbackMode::ClosestPoint},
    };
    LA_IGNORE_DEPRECATION_WARNING_END
    return _modes;
}

[[deprecated]] inline const std::map<std::string, FallbackMode>& wrap_modes()
{
    return fallback_modes();
}

///
/// @addtogroup group-raycasting
/// @{
///

///
/// Common options for projection functions.
///
struct ProjectCommonOptions
{
    /// Additional vertex attribute ids to project (beyond vertex positions).
    std::vector<AttributeId> attribute_ids;

    /// Whether to project vertex positions. Defaults to true. When true, the vertex position
    /// attribute (SurfaceMesh::attr_id_vertex_to_position) is automatically appended to the list
    /// of projected attributes.
    bool project_vertices = true;

    /// If provided, whether to skip assignment for a target vertex or not. This can be used for
    /// partial assignment (e.g., to only set boundary vertices of a mesh).
    std::function<bool(uint64_t)> skip_vertex = nullptr;
};

///
/// Options for project_directional().
///
struct ProjectDirectionalOptions : public ProjectCommonOptions
{
    /// Raycasting direction to project attributes.
    ///
    /// This can be one of three types:
    /// - `std::monostate` (default): Use per-vertex normals from the *target* mesh. If no vertex
    ///    normal attribute exists on the target mesh, one is automatically computed via
    ///    `compute_vertex_normal`.
    /// - `Eigen::Vector3f`: A uniform direction for all rays (will be normalized internally).
    /// - `AttributeId`: An attribute id referencing a per-vertex Normal attribute on the
    ///    *target* mesh, providing a per-vertex ray direction.
    std::variant<std::monostate, Eigen::Vector3f, AttributeId> direction = {};

    /// Whether to project forward along the ray, or both forward and backward.
    CastMode cast_mode = CastMode::BothWays;

    /// Fallback mode for vertices where there is no hit.
    FallbackMode fallback_mode = FallbackMode::Constant;

    /// Scalar used to fill attributes in Constant fallback mode.
    double default_value = 0.0;

    /// Optional callback invoked for each target vertex with (vertex index, was_hit). Called
    /// regardless of hit/miss. This is useful to determine if the directional raycast was a hit or
    /// if the fallback method was used.
    std::function<void(uint64_t, bool)> user_callback = nullptr;
};

///
/// Options for project().
///
struct ProjectOptions : ProjectDirectionalOptions
{
    /// Projection mode to choose from.
    ProjectMode project_mode = ProjectMode::ClosestPoint;
};

/// @}

} // namespace raycasting
} // namespace lagrange
