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
import argparse
import lagrange
from pathlib import Path


def parse_args():
    parser = argparse.ArgumentParser(description="Convert a mesh file to a different format.")
    parser.add_argument("input_mesh", help="input mesh file")
    parser.add_argument("output_mesh", help="output mesh file")
    parser.add_argument("--triangulate", "-t", action="store_true", help="triangulate the mesh")
    parser.add_argument("--stitch-vertices", '-s', action="store_true", help="stitch vertices")
    parser.add_argument(
        "--logging-level",
        "-l",
        default="info",
        help="logging level",
        choices=["debug", "info", "warning", "error", "critical"],
    )
    return parser.parse_args()


def main():
    args = parse_args()
    lagrange.logger.setLevel(args.logging_level.upper())
    input_suffix = Path(args.input_mesh).suffix
    output_suffix = Path(args.output_mesh).suffix
    if input_suffix in [".fbx", ".gltf", ".glb"]:
        lagrange.logger.info(f"Loading input scene from {args.input_mesh}")
        scene = lagrange.io.load_simple_scene(args.input_mesh)
        if input_suffix in [".gltf", ".glb"]:
            for i in range(scene.num_meshes):
                mesh = scene.ref_mesh(i)
                lagrange.remove_duplicate_vertices(mesh)

        if args.triangulate:
            for i in range(scene.num_meshes):
                mesh = scene.ref_mesh(i)
                lagrange.triangulate_polygonal_facets(mesh)

        if output_suffix in [".gltf", ".glb"]:
            lagrange.logger.info(f"Saving output scene in {args.output_mesh}")
            lagrange.io.save_simple_scene(args.output_mesh, scene)
        else:
            lagrange.logger.info(f"Saving output mesh in {args.output_mesh}")
            mesh = lagrange.scene.simple_scene_to_mesh(scene)
            lagrange.io.save_mesh(args.output_mesh, mesh)
    else:
        lagrange.logger.info(f"Loading input mesh from {args.input_mesh}")
        mesh = lagrange.io.load_mesh(args.input_mesh, stitch_vertices=args.stitch_vertices)
        if not mesh.is_regular and args.triangulate:
            lagrange.triangulate_polygonal_facets(mesh)
        lagrange.logger.info(f"Saving output mesh in {args.output_mesh}")
        lagrange.io.save_mesh(args.output_mesh, mesh)


if __name__ == "__main__":
    main()
