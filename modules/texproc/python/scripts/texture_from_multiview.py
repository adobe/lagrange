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

    uv run --extra scripts texture_from_multiview.py --mesh data/corp/texproc/original/pumpkin.glb --cameras data/corp/texproc/original/demo-16-views.json --multiview data/corp/texproc/original/multiview.png --output out.png

    uv run --extra scripts texture_from_multiview.py --grid 3 3 --base-confidence 0 --mesh data/corp/texproc/mickey/mesh.glb --multiview data/corp/texproc/mickey/multiview.png --height 2048 --width 2048
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


def split_multiview(multiview: Image.Image, grid_shape: Tuple[int, int]) -> List[ArrayNxNxK]:
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


def add_texture(
    scene: lagrange.scene.Scene,
    *,
    name: str,
    texture: npt.NDArray[np.uint8],
) -> lagrange.scene.TextureInfo:
    if texture.ndim != 3:
        raise ValueError(f"Expected texture shape (H, W, C), got {texture.shape}.")
    image = lagrange.scene.Image()
    image.name = f"{name}_image"
    image_buffer = lagrange.scene.ImageBuffer()
    image_buffer.data = texture
    image.image = image_buffer
    image_id = scene.add(image)

    scene_texture = lagrange.scene.Texture()
    scene_texture.name = f"{name}_texture"
    scene_texture.image = image_id
    scene_texture.mag_filter = lagrange.scene.Texture.TextureFilter.Linear
    scene_texture.min_filter = lagrange.scene.Texture.TextureFilter.LinearMipmapLinear
    texture_id = scene.add(scene_texture)

    texture_info = lagrange.scene.TextureInfo()
    texture_info.index = texture_id
    return texture_info


def create_textured_scene(
    mesh: lagrange.SurfaceMesh,
    base_color: npt.NDArray[np.uint8],
) -> lagrange.scene.Scene:
    scene = lagrange.scene.Scene()
    mesh_id = scene.add(mesh)
    material = lagrange.scene.Material()
    material.name = "base_color_material"
    material.double_sided = True
    material.base_color_value = np.array([1.0, 1.0, 1.0, 1.0], dtype=np.float32)
    material.alpha_mode = lagrange.scene.Material.AlphaMode.Opaque
    material.base_color_texture = add_texture(scene, name="base_color", texture=base_color)
    material_id = scene.add(material)

    instance = lagrange.scene.SceneMeshInstance()
    instance.mesh = mesh_id
    instance.materials.append(material_id)

    node = lagrange.scene.Node()
    node.name = "mesh_node"
    node.meshes.append(instance)
    node_id = scene.add(node)
    scene.root_nodes.append(node_id)
    return scene


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cameras", type=Path, required=False, help="Input multiview camera json")
    parser.add_argument("--mesh", type=Path, required=True, help="Input mesh file")
    parser.add_argument(
        "--base-confidence",
        type=float,
        default=0.3,
        help="Confidence in base mesh texture if available. Set to 0 to ignore base mesh texture.",
    )
    parser.add_argument(
        "--multiview",
        type=Path,
        required=True,
        nargs="+",
        help="Input renders as either a single 4x4 grid image or multiple individual render images",
    )
    parser.add_argument(
        "--grid",
        type=int,
        nargs=2,
        default=(4, 4),
        help="Grid size when a single image is provided",
    )
    # TODO: Use base texture width/height as default
    parser.add_argument("--width", "-W", type=int, default=1024, help="Target texture width")
    parser.add_argument("--height", "-H", type=int, default=1024, help="Target texture height")
    parser.add_argument(
        "--output-texture",
        nargs="?",
        type=Path,
        default="output.exr",
        help="Output texture file",
    )
    parser.add_argument(
        "--output-mesh", type=Path, default="output.glb", help="Output textured mesh"
    )
    return parser.parse_args()


def main():
    args = parse_args()
    if len(args.multiview) == 1:
        views = split_multiview(
            Image.open(args.multiview[0]), grid_shape=(args.grid[0], args.grid[1])
        )
    else:
        views = [np.asarray(Image.open(p)).astype(np.float32) / 255.0 for p in args.multiview]
    scene = lagrange.io.load_scene(args.mesh, stitch_vertices=True)
    if args.cameras:
        append_cameras(scene, json.loads(args.cameras.read_text()))
    colors, weights = lagrange.texproc.rasterize_textures_from_renders(
        scene,
        views,
        width=args.width,
        height=args.height,
        base_confidence=args.base_confidence,
    )
    mesh = lagrange.scene.scene_to_mesh(scene)
    image = lagrange.texproc.texture_compositing(
        mesh,
        colors,
        weights,
    )
    if args.output_texture:
        lagrange.logger.info(f"Saving texture to: '{args.output_texture.with_suffix('.exr')}'")
        pyexr.write(args.output_texture.with_suffix(".exr"), image)
    if args.output_mesh:
        lagrange.logger.info(f"Saving mesh to '{args.output_mesh}'")
        image = np.clip(image, 0.0, 1.0)
        image_uint8 = np.round(image * 255.0).astype(np.uint8)
        textured_scene = create_textured_scene(lagrange.unify_index_buffer(mesh), image_uint8)
        save_options = lagrange.io.SaveOptions()
        save_options.embed_images = True
        lagrange.io.save_scene(str(args.output_mesh), textured_scene, save_options)


if __name__ == "__main__":
    main()
