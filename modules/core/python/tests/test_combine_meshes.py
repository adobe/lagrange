#
# Copyright 2022 Adobe. All rights reserved.
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

from .assets import cube


class TestCombineMeshes:
    def test_empty(self):
        mesh = lagrange.combine_meshes([], True)
        assert mesh.num_vertices == 0
        assert mesh.num_facets == 0

    def test_single_mesh(self, cube):
        mesh = cube
        out = lagrange.combine_meshes([cube], True)
        assert np.all(out.vertices == mesh.vertices)
        assert np.all(out.facets == mesh.facets)

    def test_two_meshes(self, cube):
        mesh1 = cube
        mesh2 = lagrange.SurfaceMesh()
        mesh2.vertices = mesh1.vertices + 10
        facets = mesh1.facets
        mesh2.facets = np.copy(
            facets
        )  # A copy is needed because facets is not writable.

        out = lagrange.combine_meshes([mesh1, mesh2], True)
        assert np.all(out.vertices[:8] == mesh1.vertices)
        assert np.all(out.vertices[8:] == mesh2.vertices)
        assert np.all(out.facets[:6] == mesh1.facets)
        assert np.all(out.facets[6:] == mesh2.facets + mesh1.num_vertices)

    def test_two_meshes_with_attribute(self, cube):
        mesh = cube
        opt = lagrange.NormalOptions()
        lagrange.compute_normal(mesh)

        out = lagrange.combine_meshes([mesh, mesh], True)
        assert out.has_attribute(opt.output_attribute_name)
        assert out.is_attribute_indexed(opt.output_attribute_name)
