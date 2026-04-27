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


def _make_mesh_with_indexed_uv(vertices, facets, uv_values, uv_indices):
    """Helper to create a mesh with an indexed UV attribute."""
    mesh = lagrange.SurfaceMesh()
    mesh.add_vertices(np.array(vertices, dtype=np.float64))
    for f in facets:
        mesh.add_triangle(*f)
    mesh.create_attribute(
        "uv",
        element=lagrange.AttributeElement.Indexed,
        usage=lagrange.AttributeUsage.UV,
        initial_values=np.array(uv_values, dtype=np.float64),
        initial_indices=np.array(uv_indices, dtype=np.uint32),
    )
    return mesh


class TestDisconnectUVCharts:
    def test_single_chart_no_duplicates(self):
        """Two triangles sharing an edge in UV space -> single chart, no duplicates."""
        mesh = _make_mesh_with_indexed_uv(
            vertices=[[0, 0, 0], [1, 0, 0], [0, 1, 0], [1, 1, 0]],
            facets=[[0, 1, 2], [1, 3, 2]],
            uv_values=[[0, 0], [1, 0], [0, 1], [1, 1]],
            uv_indices=[[0, 1, 2], [1, 3, 2]],
        )
        num_duped = lagrange.disconnect_uv_charts(mesh, uv_attribute_name="uv")
        assert num_duped == 0
        attr = mesh.indexed_attribute("uv")
        assert attr.values.num_elements == 4

    def test_two_separate_charts(self):
        """Two triangles with completely separate UV indices -> no shared UV vertex."""
        mesh = _make_mesh_with_indexed_uv(
            vertices=[[0, 0, 0], [1, 0, 0], [0, 1, 0], [1, 1, 0]],
            facets=[[0, 1, 2], [1, 3, 2]],
            uv_values=[[0, 0], [1, 0], [0, 1], [0.5, 0], [1, 0.5], [0.5, 1]],
            uv_indices=[[0, 1, 2], [3, 4, 5]],
        )
        num_duped = lagrange.disconnect_uv_charts(mesh, uv_attribute_name="uv")
        assert num_duped == 0
        attr = mesh.indexed_attribute("uv")
        assert attr.values.num_elements == 6

    def test_pinch_point(self):
        """Two triangles sharing a single UV vertex (pinch point) -> one duplicate."""
        mesh = _make_mesh_with_indexed_uv(
            vertices=[[0, 0, 0], [1, 0, 0], [0.5, 0.5, 0], [0, 1, 0], [1, 1, 0]],
            facets=[[0, 1, 2], [2, 3, 4]],
            uv_values=[[0, 0], [1, 0], [0.5, 0.5], [0, 1], [1, 1]],
            uv_indices=[[0, 1, 2], [2, 3, 4]],
        )
        num_duped = lagrange.disconnect_uv_charts(mesh, uv_attribute_name="uv")
        assert num_duped == 1
        attr = mesh.indexed_attribute("uv")
        assert attr.values.num_elements == 6  # 5 original + 1 duplicate

        # The two triangles should no longer share any UV index.
        indices = attr.indices.data.flatten()
        tri0 = set(indices[:3])
        tri1 = set(indices[3:6])
        assert tri0.isdisjoint(tri1)

    def test_three_charts_at_single_vertex(self):
        """Three triangles meeting at a single UV vertex -> 2 duplicates."""
        mesh = _make_mesh_with_indexed_uv(
            vertices=[
                [0, 0, 0],
                [1, 0, 0],
                [0.5, 1, 0],
                [-1, 0, 0],
                [-0.5, 1, 0],
                [0, -1, 0],
                [1, -1, 0],
            ],
            facets=[[0, 1, 2], [0, 3, 4], [0, 5, 6]],
            uv_values=[
                [0, 0],
                [1, 0],
                [0.5, 1],
                [-1, 0],
                [-0.5, 1],
                [0, -1],
                [1, -1],
            ],
            uv_indices=[[0, 1, 2], [0, 3, 4], [0, 5, 6]],
        )
        num_duped = lagrange.disconnect_uv_charts(mesh, uv_attribute_name="uv")
        assert num_duped == 2
        attr = mesh.indexed_attribute("uv")
        assert attr.values.num_elements == 9  # 7 + 2

    def test_with_chart_id_attribute(self):
        """Force separate charts via an external chart_id attribute."""
        mesh = _make_mesh_with_indexed_uv(
            vertices=[[0, 0, 0], [1, 0, 0], [0, 1, 0], [1, 1, 0]],
            facets=[[0, 1, 2], [1, 3, 2]],
            uv_values=[[0, 0], [1, 0], [0, 1], [1, 1]],
            uv_indices=[[0, 1, 2], [1, 3, 2]],
        )
        # Force different chart ids despite shared edge.
        mesh.create_attribute(
            "@chart_id",
            element=lagrange.AttributeElement.Facet,
            usage=lagrange.AttributeUsage.Scalar,
            initial_values=np.array([0, 1], dtype=np.uint32),
        )
        num_duped = lagrange.disconnect_uv_charts(
            mesh,
            uv_attribute_name="uv",
            chart_id_attribute_name="@chart_id",
        )
        assert num_duped == 2  # UV vertices 1 and 2 are shared
        attr = mesh.indexed_attribute("uv")
        assert attr.values.num_elements == 6  # 4 + 2

    def test_uv_values_preserved(self):
        """Check that duplicated UV values match the original."""
        pinch_uv = [0.5, 0.5]
        mesh = _make_mesh_with_indexed_uv(
            vertices=[[0, 0, 0], [1, 0, 0], [0.5, 0.5, 0], [0, 1, 0], [1, 1, 0]],
            facets=[[0, 1, 2], [2, 3, 4]],
            uv_values=[[0, 0], [1, 0], pinch_uv, [0, 1], [1, 1]],
            uv_indices=[[0, 1, 2], [2, 3, 4]],
        )
        lagrange.disconnect_uv_charts(mesh, uv_attribute_name="uv")

        attr = mesh.indexed_attribute("uv")
        values = attr.values.data
        indices = attr.indices.data.flatten()

        # The pinch point was at corner 2 of tri0 and corner 0 of tri1.
        idx_tri0 = indices[2]
        idx_tri1 = indices[3]
        assert idx_tri0 != idx_tri1
        np.testing.assert_allclose(values[idx_tri0], pinch_uv)
        np.testing.assert_allclose(values[idx_tri1], pinch_uv)

    def test_already_separated_noop(self):
        """Charts that are already separated should not change."""
        mesh = _make_mesh_with_indexed_uv(
            vertices=[
                [0, 0, 0],
                [1, 0, 0],
                [0.5, 1, 0],
                [2, 0, 0],
                [3, 0, 0],
                [2.5, 1, 0],
            ],
            facets=[[0, 1, 2], [3, 4, 5]],
            uv_values=[[0, 0], [1, 0], [0.5, 1], [2, 0], [3, 0], [2.5, 1]],
            uv_indices=[[0, 1, 2], [3, 4, 5]],
        )
        indices_before = mesh.indexed_attribute("uv").indices.data.copy()
        num_duped = lagrange.disconnect_uv_charts(mesh, uv_attribute_name="uv")
        assert num_duped == 0
        np.testing.assert_array_equal(mesh.indexed_attribute("uv").indices.data, indices_before)

    def test_default_uv_attribute_name(self):
        """When no uv_attribute_name is given, auto-detect the first indexed UV."""
        mesh = _make_mesh_with_indexed_uv(
            vertices=[[0, 0, 0], [1, 0, 0], [0.5, 0.5, 0], [0, 1, 0], [1, 1, 0]],
            facets=[[0, 1, 2], [2, 3, 4]],
            uv_values=[[0, 0], [1, 0], [0.5, 0.5], [0, 1], [1, 1]],
            uv_indices=[[0, 1, 2], [2, 3, 4]],
        )
        # Call without specifying uv_attribute_name
        num_duped = lagrange.disconnect_uv_charts(mesh)
        assert num_duped == 1
