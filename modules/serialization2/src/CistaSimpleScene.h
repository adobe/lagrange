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

#include "CistaMesh.h"

#include <cista/containers/vector.h>

#include <cstdint>

namespace lagrange::serialization::internal {

namespace data = cista::offset;

/// Cista-compatible representation of a single MeshInstance.
struct CistaInstance
{
    uint64_t mesh_index = 0;

    /// Raw bytes of the Eigen AffineTransform matrix.
    /// Size = (Dimension+1)^2 * sizeof(Scalar).
    data::vector<uint8_t> transform_bytes;

    // MeshInstance::user_data (std::any) is NOT serialized.
};

/// Cista-compatible representation of SimpleScene.
struct CistaSimpleScene
{
    uint32_t version = 1;

    uint8_t scalar_type_size = 0; // sizeof(Scalar): 4 for float, 8 for double
    uint8_t index_type_size = 0; // sizeof(Index): 4 for uint32_t, 8 for uint64_t
    uint8_t dimension = 0; // 2 or 3

    data::vector<CistaMesh> meshes;

    /// Number of instances per mesh (used to reconstruct the nested vector structure).
    data::vector<uint64_t> instances_per_mesh;

    /// Flattened list of all instances across all meshes.
    data::vector<CistaInstance> instances;
};

} // namespace lagrange::serialization::internal
