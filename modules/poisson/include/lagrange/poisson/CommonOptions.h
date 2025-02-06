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

#include <lagrange/SurfaceMesh.h>

#include <string_view>

namespace lagrange::poisson {

///
/// Shared options for Poisson surface reconstruction.
///
struct CommonOptions
{
    /// Maximum octree depth. (If the value is zero then the minimum of 8 and log base 4 of the point count is used.)
    unsigned int octree_depth = 0;

    /// Numer of samples per node
    float samples_per_node = 1.5f;

    /// Output logging information (directly printed to std::cout)
    bool verbose = false;
};

} // namespace lagrange::poisson
