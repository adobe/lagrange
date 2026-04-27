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
import pytest


class TestComputeIntersectingPairs:
    def test_empty_mesh(self):
        """Test with an empty mesh."""
        mesh = lagrange.SurfaceMesh()
        intersections = lagrange.bvh.compute_intersecting_pairs(mesh)
        assert len(intersections) == 0

    def test_single_triangle(self):
        """Test with a single triangle (no self-intersections)."""
        mesh = lagrange.SurfaceMesh()
        mesh.add_vertex([0.0, 0.0, 0.0])
        mesh.add_vertex([1.0, 0.0, 0.0])
        mesh.add_vertex([0.0, 1.0, 0.0])
        mesh.add_triangle(0, 1, 2)

        intersections = lagrange.bvh.compute_intersecting_pairs(mesh)
        assert len(intersections) == 0

    def test_non_intersecting_triangles(self):
        """Test with two non-intersecting triangles."""
        mesh = lagrange.SurfaceMesh()
        # Triangle 1 in XY plane at z=0
        mesh.add_vertex([0.0, 0.0, 0.0])
        mesh.add_vertex([1.0, 0.0, 0.0])
        mesh.add_vertex([0.0, 1.0, 0.0])
        mesh.add_triangle(0, 1, 2)

        # Triangle 2 in XY plane at z=1 (parallel, separated)
        mesh.add_vertex([0.0, 0.0, 1.0])
        mesh.add_vertex([1.0, 0.0, 1.0])
        mesh.add_vertex([0.0, 1.0, 1.0])
        mesh.add_triangle(3, 4, 5)

        intersections = lagrange.bvh.compute_intersecting_pairs(mesh)
        assert len(intersections) == 0

    def test_adjacent_triangles(self):
        """Test that adjacent triangles sharing an edge are not reported."""
        mesh = lagrange.SurfaceMesh()
        mesh.add_vertex([0.0, 0.0, 0.0])
        mesh.add_vertex([1.0, 0.0, 0.0])
        mesh.add_vertex([0.5, 1.0, 0.0])
        mesh.add_vertex([0.5, -1.0, 0.0])

        # Two triangles sharing edge (0, 1)
        mesh.add_triangle(0, 1, 2)
        mesh.add_triangle(0, 1, 3)

        intersections = lagrange.bvh.compute_intersecting_pairs(mesh)
        assert len(intersections) == 0

    def test_intersecting_triangles(self):
        """Test with two triangles that intersect."""
        mesh = lagrange.SurfaceMesh()
        # Triangle 1 in XY plane
        mesh.add_vertex([-1.0, 0.0, 0.0])
        mesh.add_vertex([1.0, 0.0, 0.0])
        mesh.add_vertex([0.0, 1.0, 0.0])
        mesh.add_triangle(0, 1, 2)

        # Triangle 2 crossing through triangle 1
        mesh.add_vertex([0.0, 0.5, -1.0])
        mesh.add_vertex([0.0, 0.5, 1.0])
        mesh.add_vertex([0.0, -0.5, 0.0])
        mesh.add_triangle(3, 4, 5)

        intersections = lagrange.bvh.compute_intersecting_pairs(mesh)
        assert len(intersections) == 1
        assert intersections[0] == (0, 1)

    def test_multiple_intersections(self):
        """Test with multiple self-intersections."""
        mesh = lagrange.SurfaceMesh()

        # Create a mesh with known intersections
        # Triangle 0: horizontal at z=0
        mesh.add_vertex([-2.0, -2.0, 0.0])
        mesh.add_vertex([2.0, -2.0, 0.0])
        mesh.add_vertex([0.0, 2.0, 0.0])
        mesh.add_triangle(0, 1, 2)

        # Triangle 1: vertical crossing triangle 0
        mesh.add_vertex([0.0, 0.0, -1.0])
        mesh.add_vertex([0.0, 0.0, 1.0])
        mesh.add_vertex([1.0, 0.0, 0.0])
        mesh.add_triangle(3, 4, 5)

        # Triangle 2: another vertical crossing triangle 0
        mesh.add_vertex([-0.5, 0.0, -1.0])
        mesh.add_vertex([-0.5, 0.0, 1.0])
        mesh.add_vertex([-1.5, 0.0, 0.0])
        mesh.add_triangle(6, 7, 8)

        intersections = lagrange.bvh.compute_intersecting_pairs(mesh)

        # Should find at least 2 intersections: (0,1) and (0,2)
        assert len(intersections) >= 2

        # Check that first indices are smaller than second indices
        for pair in intersections:
            assert pair[0] < pair[1]

    def test_quad_mesh_throws(self):
        """Test that non-triangle meshes throw an error."""
        mesh = lagrange.SurfaceMesh()
        mesh.add_vertex([0.0, 0.0, 0.0])
        mesh.add_vertex([1.0, 0.0, 0.0])
        mesh.add_vertex([1.0, 1.0, 0.0])
        mesh.add_vertex([0.0, 1.0, 0.0])
        mesh.add_quad(0, 1, 2, 3)

        with pytest.raises(RuntimeError):
            lagrange.bvh.compute_intersecting_pairs(mesh)

    def test_return_type(self):
        """Test that the return type is a list."""
        mesh = lagrange.SurfaceMesh()
        mesh.add_vertex([0.0, 0.0, 0.0])
        mesh.add_vertex([1.0, 0.0, 0.0])
        mesh.add_vertex([0.0, 1.0, 0.0])
        mesh.add_triangle(0, 1, 2)

        intersections = lagrange.bvh.compute_intersecting_pairs(mesh)
        assert isinstance(intersections, list)
