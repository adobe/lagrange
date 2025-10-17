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


class TestEdgeAABBTree:
    def test_closest_point_3D(self, cube):
        cube.initialize_edges()
        vertices = cube.vertices
        edges = cube.edges
        aabb = lagrange.bvh.EdgeAABBTree3D(vertices, edges)
        element_id, closest_pt, sq_dist = aabb.get_closest_point([-1, 0.0, 0.0])
        assert sq_dist == pytest.approx(1.0)

    def test_get_elements_in_radius_3D(self, cube):
        cube.initialize_edges()
        vertices = cube.vertices
        edges = cube.edges
        aabb = lagrange.bvh.EdgeAABBTree3D(vertices, edges)
        element_ids, closest_pts = aabb.get_elements_in_radius([0.0, 0.5, 0.5], 0.1)
        assert len(element_ids) == 1
        assert np.allclose(closest_pts, [[0.0, 0.5, 0.5]])

        element_ids, closest_pts = aabb.get_elements_in_radius([0.2, 0.5, 0.5], 0.1)
        assert len(element_ids) == 0
        assert closest_pts.shape[0] == 0

    def test_get_containing_elements_3D(self, cube):
        cube.initialize_edges()
        vertices = cube.vertices
        edges = cube.edges
        aabb = lagrange.bvh.EdgeAABBTree3D(vertices, edges)

        element_ids = aabb.get_containing_elements([0.0, 0.5, 0.5])
        assert len(element_ids) == 1

        element_ids = aabb.get_containing_elements([0.01, 0.5, 0.5])
        assert len(element_ids) == 0

    def test_closest_point_2D(self, square):
        square.initialize_edges()
        vertices = square.vertices
        edges = square.edges
        aabb = lagrange.bvh.EdgeAABBTree2D(vertices, edges)
        element_id, closest_pt, sq_dist = aabb.get_closest_point([-1, 0.0])
        assert sq_dist == pytest.approx(1.0)

    def test_get_elements_in_radius_2D(self, square):
        square.initialize_edges()
        vertices = square.vertices
        edges = square.edges
        aabb = lagrange.bvh.EdgeAABBTree2D(vertices, edges)
        element_ids, closest_pts = aabb.get_elements_in_radius([0.0, 0.5], 0.1)
        assert len(element_ids) == 1
        assert np.allclose(closest_pts, [[0.0, 0.5]])

        element_ids, closest_pts = aabb.get_elements_in_radius([0.2, 0.5], 0.1)
        assert len(element_ids) == 0
        assert closest_pts.shape[0] == 0

    def test_get_containing_elements_2D(self, square):
        square.initialize_edges()
        vertices = square.vertices
        edges = square.edges
        aabb = lagrange.bvh.EdgeAABBTree2D(vertices, edges)

        element_ids = aabb.get_containing_elements([0.5, 0.5])
        assert len(element_ids) == 1

        element_ids = aabb.get_containing_elements([0.51, 0.5])
        assert len(element_ids) == 0
