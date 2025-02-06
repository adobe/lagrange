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
"""Poisson surface reconstruction from oriented point cloud."""

import argparse
import lagrange
import numpy as np


def parse_args():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("input_point_cloud", help="Input point cloud file.")
    parser.add_argument("output_mesh", help="Output mesh file.")
    parser.add_argument(
        "--depth",
        type=int,
        default=0,
        help="Max depth of the octree. If 0, the depth will be computed from the point cloud size",
    )
    parser.add_argument("--verbose", action="store_true", help="Verbose output.")
    return parser.parse_args()


def main():
    args = parse_args()
    pc = lagrange.io.load_mesh(args.input_point_cloud)

    points = pc.vertices

    normal_attr_ids = pc.get_matching_attribute_ids(usage=lagrange.AttributeUsage.Normal)
    assert len(normal_attr_ids) == 1
    normals = pc.attribute(normal_attr_ids[0]).data

    color_attr_ids = pc.get_matching_attribute_ids(usage=lagrange.AttributeUsage.Color)
    if len(color_attr_ids) == 1:
        colors = pc.attribute(color_attr_ids[0]).data
    else:
        colors = None

    mesh = lagrange.poisson.mesh_from_oriented_points(
        points=points, normals=normals, colors=colors, octree_depth=args.depth, verbose=args.verbose
    )

    lagrange.io.save_mesh(args.output_mesh, mesh)


if __name__ == "__main__":
    main()
