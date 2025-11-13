#!/usr/bin/env python

#
# Copyright 2024 Adobe. All rights reserved.
# This file is licensed to you under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License. You may obtain a copy
# of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under
# the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
# OF ANY KIND, either express or implied. See the License for the specific language
# governing permissions and limitations under the License.
#
"""Refine a mesh using subdivision surfaces."""

import argparse
import pathlib
import typing
import math
import logging

import numpy
import lagrange


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--num-levels", "-n", type=int, help="Maximum number of subdivision levels", default=4
    )
    parser.add_argument(
        "--edge-length",
        "-e",
        type=float,
        default=None,
        help="Maximum edge length to use with adaptive refinement (default: automatic based on max subdiv level).",
    )
    parser.add_argument(
        "--autosmooth-threshold",
        "-a",
        type=float,
        default=None,
        help="Normal angle threshold (in degree) for autodetecting sharp edges",
    )
    group = parser.add_mutually_exclusive_group()
    group.add_argument("--adaptive", action="store_true", dest="adaptive", default=True)
    group.add_argument("--uniform", action="store_false", dest="adaptive")
    parser.add_argument("input_mesh", type=pathlib.Path, help="Input mesh file")
    parser.add_argument(
        "output_mesh", type=pathlib.Path, default="output.obj", nargs="?", help="Output mesh file"
    )
    return parser.parse_args()


def prepare_asset(mesh: lagrange.SurfaceMesh, autosmooth: typing.Optional[float]):
    # Weld indexed attributes
    for id in mesh.get_matching_attribute_ids(lagrange.AttributeElement.Indexed):
        lagrange.weld_indexed_attribute(mesh, id, epsilon_rel=1e-3, epsilon_abs=1e-3)

    # Find an attribute to use as normals to define sharp edges
    normal_id = mesh.get_matching_attribute_id(usage=lagrange.AttributeUsage.Normal)
    if normal_id:
        lagrange.logger.info("Found normal attribute: %s", mesh.get_attribute_name(normal_id))

    # If no normals are found, compute autosmooth normals if requested
    if autosmooth is not None and normal_id:
        lagrange.logger.info(
            "Computing autosmooth normals with a threshold of %s degrees", autosmooth
        )
        normal_id = lagrange.compute_normal(mesh, math.radians(autosmooth))

    if normal_id:
        # Compute edge sharpness based on indexed normal topology
        seam_id = lagrange.compute_seam_edges(mesh, normal_id)
        edge_sharpness_id = lagrange.cast_attribute(mesh, seam_id, dtype=float)

        # Set vertex sharpness to 1 for leaf and junction vertices
        valence_id = lagrange.compute_vertex_valence(
            mesh, induced_by_attribute=mesh.get_attribute_name(seam_id)
        )
        vertex_sharpness_id = mesh.create_attribute(
            "vertex_sharpness",
            element=lagrange.AttributeElement.Vertex,
            num_channels=1,
            dtype=float,
        )
        val = mesh.attribute(valence_id).data
        mesh.attribute(vertex_sharpness_id).data[:] = numpy.select([val == 1, val > 2], [1, 1], 0)
    else:
        edge_sharpness_id = None
        vertex_sharpness_id = None

    return edge_sharpness_id, vertex_sharpness_id


def main():
    args = parse_args()
    lagrange.logger.setLevel(logging.INFO)

    # Load input mesh
    lagrange.logger.info("Loading input mesh: %s", args.input_mesh)
    if args.input_mesh.suffix in [".gltf", ".glb"]:
        lagrange.logger.warning(
            "Input mesh is a glTF file. Essential connectivity information is lost when loading from a glTF asset. We strongly advise using .fbx or .obj as input file format rather than glTF."
        )
    mesh = lagrange.io.load_mesh(args.input_mesh, stitch_vertices=True)
    lagrange.logger.info(
        "Input mesh has %s vertices and %s facets", mesh.num_vertices, mesh.num_facets
    )

    # Prepare sharpness attributes
    edge_sharpness_id, vertex_sharpness_id = prepare_asset(mesh, args.autosmooth_threshold)
    lagrange.logger.info(
        "Running subdivision with %s levels, target edge length: %s",
        args.num_levels,
        args.edge_length,
    )

    # Perform subdivision
    for id in mesh.get_matching_attribute_ids():
        if mesh.is_attribute_indexed(id):
            lagrange.logger.info(
                "Attribute %s has dtype=%s",
                mesh.get_attribute_name(id),
                mesh.indexed_attribute(id).values.dtype,
            )
        else:
            lagrange.logger.info(
                "Attribute %s has dtype=%s", mesh.get_attribute_name(id), mesh.attribute(id).dtype
            )
    mesh = lagrange.subdivision.subdivide_mesh(
        mesh,
        num_levels=args.num_levels,
        adaptive=args.adaptive,
        edge_sharpness_attr=edge_sharpness_id,
        vertex_sharpness_attr=vertex_sharpness_id,
        use_limit_surface=args.adaptive,
    )

    # Save result
    lagrange.logger.info(
        "Output mesh has %s vertices and %s facets", mesh.num_vertices, mesh.num_facets
    )
    lagrange.io.save_mesh(args.output_mesh, mesh)


if __name__ == "__main__":
    main()
