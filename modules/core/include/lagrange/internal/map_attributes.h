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

#include <lagrange/AttributeFwd.h>
#include <lagrange/SurfaceMesh.h>
#include <lagrange/types/MappingPolicy.h>
#include <lagrange/utils/span.h>

namespace lagrange::internal {

/// Attribute mapping options.
struct MapAttributesOptions
{
    /// Collision policy for float or double valued attributes
    MappingPolicy collision_policy_float = MappingPolicy::Average;

    /// Collision policy for integral valued attributes
    MappingPolicy collision_policy_integral = MappingPolicy::KeepFirst;

    /// Selected attribute ids. If empty, treat all attributes as selected.
    span<AttributeId> selected_attributes = {};
};


///
/// Map attributes from the source mesh to the target mesh.
///
/// This is the most general version that supports many-to-many mapping. Collision policy settings
/// in `options` define the behavior when multiple source vertices are mapped to the same target
/// vertex.
///
/// @tparam Scalar              The scalar type.
/// @tparam Index               The index type.
///
/// @param source_mesh          The source mesh.
/// @param target_mesh          The target mesh.
/// @param mapping_data         A flat array of source element indices.
/// @param mapping_offsets      The offset index array into the `mapping_data`. Source element with
///                             index listed from `mapping_data[mapping_offset[i]]` to
///                             `mapping_data[mapping_offset[i+1]]` are mapped to target element `i`.
///                             If empty, source element with index `mapping_data[i]` is mapped to
///                             target element `i`.
/// @param options              Option settings.
///
/// @see @ref MapAttributesOptions
///
template <AttributeElement element, typename Scalar, typename Index>
void map_attributes(
    const SurfaceMesh<Scalar, Index>& source_mesh,
    SurfaceMesh<Scalar, Index>& target_mesh,
    span<const Index> mapping_data,
    span<const Index> mapping_offsets = {},
    const MapAttributesOptions& options = {});

} // namespace lagrange::internal
