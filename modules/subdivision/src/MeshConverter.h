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

#include <lagrange/subdivision/mesh_subdivision.h>

namespace lagrange::subdivision {

// Proxy type to forward both mesh + user options to TopologyRefinerFactory
template <typename SurfaceMeshT>
struct MeshConverter
{
    using SurfaceMeshType = SurfaceMeshT;

    const SurfaceMeshType& mesh;
    const SubdivisionOptions& options;
    const std::vector<AttributeId>& face_varying_attributes;
};

struct InterpolatedAttributeIds
{
    std::vector<AttributeId> smooth_vertex_attributes;
    std::vector<AttributeId> linear_vertex_attributes;
    std::vector<AttributeId> face_varying_attributes;
};

} // namespace lagrange::subdivision
