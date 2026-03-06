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

#include <lagrange/raycasting/Options.h>

namespace lagrange::raycasting {

template <typename Scalar, typename Index>
std::vector<AttributeId> prepare_attribute_ids(
    const SurfaceMesh<Scalar, Index>& source,
    const ProjectCommonOptions& options)
{
    // Build the full list of attribute ids to project.
    std::vector<AttributeId> attribute_ids = options.attribute_ids;
    if (options.project_vertices) {
        attribute_ids.push_back(source.attr_id_vertex_to_position());
    }
    // Remove duplicates, if any
    std::sort(attribute_ids.begin(), attribute_ids.end());
    attribute_ids.erase(
        std::unique(attribute_ids.begin(), attribute_ids.end()),
        attribute_ids.end());
    return attribute_ids;
}

} // namespace lagrange::raycasting
