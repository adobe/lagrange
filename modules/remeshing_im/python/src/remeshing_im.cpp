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

#include <lagrange/python/binding.h>
#include <lagrange/python/remeshing_im.h>
#include <lagrange/remeshing_im/remesh.h>

namespace nb = nanobind;
using namespace nb::literals;

namespace lagrange::python {

void populate_remeshing_im_module(nb::module_& m)
{
    using Scalar = double;
    using Index = uint32_t;
    using MeshType = SurfaceMesh<Scalar, Index>;

    const remeshing_im::RemeshingOptions defaults;

    m.def(
        "remesh",
        [](MeshType& mesh,
           size_t target_num_facets,
           bool deterministic,
           size_t knn_points,
           uint8_t posy,
           uint8_t rosy,
           float crease_angle,
           bool align_to_boundaries,
           bool extrinsic,
           size_t num_smooth_iter) {
            remeshing_im::RemeshingOptions options;
            options.target_num_facets = target_num_facets;
            options.deterministic = deterministic;
            options.knn_points = knn_points;
            options.posy = static_cast<remeshing_im::PosyType>(posy);
            options.rosy = static_cast<remeshing_im::RosyType>(rosy);
            options.crease_angle = crease_angle;
            options.align_to_boundaries = align_to_boundaries;
            options.extrinsic = extrinsic;
            options.num_smooth_iter = num_smooth_iter;
            return remeshing_im::remesh(mesh, options);
        },
        "mesh"_a,
        "target_num_facets"_a = defaults.target_num_facets,
        "deterministic"_a = defaults.deterministic,
        "knn_points"_a = defaults.knn_points,
        "posy"_a = static_cast<uint8_t>(defaults.posy),
        "rosy"_a = static_cast<uint8_t>(defaults.rosy),
        "crease_angle"_a = defaults.crease_angle,
        "align_to_boundaries"_a = defaults.align_to_boundaries,
        "extrinsic"_a = defaults.extrinsic,
        "num_smooth_iter"_a = defaults.num_smooth_iter,
        R"(Remesh a surface mesh using Instant Meshes.

:param mesh: Input mesh to remesh (triangle mesh or point cloud).
:param target_num_facets: Target number of facets in the output mesh.
:param deterministic: Whether to make the process deterministic.
:param knn_points: Number of nearest neighbors when remeshing point clouds.
:param posy: Positional symmetry type (3 for triangle, 4 for quad).
:param rosy: Rotational symmetry type (2 for line field, 4 for cross field, 6 for hex field).
:param crease_angle: Crease angle in degrees for detecting sharp edges.
:param align_to_boundaries: Whether to align output to input boundaries.
:param extrinsic: Whether to use extrinsic metrics for cross-field alignment.
:param num_smooth_iter: Number of smoothing iterations.

:return: The remeshed surface mesh.)");
}

} // namespace lagrange::python
