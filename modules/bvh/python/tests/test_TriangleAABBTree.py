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
import lagrange
import numpy as np
import pytest

from .asset import cube, square


class TestTriangleAABBTree:
    def test_elements_in_radius_3D(self, cube):
        aabb = lagrange.bvh.TriangleAABBTree3D(cube)
        element_ids, closest_pts = aabb.get_elements_in_radius([0.0, 0.0, 0.0], 0.1)
        assert len(element_ids) == 5
        assert closest_pts.shape[0] == 5
        assert np.all(np.any(cube.facets[element_ids] == 0, axis=1))

    def test_closest_point_3D(self, cube):
        aabb = lagrange.bvh.TriangleAABBTree3D(cube)
        element_id, closest_pt, sq_dist = aabb.get_closest_point([0.0, 0.0, 0.0])
        assert sq_dist == 0.0
        assert np.allclose(closest_pt, [0.0, 0.0, 0.0])

        element_id, closest_pt, sq_dist = aabb.get_closest_point([0.1, 0.2, -1.0])
        assert element_id == 0
        assert sq_dist == 1.0
        assert np.allclose(closest_pt, [0.1, 0.2, 0.0])

    def test_elements_in_radius_2D(self, square):
        aabb = lagrange.bvh.TriangleAABBTree2D(square)
        element_ids, closest_pts = aabb.get_elements_in_radius([0.0, 0.0], 0.1)
        assert len(element_ids) == 2
        assert closest_pts.shape[0] == 2
        assert np.all(np.any(square.facets[element_ids] == 0, axis=1))

    def test_closest_point_2D(self, square):
        aabb = lagrange.bvh.TriangleAABBTree2D(square)
        element_id, closest_pt, sq_dist = aabb.get_closest_point([0.0, 0.0])
        assert sq_dist == 0.0
        assert np.allclose(closest_pt, [0.0, 0.0])

        element_id, closest_pt, sq_dist = aabb.get_closest_point([-0.1, 0.2])
        assert element_id == 1
        assert sq_dist == pytest.approx(0.1**2)
        assert np.allclose(closest_pt, [0.0, 0.2])
