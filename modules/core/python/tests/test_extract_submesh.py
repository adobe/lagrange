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


class TestExtractSubmesh:
    def test_extract_submesh_with_tensor(self, cube):
        """Test extract_submesh with Tensor input (existing behavior)."""
        mesh = cube
        # Select first 3 facets using numpy array (automatically converted to Tensor)
        selected_facets = np.array([0, 1, 2], dtype=np.uint32)

        submesh = lagrange.extract_submesh(
            mesh,
            selected_facets,
            source_vertex_attr_name="source_vertex",
            source_facet_attr_name="source_facet",
            map_attributes=True,
        )

        assert submesh.num_facets == 3
        assert submesh.num_vertices <= mesh.num_vertices
        assert submesh.has_attribute("source_vertex")
        assert submesh.has_attribute("source_facet")

        # Verify the source facet mapping
        source_facet = submesh.attribute("source_facet").data
        assert np.array_equal(source_facet, [0, 1, 2])

    def test_extract_submesh_with_list(self, cube):
        """Test extract_submesh with Python list input (new behavior)."""
        mesh = cube
        # Select first 3 facets using Python list
        selected_facets = [0, 1, 2]

        submesh = lagrange.extract_submesh(
            mesh,
            selected_facets,
            source_vertex_attr_name="source_vertex",
            source_facet_attr_name="source_facet",
            map_attributes=True,
        )

        assert submesh.num_facets == 3
        assert submesh.num_vertices <= mesh.num_vertices
        assert submesh.has_attribute("source_vertex")
        assert submesh.has_attribute("source_facet")

        # Verify the source facet mapping
        source_facet = submesh.attribute("source_facet").data
        assert np.array_equal(source_facet, [0, 1, 2])

    def test_extract_submesh_tensor_vs_list(self, cube):
        """Verify that tensor and list inputs produce identical results."""
        mesh = cube
        selected_indices = [0, 2, 4]

        # Extract using numpy array (tensor)
        submesh_tensor = lagrange.extract_submesh(
            mesh,
            np.array(selected_indices, dtype=np.uint32),
            source_vertex_attr_name="source_vertex",
            source_facet_attr_name="source_facet",
            map_attributes=True,
        )

        # Extract using Python list
        submesh_list = lagrange.extract_submesh(
            mesh,
            selected_indices,
            source_vertex_attr_name="source_vertex",
            source_facet_attr_name="source_facet",
            map_attributes=True,
        )

        # Both should produce identical results
        assert submesh_tensor.num_facets == submesh_list.num_facets
        assert submesh_tensor.num_vertices == submesh_list.num_vertices
        assert np.allclose(submesh_tensor.vertices, submesh_list.vertices)
        assert np.array_equal(submesh_tensor.facets, submesh_list.facets)

        # Verify source attributes match
        source_facet_tensor = submesh_tensor.attribute("source_facet").data
        source_facet_list = submesh_list.attribute("source_facet").data
        assert np.array_equal(source_facet_tensor, source_facet_list)

    def test_extract_submesh_with_attributes(self, cube_with_uv):
        """Test extract_submesh preserves and maps attributes correctly."""
        mesh = cube_with_uv
        assert mesh.has_attribute("uv")

        # Select subset of facets using list
        selected_facets = [0, 1, 2]

        submesh = lagrange.extract_submesh(
            mesh,
            selected_facets,
            source_vertex_attr_name="source_vertex",
            source_facet_attr_name="source_facet",
            map_attributes=True,
        )

        assert submesh.num_facets == 3
        assert submesh.has_attribute("source_vertex")
        assert submesh.has_attribute("source_facet")
        assert submesh.has_attribute("uv")
        assert submesh.is_attribute_indexed("uv")

    def test_extract_submesh_single_facet_list(self, cube):
        """Test extract_submesh with a single facet in a list."""
        mesh = cube
        selected_facets = [3]

        submesh = lagrange.extract_submesh(
            mesh,
            selected_facets,
            source_facet_attr_name="source_facet",
        )

        assert submesh.num_facets == 1
        assert submesh.has_attribute("source_facet")
        source_facet = submesh.attribute("source_facet").data
        assert source_facet[0] == 3

    def test_extract_submesh_all_facets_list(self, cube):
        """Test extract_submesh with all facets using a list."""
        mesh = cube
        selected_facets = list(range(mesh.num_facets))

        submesh = lagrange.extract_submesh(
            mesh,
            selected_facets,
            map_attributes=True,
        )

        assert submesh.num_facets == mesh.num_facets
        assert submesh.num_vertices == mesh.num_vertices

    def test_extract_submesh_empty_list(self, cube):
        """Test extract_submesh with an empty list."""
        mesh = cube
        selected_facets = []

        submesh = lagrange.extract_submesh(
            mesh,
            selected_facets,
        )

        assert submesh.num_facets == 0
        assert submesh.num_vertices == 0
