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
import pytest

from .asset import cube  # noqa: F401


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def make_parallel_squares(d: float):
    """Two axis-aligned unit squares at z=0 and z=d, each made of 2 triangles.
    Every vertex of sq_a is directly below a vertex of sq_b, so the
    closest-point distance from each vertex to the opposite mesh is exactly d.
    """
    sq_a = lagrange.SurfaceMesh()
    sq_a.vertices = np.array([[0, 0, 0], [1, 0, 0], [1, 1, 0], [0, 1, 0]], dtype=np.float32)
    sq_a.facets = np.array([[0, 1, 2], [0, 2, 3]], dtype=np.uint32)

    sq_b = lagrange.SurfaceMesh()
    sq_b.vertices = np.array([[0, 0, d], [1, 0, d], [1, 1, d], [0, 1, d]], dtype=np.float32)
    sq_b.facets = np.array([[0, 1, 2], [0, 2, 3]], dtype=np.uint32)

    return sq_a, sq_b


def make_empty_mesh():
    """Triangle mesh with no vertices and no faces."""
    return lagrange.SurfaceMesh()


class TestComputeMeshDistances:
    def test_self_all_distances_zero(self, cube):
        attr_id = lagrange.bvh.compute_mesh_distances(cube, cube)
        dist = cube.attribute(attr_id).data
        assert len(dist) == cube.num_vertices
        assert np.allclose(dist, 0.0, atol=1e-5)

    def test_parallel_squares_all_distances_equal_d(self):
        d = 1.5
        sq_a, sq_b = make_parallel_squares(d)
        attr_id = lagrange.bvh.compute_mesh_distances(sq_a, sq_b)
        dist = sq_a.attribute(attr_id).data
        assert len(dist) == sq_a.num_vertices
        assert np.allclose(dist, d, atol=1e-5)

    def test_custom_attribute_name(self):
        sq_a, sq_b = make_parallel_squares(1.0)
        lagrange.bvh.compute_mesh_distances(sq_a, sq_b, output_attribute_name="my_dist")
        assert sq_a.has_attribute("my_dist")
        assert not sq_a.has_attribute("@distance_to_mesh")

    def test_default_attribute_name(self):
        sq_a, sq_b = make_parallel_squares(1.0)
        lagrange.bvh.compute_mesh_distances(sq_a, sq_b)
        assert sq_a.has_attribute("@distance_to_mesh")

    def test_empty_source_no_crash(self):
        _, sq_b = make_parallel_squares(1.0)
        empty = make_empty_mesh()
        attr_id = lagrange.bvh.compute_mesh_distances(empty, sq_b)
        dist = empty.attribute(attr_id).data
        assert empty.num_vertices == 0
        assert len(dist) == 0

    def test_empty_target_all_distances_zero(self):
        sq_a, _ = make_parallel_squares(1.0)
        empty = make_empty_mesh()
        attr_id = lagrange.bvh.compute_mesh_distances(sq_a, empty)
        dist = sq_a.attribute(attr_id).data
        assert np.allclose(dist, 0.0, atol=1e-6)

    def test_both_empty_no_crash(self):
        empty_src = make_empty_mesh()
        empty_tgt = make_empty_mesh()
        attr_id = lagrange.bvh.compute_mesh_distances(empty_src, empty_tgt)
        dist = empty_src.attribute(attr_id).data
        assert empty_src.num_vertices == 0
        assert len(dist) == 0


class TestComputeHausdorff:
    def test_self_zero(self, cube):
        h = lagrange.bvh.compute_hausdorff(cube, cube)
        assert h == pytest.approx(0.0, abs=1e-5)

    def test_parallel_squares_equals_d(self):
        d = 2.0
        sq_a, sq_b = make_parallel_squares(d)
        h = lagrange.bvh.compute_hausdorff(sq_a, sq_b)
        assert h == pytest.approx(d, abs=1e-5)

    def test_symmetric(self):
        sq_a, sq_b = make_parallel_squares(1.0)
        h_ab = lagrange.bvh.compute_hausdorff(sq_a, sq_b)
        h_ba = lagrange.bvh.compute_hausdorff(sq_b, sq_a)
        assert h_ab == pytest.approx(h_ba, abs=1e-5)

    def test_empty_source_zero(self):
        _, sq_b = make_parallel_squares(1.0)
        empty = make_empty_mesh()
        h = lagrange.bvh.compute_hausdorff(empty, sq_b)
        assert h == pytest.approx(0.0, abs=1e-6)

    def test_empty_target_zero(self):
        sq_a, _ = make_parallel_squares(1.0)
        empty = make_empty_mesh()
        h = lagrange.bvh.compute_hausdorff(sq_a, empty)
        assert h == pytest.approx(0.0, abs=1e-6)

    def test_both_empty_zero(self):
        h = lagrange.bvh.compute_hausdorff(make_empty_mesh(), make_empty_mesh())
        assert h == pytest.approx(0.0, abs=1e-6)


class TestComputeChamfer:
    def test_self_zero(self, cube):
        c = lagrange.bvh.compute_chamfer(cube, cube)
        assert c == pytest.approx(0.0, abs=1e-5)

    def test_parallel_squares_equals_2d_squared(self):
        # Every vertex of sq_a is at distance d from sq_b and vice versa.
        # Chamfer = (1/4)*4*d^2 + (1/4)*4*d^2 = 2*d^2.
        d = 1.5
        sq_a, sq_b = make_parallel_squares(d)
        c = lagrange.bvh.compute_chamfer(sq_a, sq_b)
        assert c == pytest.approx(2.0 * d * d, abs=1e-4)

    def test_symmetric(self):
        sq_a, sq_b = make_parallel_squares(1.0)
        c_ab = lagrange.bvh.compute_chamfer(sq_a, sq_b)
        c_ba = lagrange.bvh.compute_chamfer(sq_b, sq_a)
        assert c_ab == pytest.approx(c_ba, abs=1e-4)

    def test_empty_source_zero(self):
        _, sq_b = make_parallel_squares(1.0)
        empty = make_empty_mesh()
        c = lagrange.bvh.compute_chamfer(empty, sq_b)
        assert c == pytest.approx(0.0, abs=1e-6)

    def test_empty_target_zero(self):
        sq_a, _ = make_parallel_squares(1.0)
        empty = make_empty_mesh()
        c = lagrange.bvh.compute_chamfer(sq_a, empty)
        assert c == pytest.approx(0.0, abs=1e-6)

    def test_both_empty_zero(self):
        c = lagrange.bvh.compute_chamfer(make_empty_mesh(), make_empty_mesh())
        assert c == pytest.approx(0.0, abs=1e-6)
