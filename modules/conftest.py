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
"""
Global pytest fixtures for Lagrange tests.

This conftest.py provides common fixtures that are shared across all modules.
Fixtures defined here are automatically available to all test files without imports.
"""

import lagrange
import numpy as np
import pytest


# =============================================================================
# Basic Mesh Fixtures
# =============================================================================


@pytest.fixture
def single_triangle():
    """A simple triangle mesh with vertices at the identity matrix positions."""
    mesh = lagrange.SurfaceMesh()
    mesh.add_vertices(np.eye(3))
    mesh.add_triangle(0, 1, 2)
    assert mesh.num_vertices == 3
    assert mesh.num_facets == 1
    return mesh


@pytest.fixture
def single_triangle_with_index(single_triangle):
    """A single triangle mesh with vertex_index and facet_index attributes."""
    mesh = single_triangle
    mesh.wrap_as_attribute(
        "vertex_index",
        lagrange.AttributeElement.Vertex,
        lagrange.AttributeUsage.Scalar,
        np.array([1, 2, 3], dtype=np.intc),
    )
    assert mesh.has_attribute("vertex_index")
    mesh.wrap_as_attribute(
        "facet_index",
        lagrange.AttributeElement.Facet,
        lagrange.AttributeUsage.Scalar,
        np.array([1], dtype=np.intc),
    )
    assert mesh.has_attribute("facet_index")
    return mesh


@pytest.fixture
def single_triangle_with_uv(single_triangle):
    """A single triangle mesh with UV coordinates."""
    mesh = single_triangle
    mesh.wrap_as_indexed_attribute(
        "uv",
        lagrange.AttributeUsage.UV,
        np.array([[1, 0], [0.5, 1], [0, 0]], dtype=float),
        np.array([[0, 1, 2]], dtype=np.intc),
    )
    assert mesh.has_attribute("uv")
    return mesh


@pytest.fixture
def cube():
    """A unit cube mesh with quad facets."""
    vertices = np.array(
        [
            [0, 0, 0],
            [1, 0, 0],
            [1, 1, 0],
            [0, 1, 0],
            [0, 0, 1],
            [1, 0, 1],
            [1, 1, 1],
            [0, 1, 1],
        ],
        dtype=float,
    )
    facets = np.array(
        [
            [0, 3, 2, 1],
            [4, 5, 6, 7],
            [1, 2, 6, 5],
            [4, 7, 3, 0],
            [2, 3, 7, 6],
            [0, 1, 5, 4],
        ],
        dtype=np.uint32,
    )
    mesh = lagrange.SurfaceMesh()
    mesh.vertices = vertices
    mesh.facets = facets
    return mesh


@pytest.fixture
def cube_triangular():
    """A unit cube mesh with triangular facets."""
    vertices = np.array(
        [
            [0, 0, 0],
            [1, 0, 0],
            [1, 1, 0],
            [0, 1, 0],
            [0, 0, 1],
            [1, 0, 1],
            [1, 1, 1],
            [0, 1, 1],
        ],
        dtype=float,
    )
    facets = np.array(
        [
            [0, 3, 2],
            [2, 1, 0],
            [4, 5, 6],
            [6, 7, 4],
            [1, 2, 6],
            [6, 5, 1],
            [4, 7, 3],
            [3, 0, 4],
            [2, 3, 7],
            [7, 6, 2],
            [0, 1, 5],
            [5, 4, 0],
        ],
        dtype=np.uint32,
    )
    mesh = lagrange.SurfaceMesh()
    mesh.vertices = vertices
    mesh.facets = facets
    return mesh


@pytest.fixture
def cube_with_uv(cube):
    """A unit cube mesh with UV coordinates."""
    mesh = cube
    mesh.create_attribute(
        "uv",
        lagrange.AttributeElement.Indexed,
        lagrange.AttributeUsage.UV,
        np.array(
            [
                [0.25, 0],
                [0.5, 0],
                [0.25, 0.25],
                [0.5, 0.25],
                [0.25, 0.5],
                [0.5, 0.5],
                [0.25, 0.75],
                [0.5, 0.75],
                [0.25, 1],
                [0.5, 1],
                [0, 0.75],
                [0, 0.5],
                [0.75, 0.75],
                [0.75, 0.5],
            ]
        ),
        np.array(
            [
                [8, 6, 7, 9],
                [2, 3, 5, 4],
                [12, 7, 5, 13],
                [11, 4, 6, 10],
                [7, 6, 4, 5],
                [0, 1, 3, 2],
            ]
        ),
    )
    return mesh


@pytest.fixture
def house():
    """A house-shaped mesh (quad + triangle) with UV coordinates."""
    vertices = np.array(
        [
            [0.25, 0.25, 0.0],
            [0.75, 0.25, 0.0],
            [0.75, 0.75, 0.0],
            [0.25, 0.75, 0.0],
            [0.5, 1.0, 0.0],
        ],
        dtype=float,
    )
    mesh = lagrange.SurfaceMesh()
    mesh.vertices = vertices
    mesh.add_quad(0, 1, 2, 3)
    mesh.add_triangle(3, 2, 4)
    texcoord_values = np.array(
        [
            [0.25, 0.25],
            [0.75, 0.25],
            [0.75, 0.75],
            [0.25, 0.75],
            [0.25, 0.75],
            [0.75, 0.75],
            [0.5, 1.0],
        ],
        dtype=float,
    )
    texcoord_indices = np.arange(len(texcoord_values), dtype=np.uint32)
    texcoord_id = mesh.create_attribute(
        name="texcoord_0",
        element=lagrange.AttributeElement.Indexed,
        usage=lagrange.AttributeUsage.UV,
        initial_values=texcoord_values,
        initial_indices=texcoord_indices,
    )
    lagrange.weld_indexed_attribute(mesh, texcoord_id)
    return mesh


@pytest.fixture
def triangle():
    mesh = lagrange.SurfaceMesh()
    mesh.add_vertices(np.array([[0, 0, 0], [1, 0, 0], [0, 1, 0]], dtype=np.float64))
    mesh.add_triangle(0, 1, 2)
    return mesh


@pytest.fixture
def quad():
    mesh = lagrange.SurfaceMesh()
    mesh.add_vertices(np.array([[0, 0, 0], [1, 0, 0], [1, 1, 0], [0, 1, 0]], dtype=np.float64))
    mesh.add_quad(0, 1, 2, 3)
    return mesh


@pytest.fixture
def triangle_with_attribute(triangle):
    mesh = triangle
    mesh.create_attribute(
        "temperature",
        element=lagrange.AttributeElement.Vertex,
        usage=lagrange.AttributeUsage.Scalar,
        initial_values=np.array([1.0, 2.0, 3.0], dtype=np.float64),
    )
    return mesh


@pytest.fixture
def square():
    """A 2D unit square mesh with triangular facets."""
    vertices = np.array(
        [
            [0, 0],
            [1, 0],
            [1, 1],
            [0, 1],
        ],
        dtype=float,
    )
    facets = np.array(
        [
            [0, 1, 2],
            [2, 3, 0],
        ],
        dtype=np.uint32,
    )
    mesh = lagrange.SurfaceMesh(2)
    mesh.vertices = vertices
    mesh.facets = facets
    return mesh
