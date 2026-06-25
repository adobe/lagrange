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


def test_repack_mesh_kwargs(unwrapped_two_triangle_quad):
    out = lagrange.xatlas.repack_mesh(
        unwrapped_two_triangle_quad,
        input_uv_attribute_name="uv",
        output_uv_attribute_name="uv2",
        padding=2,
    )
    assert out.num_vertices == unwrapped_two_triangle_quad.num_vertices
    assert out.num_facets == unwrapped_two_triangle_quad.num_facets
    assert unwrapped_two_triangle_quad.has_attribute("uv")
    assert not unwrapped_two_triangle_quad.has_attribute("uv2")
    assert out.has_attribute("uv")
    assert out.has_attribute("uv2")
