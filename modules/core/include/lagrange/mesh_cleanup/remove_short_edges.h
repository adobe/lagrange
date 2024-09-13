/*
 * Copyright 2019 Adobe. All rights reserved.
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
    #include <lagrange/mesh_cleanup/legacy/remove_short_edges.h>
#endif

#include <lagrange/SurfaceMesh.h>

namespace lagrange {

///
/// Collapse all edges shorter than a given tolerance.
///
/// @param mesh       Input mesh to be updated in place.
/// @param threshold  Edges with length <= threshold will be removed.
///
template <typename Scalar, typename Index>
void remove_short_edges(SurfaceMesh<Scalar, Index>& mesh, Scalar threshold = 0);

} // namespace lagrange
