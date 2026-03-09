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

#include <lagrange/SurfaceMesh.h>
#include <lagrange/bvh/api.h>

#include <string>

namespace lagrange::bvh {

/// @addtogroup module-bvh
/// @{

///
/// Options for compute_mesh_distances.
///
struct MeshDistancesOptions
{
    /// Name of the output per-vertex scalar attribute written to the source mesh.
    std::string output_attribute_name = "@distance_to_mesh";
};

///
/// Compute the distance from each vertex in @p source to the closest point on @p target,
/// and store the result as a per-vertex scalar attribute on @p source.
///
/// Both meshes must have the same spatial dimension. @p target must be a triangle mesh.
///
/// @param[in,out] source   Mesh whose vertices are queried. The output attribute is added here.
/// @param[in]     target   Triangle mesh against which distances are computed.
/// @param[in]     options  Options controlling the name of the output attribute.
///
/// @return  AttributeId of the newly created (or overwritten) distance attribute on @p source.
///
/// @tparam Scalar  Mesh scalar type.
/// @tparam Index   Mesh index type.
///
template <typename Scalar, typename Index>
LA_BVH_API AttributeId compute_mesh_distances(
    SurfaceMesh<Scalar, Index>& source,
    const SurfaceMesh<Scalar, Index>& target,
    const MeshDistancesOptions& options = {});

///
/// Compute the symmetric Hausdorff distance between @p source and @p target.
///
/// The Hausdorff distance is the maximum of the two directed Hausdorff distances:
/// @f[
///   H(A, B) = \max\!\left(
///     \max_{a \in A} \mathrm{dist}(a, B),\;
///     \max_{b \in B} \mathrm{dist}(b, A)
///   \right)
/// @f]
/// where @f$ \mathrm{dist}(v, M) @f$ is the distance from vertex @f$ v @f$ to the closest
/// point on mesh @f$ M @f$.
///
/// Both meshes must have the same spatial dimension and must be triangle meshes.
///
/// @param[in] source  First mesh.
/// @param[in] target  Second mesh.
///
/// @return Hausdorff distance.
///
/// @tparam Scalar  Mesh scalar type.
/// @tparam Index   Mesh index type.
///
template <typename Scalar, typename Index>
LA_BVH_API Scalar compute_hausdorff(
    const SurfaceMesh<Scalar, Index>& source,
    const SurfaceMesh<Scalar, Index>& target);

///
/// Compute the Chamfer distance between @p source and @p target.
///
/// The Chamfer distance is defined as:
/// @f[
///   C(A, B) = \frac{1}{|A|} \sum_{a \in A} \mathrm{dist}(a, B)^2
///           + \frac{1}{|B|} \sum_{b \in B} \mathrm{dist}(b, A)^2
/// @f]
/// where @f$ \mathrm{dist}(v, M) @f$ is the distance from vertex @f$ v @f$ to the closest
/// point on mesh @f$ M @f$.
///
/// Both meshes must have the same spatial dimension and must be triangle meshes.
///
/// @param[in] source  First mesh.
/// @param[in] target  Second mesh.
///
/// @return Chamfer distance.
///
/// @tparam Scalar  Mesh scalar type.
/// @tparam Index   Mesh index type.
///
template <typename Scalar, typename Index>
LA_BVH_API Scalar
compute_chamfer(const SurfaceMesh<Scalar, Index>& source, const SurfaceMesh<Scalar, Index>& target);

/// @}

} // namespace lagrange::bvh
