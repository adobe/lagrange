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

#include <lagrange/SurfaceMesh.h>

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
    #include <lagrange/legacy/orient_outward.h>
#endif

namespace lagrange {

///
/// Options for orienting the facets of a mesh.
///
struct OrientOptions
{
    /// Orient to have positive signed volume.
    bool positive = true;
};

///
/// Orient the facets of a mesh so that the signed volume of each connected component is positive or
/// negative.
///
/// @param[in,out] mesh     Mesh whose facets needs to be re-oriented.
/// @param[in]     options  Orientation options.
///
/// @tparam        Scalar   Mesh scalar type.
/// @tparam        Index    Mesh index type.
///
template <typename Scalar, typename Index>
void orient_outward(lagrange::SurfaceMesh<Scalar, Index>& mesh, const OrientOptions& options = {});

} // namespace lagrange
