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
"""
Rasterize and composite textures from multiview renders.

To try it out on sample data, run the following commands:

    uv run scripts/artifactory.py pull
    uv run --extra scripts texture_from_multiview.py --mesh data/corp/texproc/original/pumpkin.glb --cameras data/corp/texproc/original/demo-16-views.json --multiview data/corp/texproc/original/multiview.png out.png
"""

from pathlib import Path
import json
from typing import Tuple, List, Annotated, Literal, TypeVar
import math
import logging
import argparse

import numpy as np
import numpy.typing as npt
from PIL import Image
import pyexr

import lagrange

lagrange.logger.setLevel(logging.INFO)

DType = TypeVar("DType", bound=np.generic)

ArrayNxNxK = Annotated[npt.NDArray[DType], Literal["N", "N", "K"]]
ArrayNxNx1 = Annotated[npt.NDArray[DType], Literal["N", "N", 1]]


def append_cameras(scene: lagrange.scene.Scene, cameras: dict) -> lagrange.scene.Scene:
    """
    Convert camera data from multiview json to a Lagrange camera, and append it to the scene.
    """
    assert len(scene.cameras) == 0, (
        f"Scene already contains {len(scene.cameras)} cameras. Please use a clean scene before calling this function."
    )
    camera_root = lagrange.scene.Node()
    camera_root.name = "cameras"
    camera_root_id = scene.add(camera_root)
    for i, frame in enumerate(cameras["frames"]):
        # Create camera object with intrinsic properties
        camera = lagrange.scene.Camera()
        camera.name = f"camera_{i}"
        # Compute angular field of view from pixel-space focal length. It inverts the operations from this script:
        # https://git.corp.adobe.com/kaiz/blender_renderer/blob/main/blender_renderer/objaverse_rgba.py
        # camera_object.data.lens = camera_object.data.sensor_width / ( 2 * np.tan(np.deg2rad(hfov / 2)) )
        # fx = cx / (camera_object.data.sensor_width / 2.0 / camera_object.data.lens)
        # fx = cx / np.tan(np.deg2rad(hfov / 2))
        camera.horizontal_fov = 2 * math.atan(frame["cx"] / frame["fx"])
        camera.aspect_ratio = frame["w"] / frame["h"]
        if frame["cx"] * 2 != frame["w"] or frame["cy"] * 2 != frame["h"]:
            lagrange.logger.warning(
                f"Camera {camera.name} has non-centered principal point. "
                f"cx: {frame['cx']}, cy: {frame['cy']}, width: {frame['w']}, height: {frame['h']}"
            )
        camera_id = scene.add(camera)

        # Create a node in the scene to encode the extrinsic properties
        node = lagrange.scene.Node()
        node.name = camera.name
        node.cameras.append(camera_id)
        node.transform = np.linalg.inv(np.array(frame["w2c"]))
        A = np.identity(4, dtype=np.float64)
        A[1, 1] = -1  # Flip camera space Y axis
        A[2, 2] = -1  # Flip camera space Z axis
        B = np.identity(4, dtype=np.float64)
        B[1, 1] = B[2, 2] = 0  # Rotate 90° around X axis (swap Y and Z, then flip Z)
        B[2, 1] = -1
        B[1, 2] = 1
        node.transform = B @ node.transform @ A
        node_id = scene.add(node)
        scene.add_child(camera_root_id, node_id)
    return scene


def split_multiview(multiview: Image, grid_shape: Tuple[int, int]) -> List[ArrayNxNxK[np.float32]]:
    """
    Split the multiview image into individual render images based on camera data.
    """
    width, height = multiview.size
    render_width = width // grid_shape[1]
    render_height = height // grid_shape[0]

    renders = []
    for i in range(grid_shape[0]):
        for j in range(grid_shape[1]):
            left = j * render_width
            upper = i * render_height
            right = left + render_width
            lower = upper + render_height
            render = multiview.crop((left, upper, right, lower))
            render = np.asarray(render).astype(np.float32)
            render = render / 255.0
            renders.append(render)

    return renders


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cameras", type=Path, required=True, help="Input multiview camera json")
    parser.add_argument("--mesh", type=Path, required=True, help="Input mesh file")
    parser.add_argument("--multiview", type=Path, required=True, help="Input renders in a 4x4 grid")
    parser.add_argument("--width", "-W", type=int, default=1024, help="Target texture width")
    parser.add_argument("--height", "-H", type=int, default=1024, help="Target texture height")
    parser.add_argument(
        "output",
        nargs="?",
        type=Path,
        default="output.exr",
        help="Output texture file",
    )
    return parser.parse_args()


def main():
    args = parse_args()
    views = split_multiview(Image.open(args.multiview), grid_shape=(4, 4))
    scene = lagrange.io.load_scene(args.mesh, stitch_vertices=True)
    append_cameras(scene, json.loads(args.cameras.read_text()))
    colors, weights = lagrange.texproc.rasterize_textures_from_renders(
        scene,
        views,
        width=args.width,
        height=args.height,
    )
    mesh = lagrange.scene.scene_to_mesh(scene)
    image = lagrange.texproc.texture_compositing(
        mesh,
        colors,
        weights,
    )
    lagrange.logger.info(f"Saving result to: '{args.output}'")
    pyexr.write(args.output, image)


if __name__ == "__main__":
    main()
