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

"""Extract texture from a mesh"""

import argparse
import lagrange
from pathlib import Path
from PIL import Image


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", help="input mesh file")
    parser.add_argument("output", help="output mesh file")
    return parser.parse_args()


def dump_texture(img, filename):
    img.uri = filename
    img_buffer = img.image
    buffer = img_buffer.data.reshape((img_buffer.height, img_buffer.width, img_buffer.num_channels))
    if img_buffer.num_channels == 4:
        im = Image.fromarray(buffer, "RGBA")
    elif img_buffer.num_channels == 3:
        im = Image.fromarray(buffer, "RGB")
    elif img_buffer.num_channels == 1:
        im = Image.fromarray(buffer, "L")
    else:
        raise ValueError(f"Unsupported number of channels: {img_buffer.num_channels}")
    im.save(filename)


def main():
    args = parse_args()
    scene = lagrange.io.load_scene(args.input)

    output_filename = Path(args.output)
    basename = output_filename.stem
    texture_count = 0
    for node in scene.nodes:
        for instance in node.meshes:
            assert instance.mesh != lagrange.invalid_index
            for mat_id in instance.materials:
                mat = scene.materials[mat_id]
                if mat.base_color_texture.index != lagrange.invalid_index:
                    tex = scene.textures[mat.base_color_texture.index]
                    assert tex.image != lagrange.invalid_index
                    img = scene.images[tex.image]
                    if len(img.image.data) != 0:
                        texture_filename = output_filename.with_suffix(".png").with_stem(
                            f"{basename}_{texture_count:03}"
                        )
                        dump_texture(img, texture_filename)
                        texture_count += 1

    lagrange.io.save_scene(args.output, scene)


if __name__ == "__main__":
    main()
