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

from .assets import cube, cube_with_uv

class TestCloseSmallHoles:
    def test_close_small_holes(self, cube):
        mesh = cube
        mesh.remove_facets([0]);
        mesh.initialize_edges()
        assert mesh.num_facets == 5

        lagrange.close_small_holes(mesh, 4, False)
        assert mesh.num_vertices == 8
        assert mesh.num_facets == 6

    def test_close_small_holes_with_uv(self, cube_with_uv):
        mesh = cube_with_uv
        mesh.remove_facets([0]);
        mesh.initialize_edges()
        assert mesh.num_facets == 5
        assert len(mesh.get_matching_attribute_ids(usage=lagrange.AttributeUsage.UV)) == 1

        lagrange.close_small_holes(mesh, 4, False)
        # Note triangles will be used for hole filling due to UV seams.

        assert mesh.num_vertices == 9
        assert mesh.num_facets == 5 + 4 # 5 quads + 4 triangles

        assert len(mesh.get_matching_attribute_ids(usage=lagrange.AttributeUsage.UV)) == 1
