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

import lagrange
import numpy as np
from pathlib import Path
import pytest


def load_alpha_data(scene_path: Path):
    scene_options = lagrange.io.LoadOptions()
    scene_options.stitch_vertices = True
    scene = lagrange.io.load_scene(scene_path, scene_options)

    # retrieve single instance
    assert len(scene.nodes) == 1
    node = scene.nodes[0]
    assert len(node.meshes) == 1
    instance = node.meshes[0]

    # retrieve material
    assert len(instance.materials) == 1
    material = scene.materials[instance.materials[0]]
    assert material.alpha_mode == lagrange.scene.Material.AlphaMode.Blend
    assert material.alpha_cutoff >= 0.0
    assert material.alpha_cutoff <= 1.0
    image = scene.images[material.base_color_texture.index].image.data
    assert image.shape[2] == 4

    # retrieve mesh
    mesh = scene.meshes[instance.mesh]
    texcoord_id = mesh.get_attribute_id(f"texcoord_{material.base_color_texture.texcoord}")
    lagrange.cast_attribute(mesh, texcoord_id, np.float64)
    assert mesh.is_attribute_indexed(texcoord_id)
    assert mesh.is_triangle_mesh

    # move mesh to world space
    node_transform = lagrange.scene.compute_global_node_transform(scene, 0)

    return mesh, texcoord_id, image, material.alpha_cutoff, node_transform


all_alpha_datas = (
    []
    if lagrange.variant == "open"
    else list(
        map(
            lambda pp: load_alpha_data(Path("data/corp/texproc") / pp),
            [Path("alpha_cube_numbers.glb"), Path("alpha_cube_letters.glb")],
        )
    )
)


@pytest.mark.skipif(
    lagrange.variant == "open",
    reason="Test requires corp data",
)
@pytest.mark.parametrize(
    "alpha_data",
    all_alpha_datas,
)
class TestMeshWithAlphaMask:
    def test_extract_alpha_cube(self, alpha_data):
        mesh, texcoord_id, image, alpha_threshold, transform = alpha_data

        # extract tessellated mesh
        mesh_ = lagrange.texproc.extract_mesh_with_alpha_mask(
            mesh,
            image,
            texcoord_id,
            alpha_threshold,
        )
        assert mesh_.num_facets > 0

    def test_transform(self, alpha_data):
        mesh, texcoord_id, image, alpha_threshold, transform = alpha_data

        assert np.abs(transform - np.eye(4)).max() < 1e-7
