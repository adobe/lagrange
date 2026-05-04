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


class TestRemoveInteriorShells:
    def test_remove_interior_shells(self, cube):
        mesh = cube.clone()

        # Create an interior shell by duplicating and scaling down the cube
        interior_shell = cube.clone()
        interior_shell.vertices = 0.5 * (interior_shell.vertices - 0.5) + 0.5
        mesh = lagrange.combine_meshes([mesh, interior_shell])

        assert mesh.num_facets == 24  # 12 facets per cube

        cleaned_mesh = lagrange.bvh.remove_interior_shells(mesh)

        assert cleaned_mesh.num_facets == 12  # Only the outer shell remains
