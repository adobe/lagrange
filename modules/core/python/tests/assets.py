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


@pytest.fixture
def single_triangle():
    mesh = lagrange.SurfaceMesh()
    mesh.add_vertices(np.eye(3))
    mesh.add_triangle(0, 1, 2)
    assert mesh.num_vertices == 3
    assert mesh.num_facets == 1
    return mesh


@pytest.fixture
def single_triangle_with_index(single_triangle):
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
def cube_with_uv(cube):
    mesh = cube
    id = mesh.create_attribute(
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
    uv_attr = mesh.indexed_attribute(id)
    #uv_attr.values.create_internal_copy()
    #uv_attr.indices.create_internal_copy()
    #uv_attr.values.growth_policy = lagrange.AttributeGrowthPolicy.WarnAndCopy
    #uv_attr.indices.growth_policy = lagrange.AttributeGrowthPolicy.WarnAndCopy
    return mesh
