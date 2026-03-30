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


@pytest.fixture
def triangle():
    """A triangle mesh with vertices at identity matrix positions.

    This fixture uses np.eye(3) for vertex positions, which creates a triangle
    in 3D space with vertices at [1,0,0], [0,1,0], [0,0,1]. This is useful for
    testing differential operators as it creates a non-degenerate triangle.
    """
    mesh = lagrange.SurfaceMesh()
    mesh.add_vertices(np.eye(3))
    mesh.add_triangle(0, 1, 2)
    assert mesh.num_vertices == 3
    assert mesh.num_facets == 1
    return mesh


@pytest.fixture
def nonplanar_quad():
    """A non-planar quadrilateral mesh.

    Creates a quad with vertices that are not coplanar, useful for testing
    differential operators on non-planar polygonal faces.
    """
    mesh = lagrange.SurfaceMesh()
    mesh.add_vertex([0, 0, 0])
    mesh.add_vertex([1, 0, 1])
    mesh.add_vertex([1, 1, 0])
    mesh.add_vertex([0, 1, 1])
    mesh.add_quad(0, 1, 2, 3)
    return mesh


@pytest.fixture
def pyramid():
    """A pyramid mesh (4 triangles + 1 quad base).

    Creates a pyramid with a square base and four triangular faces meeting at an apex.
    This is a mixed polygonal mesh useful for testing polyddg operators.
    """
    mesh = lagrange.SurfaceMesh()
    mesh.add_vertex([0, 0, 0])
    mesh.add_vertex([1, 0, 0])
    mesh.add_vertex([1, 1, 0])
    mesh.add_vertex([0, 1, 0])
    mesh.add_vertex([0.5, 0.5, 1])
    mesh.add_triangle(0, 1, 4)
    mesh.add_triangle(1, 2, 4)
    mesh.add_triangle(2, 3, 4)
    mesh.add_triangle(3, 0, 4)
    mesh.add_quad(0, 3, 2, 1)
    return mesh


@pytest.fixture
def octahedron():
    """An octahedron mesh (genus-0 closed surface).

    Creates a regular octahedron with 6 vertices and 8 triangular faces.
    This is a closed manifold mesh with genus 0, useful for testing Hodge decomposition
    and other topological operations. The harmonic component of Hodge decomposition
    should vanish on this mesh.
    """
    mesh = lagrange.SurfaceMesh()
    mesh.add_vertex([1, 0, 0])
    mesh.add_vertex([0, 1, 0])
    mesh.add_vertex([-1, 0, 0])
    mesh.add_vertex([0, -1, 0])
    mesh.add_vertex([0, 0, 1])
    mesh.add_vertex([0, 0, -1])
    mesh.add_triangle(0, 1, 4)
    mesh.add_triangle(1, 2, 4)
    mesh.add_triangle(2, 3, 4)
    mesh.add_triangle(3, 0, 4)
    mesh.add_triangle(0, 5, 3)
    mesh.add_triangle(1, 5, 0)
    mesh.add_triangle(2, 5, 1)
    mesh.add_triangle(3, 5, 2)
    mesh.initialize_edges()
    return mesh
