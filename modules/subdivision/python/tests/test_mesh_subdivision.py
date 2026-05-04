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
import lagrange
import numpy as np


class TestMeshSubdivision:
    def test_cube_passthrough(self, cube):
        mesh = lagrange.subdivision.subdivide_mesh(
            cube,
            num_levels=0,
        )
        all_zeros = (np.abs(mesh.vertices) < 1e-6).all()
        assert not all_zeros

    def test_cube_passthrough_limit(self, cube):
        mesh = lagrange.subdivision.subdivide_mesh(
            cube,
            num_levels=0,
            use_limit_surface=True,
        )
        all_zeros = (np.abs(mesh.vertices) < 1e-6).all()
        assert not all_zeros

    def test_house_passthrough(self, house):
        assert house.is_attribute_indexed("texcoord_0")
        mesh = lagrange.subdivision.subdivide_mesh(
            house,
            num_levels=0,
        )
        all_zeros = (np.abs(mesh.vertices) < 1e-6).all()
        assert not all_zeros
        assert mesh.is_attribute_indexed("texcoord_0")

    def test_house_passthrough_limit(self, house):
        assert house.is_attribute_indexed("texcoord_0")
        mesh = lagrange.subdivision.subdivide_mesh(
            house,
            num_levels=0,
            use_limit_surface=True,
        )
        all_zeros = (np.abs(mesh.vertices) < 1e-6).all()
        assert not all_zeros
        assert mesh.is_attribute_indexed("texcoord_0")

    def test_cube_twice(self, cube):
        num_levels = 2
        vert_id, edge_id, normal_id = lagrange.subdivision.compute_sharpness(cube)
        mesh = lagrange.subdivision.subdivide_mesh(
            cube,
            num_levels=num_levels,
            vertex_sharpness_attr=vert_id,
            edge_sharpness_attr=edge_id,
        )
        assert mesh.num_facets == cube.num_corners * 4 ** (num_levels - 1)

    def test_house_twice(self, house):
        assert house.is_attribute_indexed("texcoord_0")
        mesh = lagrange.subdivision.subdivide_mesh(
            house,
            num_levels=2,
        )
        all_zeros = (np.abs(mesh.vertices) < 1e-6).all()
        assert not all_zeros
        texcoord_id = mesh.get_attribute_id("texcoord_0")
        assert mesh.is_attribute_indexed(texcoord_id)
        texcoord_attr = mesh.indexed_attribute(texcoord_id)
        assert np.abs(texcoord_attr.indices.data - mesh.facets.flatten()).max() == 0
        assert np.abs(texcoord_attr.values.data - mesh.vertices[:, :2]).max() < 1e-6
