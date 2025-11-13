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
    #include <lagrange/legacy/compute_mesh_covariance.h>
#endif

#include <lagrange/SurfaceMesh.h>
#include <optional>

namespace lagrange {

///
/// Options struct for computing mesh covariance.
///
struct MeshCovarianceOptions
{
    /// The center around which the covariance is computed.
    std::array<double, 3> center = {0, 0, 0};
    /// The attribute name for the active facets in covariance computation.
    /// If empty, all facets are active.
    std::optional<std::string_view> active_facets_attribute_name;
};

/**
 * Compute the covariance matrix w.r.t. a given center (default to zeros).
 *
 * @param[in]  mesh     The input mesh.
 * @param[in]  options  Optional arguments.
 *
 * @tparam     Scalar   The scalar type of the mesh.
 * @tparam     Index    The index type of the mesh.
 *
 * @return     The covariance matrix in column-major order (but it should be symmetric).
 *
 * @see        @ref MeshCovarianceOptions.
 *
 * @note Adapted from https://github.com/mkazhdan/ShapeSPH/blob/master/Util/TriangleMesh.h#L101
 */
template <typename Scalar, typename Index>
std::array<std::array<Scalar, 3>, 3> compute_mesh_covariance(
    const SurfaceMesh<Scalar, Index>& mesh,
    const MeshCovarianceOptions& options = {});

} // namespace lagrange
