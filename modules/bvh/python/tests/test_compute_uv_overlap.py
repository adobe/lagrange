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


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
def make_uv_mesh(uv_coords, faces):
    """Build a triangle mesh with per-vertex UV attribute.

    The 3-D positions are set to (u, v, 0) for convenience.
    """
    mesh = lagrange.SurfaceMesh()
    uv = np.array(uv_coords, dtype=float)
    mesh.vertices = np.column_stack([uv, np.zeros(len(uv))])
    mesh.facets = np.array(faces, dtype=np.uint32)
    mesh.create_attribute(
        "@uv",
        element=lagrange.AttributeElement.Vertex,
        usage=lagrange.AttributeUsage.UV,
        num_channels=2,
        initial_values=uv,
    )
    return mesh


# ---------------------------------------------------------------------------
# Basic overlap detection
# ---------------------------------------------------------------------------
class TestNoOverlap:
    def test_disjoint_triangles(self):
        mesh = make_uv_mesh(
            [[0, 0], [1, 0], [0, 1], [2, 0], [3, 0], [2, 1]],
            [[0, 1, 2], [3, 4, 5]],
        )
        result = lagrange.bvh.compute_uv_overlap(mesh)
        assert not result.has_overlap
        assert result.overlap_area is None

    def test_adjacent_shared_edge(self):
        mesh = make_uv_mesh(
            [[0, 0], [1, 0], [0, 1], [1, 1]],
            [[0, 1, 2], [1, 3, 2]],
        )
        result = lagrange.bvh.compute_uv_overlap(mesh)
        assert not result.has_overlap
        assert result.overlap_area is None

    def test_adjacent_shared_vertex(self):
        mesh = make_uv_mesh(
            [[0, 0], [1, 0], [0, 1], [1, 0], [2, 0], [1, 1]],
            [[0, 1, 2], [3, 4, 5]],
        )
        result = lagrange.bvh.compute_uv_overlap(mesh)
        assert not result.has_overlap


class TestOverlap:
    def test_identical_triangles(self):
        mesh = make_uv_mesh(
            [[0, 0], [1, 0], [0, 1], [0, 0], [1, 0], [0, 1]],
            [[0, 1, 2], [3, 4, 5]],
        )
        result = lagrange.bvh.compute_uv_overlap(mesh)
        assert result.has_overlap
        assert result.overlap_area == pytest.approx(0.5, abs=1e-5)

    def test_partial_overlap_known_area(self):
        # A: (0,0)-(1,0)-(0,1)  B: (0.5,0)-(1.5,0)-(0.5,1)
        # Intersection area = 0.125
        mesh = make_uv_mesh(
            [[0, 0], [1, 0], [0, 1], [0.5, 0], [1.5, 0], [0.5, 1]],
            [[0, 1, 2], [3, 4, 5]],
        )
        result = lagrange.bvh.compute_uv_overlap(mesh)
        assert result.has_overlap
        assert result.overlap_area == pytest.approx(0.125, abs=1e-5)

    def test_cw_oriented_triangle(self):
        # CW triangle overlaps identically with CCW triangle.
        mesh = make_uv_mesh(
            [[0, 0], [1, 0], [0, 1], [0, 0], [0, 1], [1, 0]],
            [[0, 1, 2], [3, 4, 5]],
        )
        result = lagrange.bvh.compute_uv_overlap(mesh)
        assert result.has_overlap
        assert result.overlap_area == pytest.approx(0.5, abs=1e-5)


# ---------------------------------------------------------------------------
# Edge cases
# ---------------------------------------------------------------------------
class TestEdgeCases:
    def test_single_triangle(self):
        mesh = make_uv_mesh([[0, 0], [1, 0], [0, 1]], [[0, 1, 2]])
        result = lagrange.bvh.compute_uv_overlap(mesh)
        assert not result.has_overlap
        assert result.overlap_area is None

    def test_three_triangles_one_pair_overlaps(self):
        # A and C overlap, B is disjoint.
        mesh = make_uv_mesh(
            [
                [0, 0],
                [1, 0],
                [0, 1],  # A
                [5, 5],
                [6, 5],
                [5, 6],  # B (far away)
                [0, 0],
                [1, 0],
                [0, 1],
            ],  # C (identical to A)
            [[0, 1, 2], [3, 4, 5], [6, 7, 8]],
        )
        result = lagrange.bvh.compute_uv_overlap(mesh, compute_overlapping_pairs=True)
        assert result.has_overlap
        assert result.overlapping_pairs == [(0, 2)]


# ---------------------------------------------------------------------------
# Options / kwargs
# ---------------------------------------------------------------------------
class TestOptions:
    def test_overlap_area_disabled(self):
        mesh = make_uv_mesh(
            [[0, 0], [1, 0], [0, 1], [0, 0], [1, 0], [0, 1]],
            [[0, 1, 2], [3, 4, 5]],
        )
        result = lagrange.bvh.compute_uv_overlap(mesh, compute_overlap_area=False)
        assert result.has_overlap
        assert result.overlap_area is None

    def test_overlapping_pairs(self):
        mesh = make_uv_mesh(
            [[0, 0], [1, 0], [0, 1], [0, 0], [1, 0], [0, 1]],
            [[0, 1, 2], [3, 4, 5]],
        )
        result = lagrange.bvh.compute_uv_overlap(mesh, compute_overlapping_pairs=True)
        assert result.overlapping_pairs == [(0, 1)]

    def test_overlapping_pairs_empty_when_disabled(self):
        mesh = make_uv_mesh(
            [[0, 0], [1, 0], [0, 1], [0, 0], [1, 0], [0, 1]],
            [[0, 1, 2], [3, 4, 5]],
        )
        result = lagrange.bvh.compute_uv_overlap(mesh, compute_overlapping_pairs=False)
        assert result.overlapping_pairs == []

    def test_coloring(self):
        mesh = make_uv_mesh(
            [[0, 0], [1, 0], [0, 1], [0, 0], [1, 0], [0, 1]],
            [[0, 1, 2], [3, 4, 5]],
        )
        result = lagrange.bvh.compute_uv_overlap(mesh, compute_overlap_coloring=True)
        assert result.has_overlap
        assert mesh.has_attribute("@uv_overlap_color")

    def test_coloring_not_created_when_disabled(self):
        mesh = make_uv_mesh(
            [[0, 0], [1, 0], [0, 1], [0, 0], [1, 0], [0, 1]],
            [[0, 1, 2], [3, 4, 5]],
        )
        lagrange.bvh.compute_uv_overlap(mesh, compute_overlap_coloring=False)
        assert not mesh.has_attribute("@uv_overlap_color")


# ---------------------------------------------------------------------------
# All methods produce consistent results
# ---------------------------------------------------------------------------
class TestMethods:
    @pytest.mark.parametrize("method", list(lagrange.bvh.UVOverlapMethod))
    def test_method_overlap_area(self, method):
        mesh = make_uv_mesh(
            [[0, 0], [1, 0], [0, 1], [0.5, 0], [1.5, 0], [0.5, 1]],
            [[0, 1, 2], [3, 4, 5]],
        )
        result = lagrange.bvh.compute_uv_overlap(
            mesh, method=method, compute_overlapping_pairs=True
        )
        assert result.has_overlap
        assert result.overlap_area == pytest.approx(0.125, abs=1e-5)
        assert result.overlapping_pairs == [(0, 1)]

    @pytest.mark.parametrize("method", list(lagrange.bvh.UVOverlapMethod))
    def test_method_no_overlap(self, method):
        mesh = make_uv_mesh(
            [[0, 0], [1, 0], [0, 1], [2, 0], [3, 0], [2, 1]],
            [[0, 1, 2], [3, 4, 5]],
        )
        result = lagrange.bvh.compute_uv_overlap(mesh, method=method)
        assert not result.has_overlap


# ---------------------------------------------------------------------------
# NamedTuple behavior
# ---------------------------------------------------------------------------
class TestResultType:
    def test_unpacking(self):
        mesh = make_uv_mesh(
            [[0, 0], [1, 0], [0, 1], [0, 0], [1, 0], [0, 1]],
            [[0, 1, 2], [3, 4, 5]],
        )
        has_overlap, area, pairs, coloring_id = lagrange.bvh.compute_uv_overlap(mesh)
        assert has_overlap is True
        assert area == pytest.approx(0.5, abs=1e-5)

    def test_asdict(self):
        mesh = make_uv_mesh(
            [[0, 0], [1, 0], [0, 1], [0, 0], [1, 0], [0, 1]],
            [[0, 1, 2], [3, 4, 5]],
        )
        result = lagrange.bvh.compute_uv_overlap(mesh)
        d = result._asdict()
        assert "has_overlap" in d
        assert "overlap_area" in d
        assert "overlapping_pairs" in d
        assert "overlap_coloring_id" in d
