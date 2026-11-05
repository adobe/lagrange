#
# Copyright 2026 Adobe. All rights reserved.
# This file is licensed to you under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License. You may obtain a copy
# of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under
# the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
# OF ANY KIND, either express or implied. See the License for the specific language
# governing permissions and limitations under the License.
#
"""High-level workflows using the remeshing and decimation modules."""

import lagrange


def remesh_mesh(
    mesh: lagrange.SurfaceMesh,
    *,
    remesh_num_vertices: int = 100000,
    decimate_num_facets: int = 20000,
    max_lloyd_iterations: int = 16,
    max_newton_iterations: int = 64,
    skip_normalize: bool = False,
    force_lloyd_only: bool = False,
    absolute_convergence_threshold: float = 0.0,
    relative_convergence_threshold: float = 1e-3,
) -> lagrange.SurfaceMesh:
    """Remesh and decimate a copy while preserving the original coordinate system.

    Requires the optional remeshing and decimation modules. The input mesh is
    not modified. When normalization is enabled, the result is transformed back
    into the input coordinate system before it is returned.

    :param mesh: Input surface mesh.
    :param remesh_num_vertices: Target number of vertices for remeshing.
    :param decimate_num_facets: Target maximum number of facets after decimation.
    :param max_lloyd_iterations: Maximum number of Lloyd relaxation iterations.
    :param max_newton_iterations: Maximum number of feature-aware Newton iterations.
    :param skip_normalize: Keep the input scale throughout processing.
    :param force_lloyd_only: Use Lloyd remeshing without feature-aware optimization.
    :param absolute_convergence_threshold: Absolute convergence threshold.
    :param relative_convergence_threshold: Relative convergence threshold.
    :return: The remeshed and decimated surface.
    """
    mesh = mesh.clone()
    bbox_center = None
    bbox_scale = None
    if not skip_normalize:
        lagrange.logger.info("normalizing")
        bbox_min = mesh.vertices.min(axis=0)
        bbox_max = mesh.vertices.max(axis=0)
        bbox_center = (bbox_max + bbox_min) / 2
        bbox_scale = (bbox_max - bbox_min).max() / 2
        mesh.vertices = (mesh.vertices - bbox_center) / bbox_scale

    if force_lloyd_only:
        lagrange.logger.info("remesh lloyd")
        mesh = lagrange.remeshing.remesh_lloyd(
            mesh,
            target_num_vertices=remesh_num_vertices,
            max_iterations=max_lloyd_iterations,
            absolute_convergence_threshold=absolute_convergence_threshold,
            relative_convergence_threshold=relative_convergence_threshold,
        )
    else:
        lagrange.logger.info("remesh feature aware")
        mesh = lagrange.remeshing.remesh_feature_aware(
            mesh,
            target_num_vertices=remesh_num_vertices,
            max_lloyd_iterations=max_lloyd_iterations,
            max_newton_iterations=max_newton_iterations,
            absolute_convergence_threshold=absolute_convergence_threshold,
            relative_convergence_threshold=relative_convergence_threshold,
        )

    mesh.initialize_edges()
    lagrange.logger.info(
        "mesh {}v {}e {}f{}{}".format(
            mesh.num_vertices,
            mesh.num_edges,
            mesh.num_facets,
            " tri" if mesh.is_triangle_mesh else "",
            " quad" if mesh.is_quad_mesh else "",
        )
    )

    lagrange.logger.info("decimation")
    decim_options = lagrange.decimation.DecimationOptions()
    decim_options.max_facets = decimate_num_facets
    mesh = lagrange.decimation.decimate_quadric(mesh, decim_options)

    if bbox_center is not None and bbox_scale is not None:
        lagrange.logger.info("unnormalizing")
        mesh.vertices = (mesh.vertices * bbox_scale) + bbox_center

    mesh.initialize_edges()
    lagrange.logger.info(
        "mesh {}v {}e {}f{}{}".format(
            mesh.num_vertices,
            mesh.num_edges,
            mesh.num_facets,
            " tri" if mesh.is_triangle_mesh else "",
            " quad" if mesh.is_quad_mesh else "",
        )
    )

    return mesh
