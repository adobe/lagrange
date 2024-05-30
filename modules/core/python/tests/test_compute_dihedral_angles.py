#
# Copyright 2023 Adobe. All rights reserved.
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
import pytest

from .assets import single_triangle, cube


class TestComputeDihedralAngles:
    def test_cube(self, cube):
        mesh = cube
        attr_id = lagrange.compute_dihedral_angles(mesh, output_attribute_name="dihedral_angles")
        assert mesh.has_attribute("dihedral_angles")
        assert mesh.get_attribute_name(attr_id) == "dihedral_angles"

        dihedral_angles = mesh.attribute(attr_id).data
        assert dihedral_angles.shape == (12,)

        assert np.all(dihedral_angles == pytest.approx(np.pi / 2))

    def test_triangle(self, single_triangle):
        mesh = single_triangle
        attr_id = lagrange.compute_dihedral_angles(
            mesh,
            facet_normal_attribute_name="facet_normals",
            keep_facet_normals=False,
        )
        assert not mesh.has_attribute("facet_normals")

        dihedral_angles = mesh.attribute(attr_id).data
        assert dihedral_angles.shape == (3,)

        assert np.all(dihedral_angles == 0)
