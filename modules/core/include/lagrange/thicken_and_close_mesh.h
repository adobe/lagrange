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

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
    #include <lagrange/legacy/thicken_and_close_mesh.h>
#endif

#include <lagrange/SurfaceMesh.h>

#include <optional>
#include <variant>

namespace lagrange {

///
/// Options for thicken_and_close_mesh.
///
struct ThickenAndCloseOptions
{
    /// Offset direction. Can either be an attribute (e.g. vertex normal), or a constant direction.
    /// By default it will use the first vertex attribute with usage tag `Normal`, or compute it if
    /// it does not exist.
    std::variant<std::array<double, 3>, std::string_view> direction = "";

    /// Offset amount along the specified direction.
    double offset_amount = 1.0;

    /// Optional mirroring applied to the offset vertices. The mirroring is computed wrt to the
    /// plane going through the origin along the offset direction. If this argument is set without
    /// setting a fixed offset direction, an exception is raised.
    /// * -1 means fully mirrored.
    /// * 0 means flat region.
    /// * 1 means fully translated (default behavior).
    std::optional<double> mirror_ratio;

    /// Number of segments used to split edges joining a vertex and its offset.
    size_t num_segments = 1;

    /// List of indexed attributes to preserve (e.g. UVs). Values will be duplicated for offset vertices.
    std::vector<std::string> indexed_attributes;
};

///
/// Thicken a mesh by offsetting it, and close the shape into a thick 3D solid. Input mesh vertices
/// are duplicated and can additionally be mirrored wrt to a plane going through the origin.
///
/// @param[in]  input_mesh  Input mesh, assumed to have a disk topology. Must have edge information
///                         included.
/// @param[in]  options     Thickening options.
///
/// @tparam     Scalar      Mesh scalar type.
/// @tparam     Index       Mesh index type.
///
/// @return     A mesh thickened into a 3D solid shape.
///
template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> thicken_and_close_mesh(
    SurfaceMesh<Scalar, Index> input_mesh,
    const ThickenAndCloseOptions& options = {});

} // namespace lagrange
