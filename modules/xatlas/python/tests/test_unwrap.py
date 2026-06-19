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


def test_unwrap_mesh_default_kwargs(two_triangle_quad):
    out = lagrange.xatlas.unwrap_mesh(two_triangle_quad)
    assert out.num_vertices == two_triangle_quad.num_vertices
    assert out.num_facets == two_triangle_quad.num_facets
    assert out.has_attribute("@uv")


def test_unwrap_mesh_custom_kwargs(two_triangle_quad):
    out = lagrange.xatlas.unwrap_mesh(
        two_triangle_quad,
        output_uv_attribute_name="uv",
        max_iterations=2,
        padding=4,
        resolution=512,
    )
    assert out.num_vertices == two_triangle_quad.num_vertices
    assert out.num_facets == two_triangle_quad.num_facets
    assert out.has_attribute("uv")


def test_unwrap_mesh_atlas_attribute(two_triangle_quad):
    out = lagrange.xatlas.unwrap_mesh(
        two_triangle_quad,
        output_atlas_attribute_name="uv_atlas",
    )
    assert out.num_vertices == two_triangle_quad.num_vertices
    assert out.num_facets == two_triangle_quad.num_facets
    assert out.has_attribute("uv_atlas")


def test_unwrap_mesh_chart_attribute(two_triangle_quad):
    out = lagrange.xatlas.unwrap_mesh(
        two_triangle_quad,
        output_chart_attribute_name="uv_chart",
    )
    assert out.num_vertices == two_triangle_quad.num_vertices
    assert out.num_facets == two_triangle_quad.num_facets
    assert out.has_attribute("uv_chart")


def test_unwrap_mesh_multi_atlas_policy(two_triangle_quad):
    # Small mesh fits in one atlas; ErrorIfMultiple should not raise.
    out = lagrange.xatlas.unwrap_mesh(
        two_triangle_quad,
        multi_atlas_policy=lagrange.xatlas.MultiAtlasPolicy.ErrorIfMultiple,
    )
    assert out.num_vertices == two_triangle_quad.num_vertices
    assert out.num_facets == two_triangle_quad.num_facets
    assert out.has_attribute("@uv")
