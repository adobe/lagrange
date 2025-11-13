#!/usr/bin/env python

#
# Copyright 2025 Adobe. All rights reserved.
# This file is licensed to you under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License. You may obtain a copy
# of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under
# the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
# OF ANY KIND, either express or implied. See the License for the specific language
# governing permissions and limitations under the License.
#

from pathlib import Path
import argparse
from typing import Tuple, Annotated, Literal, TypeVar, Optional

import lagrange
import numpy as np
import numpy.typing as npt
import pyexr
from PIL import Image

DType = TypeVar("DType", bound=np.generic)

ArrayNxMxK = Annotated[npt.NDArray[DType], Literal["N", "M", "K"]]


def single_mesh_from_scene(
    scene: lagrange.scene.Scene,
) -> Tuple[lagrange.SurfaceMesh, Optional[ArrayNxMxK]]:
    """
    Extract a single UV unwrapped mesh and optionally its base color texture from a scene.

    Args:
        scene: Input scene containing exactly one mesh instance node.

    Returns:
        A tuple containing:
        - The extracted mesh with world transform applied
        - The base color texture as a numpy array (height, width, 3), or None if not available

    Raises:
        RuntimeError: If the scene does not contain exactly one mesh node with one mesh instance
    """
    # Find mesh nodes in the scene
    mesh_node_ids = []
    for node_id, node in enumerate(scene.nodes):
        if len(node.meshes) > 0:
            mesh_node_ids.append(node_id)

    if len(mesh_node_ids) != 1:
        raise RuntimeError(
            f"Input scene contains {len(mesh_node_ids)} mesh nodes. Expected exactly 1 mesh node."
        )

    mesh_node = scene.nodes[mesh_node_ids[0]]

    if len(mesh_node.meshes) != 1:
        raise RuntimeError(
            f"Input scene has a mesh node with {len(mesh_node.meshes)} instance per node. "
            f"Expected exactly 1 instance per node"
        )

    mesh_instance = mesh_node.meshes[0]
    assert mesh_instance.mesh is not None
    mesh = scene.meshes[mesh_instance.mesh]

    # Apply node local->world transform
    world_from_mesh = lagrange.scene.compute_global_node_transform(scene, mesh_node_ids[0])
    lagrange.transform_mesh(mesh, world_from_mesh)

    # Find base texture if available
    num_materials = len(mesh_instance.materials)
    if num_materials != 1:
        lagrange.logger.warning(
            f"Mesh node has {num_materials} materials. Expected exactly 1 material. "
            f"Ignoring materials."
        )
        return mesh, None

    material = scene.materials[mesh_instance.materials[0]]
    if material.base_color_texture.texcoord != 0:
        lagrange.logger.warning(
            f"Mesh node material texcoord is {material.base_color_texture.texcoord} != 0. "
            f"Expected 0. Ignoring texcoord."
        )

    texture_id = material.base_color_texture.index
    assert texture_id < len(scene.textures)
    texture = scene.textures[texture_id]

    image_id = texture.image
    assert image_id < len(scene.images)
    image = scene.images[image_id].image.data

    return mesh, image


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mesh", type=Path, required=True, help="Input mesh file")
    parser.add_argument(
        "--texture",
        type=Path,
        help="Optional input texture (overrides base color texture from mesh material)",
    )
    parser.add_argument("--radius", "-r", type=float, default=10, help="Dilation radius in pixels")
    parser.add_argument("--position", action="store_true", help="Output dilated position map")
    parser.add_argument(
        "--output",
        nargs="?",
        type=Path,
        default="output.exr",
        help="Output texture file",
    )
    return parser.parse_args()


def main():
    args = parse_args()
    scene = lagrange.io.load_scene(args.mesh)
    mesh, texture_img = single_mesh_from_scene(scene)

    if args.texture is not None:
        lagrange.logger.info(f"Loading texture from: '{args.texture}'")
        texture_img = np.asarray(Image.open(args.texture)).astype(np.float32) / 255.0
    else:
        assert texture_img is not None
        texture_img = texture_img.astype(np.float32) / 255.0

    if args.position:
        lagrange.logger.info("Computing dilated geodesic position map")
        dilated_texture = lagrange.texproc.geodesic_position(
            mesh, texture_img.shape[0], texture_img.shape[1]
        )
    else:
        lagrange.logger.info("Using input texture as initial texture for dilation")
        dilated_texture = lagrange.texproc.geodesic_dilation(
            mesh, texture_img, dilation_radius=args.radius
        )

    pyexr.write(args.output, dilated_texture)


if __name__ == "__main__":
    main()
