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
def make_mesh_with_indexed_uv():
    def _make(vertices, facets, uv_values, uv_indices):
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

    return _make


class TestComputeUVOrientation:
    def test_no_flips(self, make_mesh_with_indexed_uv):
        mesh = make_mesh_with_indexed_uv(
            vertices=[[0, 0, 0], [1, 0, 0], [0, 1, 0], [1, 1, 0]],
            facets=[[0, 1, 2], [1, 3, 2]],
            uv_values=[[0, 0], [1, 0], [0, 1], [1, 1]],
            uv_indices=[[0, 1, 2], [1, 3, 2]],
        )
        counts = lagrange.compute_uv_orientation(mesh, uv_attribute_name="uv")
        assert counts.positive == 2
        assert counts.degenerate == 0
        assert counts.negative == 0
        orient = mesh.attribute("@uv_orientation").data
        assert np.array_equal(orient, [1, 1])

    def test_all_flipped(self, make_mesh_with_indexed_uv):
        # Both triangles wound CW in UV space.
        mesh = make_mesh_with_indexed_uv(
            vertices=[[0, 0, 0], [1, 0, 0], [0, 1, 0], [1, 1, 0]],
            facets=[[0, 1, 2], [1, 3, 2]],
            uv_values=[[0, 0], [0, 1], [1, 0], [1, 1]],
            uv_indices=[[0, 1, 2], [1, 3, 2]],
        )
        counts = lagrange.compute_uv_orientation(mesh, uv_attribute_name="uv")
        assert counts.negative == 2
        assert counts.positive == 0
        assert counts.degenerate == 0
        orient = mesh.attribute("@uv_orientation").data
        assert np.array_equal(orient, [-1, -1])

    def test_mixed(self, make_mesh_with_indexed_uv):
        # tri0 CCW (positive), tri1 CW (negative) — disconnected charts.
        mesh = make_mesh_with_indexed_uv(
            vertices=[[0, 0, 0], [1, 0, 0], [0, 1, 0], [1, 1, 0]],
            facets=[[0, 1, 2], [1, 3, 2]],
            uv_values=[[0, 0], [1, 0], [0, 1], [2, 0], [2, 1], [3, 0]],
            uv_indices=[[0, 1, 2], [3, 4, 5]],
        )
        counts = lagrange.compute_uv_orientation(mesh, uv_attribute_name="uv")
        assert counts.positive == 1
        assert counts.negative == 1
        assert counts.degenerate == 0
        orient = mesh.attribute("@uv_orientation").data
        assert np.array_equal(orient, [1, -1])

    def test_custom_output_attribute(self, make_mesh_with_indexed_uv):
        mesh = make_mesh_with_indexed_uv(
            vertices=[[0, 0, 0], [1, 0, 0], [0, 1, 0]],
            facets=[[0, 1, 2]],
            uv_values=[[0, 0], [0, 1], [1, 0]],
            uv_indices=[[0, 1, 2]],
        )
        counts = lagrange.compute_uv_orientation(
            mesh, uv_attribute_name="uv", output_attribute_name="@my_orient"
        )
        assert counts.negative == 1
        assert mesh.has_attribute("@my_orient")
        orient = mesh.attribute("@my_orient").data
        assert np.array_equal(orient, [-1])


class TestUnflipUVCharts:
    def test_all_flipped_chart(self, make_mesh_with_indexed_uv):
        mesh = make_mesh_with_indexed_uv(
            vertices=[[0, 0, 0], [1, 0, 0], [0, 1, 0], [1, 1, 0]],
            facets=[[0, 1, 2], [1, 3, 2]],
            uv_values=[[0, 0], [0, 1], [1, 0], [1, 1]],
            uv_indices=[[0, 1, 2], [1, 3, 2]],
        )
        assert lagrange.compute_uv_orientation(mesh, uv_attribute_name="uv").negative == 2
        num_unflipped = lagrange.unflip_uv_charts(mesh, uv_attribute_name="uv")
        assert num_unflipped == 1
        assert lagrange.compute_uv_orientation(mesh, uv_attribute_name="uv").negative == 0
        # U is negated for every UV vertex in the flipped chart; V is unchanged.
        uv_attr = mesh.indexed_attribute("uv")
        assert np.array_equal(uv_attr.values.data, [[0, 0], [0, 1], [-1, 0], [-1, 1]])
        assert np.array_equal(uv_attr.indices.data, [0, 1, 2, 1, 3, 2])

    def test_only_flipped_chart_is_unflipped(self, make_mesh_with_indexed_uv):
        mesh = make_mesh_with_indexed_uv(
            vertices=[[0, 0, 0], [1, 0, 0], [0, 1, 0], [1, 1, 0]],
            facets=[[0, 1, 2], [1, 3, 2]],
            uv_values=[[0, 0], [1, 0], [0, 1], [2, 0], [2, 1], [3, 0]],
            uv_indices=[[0, 1, 2], [3, 4, 5]],
        )
        assert lagrange.compute_uv_orientation(mesh, uv_attribute_name="uv").negative == 1
        num_unflipped = lagrange.unflip_uv_charts(mesh, uv_attribute_name="uv")
        assert num_unflipped == 1
        assert lagrange.compute_uv_orientation(mesh, uv_attribute_name="uv").negative == 0
        # Chart A (UV verts 0..2) untouched; Chart B (UV verts 3..5) has negated U.
        uv_attr = mesh.indexed_attribute("uv")
        assert np.array_equal(
            uv_attr.values.data,
            [[0, 0], [1, 0], [0, 1], [-2, 0], [-2, 1], [-3, 0]],
        )

    def test_no_flipped_charts(self, make_mesh_with_indexed_uv):
        mesh = make_mesh_with_indexed_uv(
            vertices=[[0, 0, 0], [1, 0, 0], [0, 1, 0], [1, 1, 0]],
            facets=[[0, 1, 2], [1, 3, 2]],
            uv_values=[[0, 0], [1, 0], [0, 1], [1, 1]],
            uv_indices=[[0, 1, 2], [1, 3, 2]],
        )
        num_unflipped = lagrange.unflip_uv_charts(mesh, uv_attribute_name="uv")
        assert num_unflipped == 0

    def test_flipped_with_degenerate(self, make_mesh_with_indexed_uv):
        # Two triangles in one chart: one flipped (CW) and one degenerate (zero area).
        # The chart has no positively-oriented triangles, so it should be unflipped.
        mesh = make_mesh_with_indexed_uv(
            vertices=[[0, 0, 0], [1, 0, 0], [0, 1, 0], [1, 1, 0]],
            facets=[[0, 1, 2], [1, 3, 2]],
            uv_values=[[0, 0], [0, 1], [1, 0], [1, 1]],
            uv_indices=[[0, 1, 2], [1, 1, 3]],
        )
        counts = lagrange.compute_uv_orientation(mesh, uv_attribute_name="uv")
        assert counts.negative == 1
        assert counts.degenerate == 1
        assert counts.positive == 0
        num_unflipped = lagrange.unflip_uv_charts(mesh, uv_attribute_name="uv")
        assert num_unflipped == 1
        post = lagrange.compute_uv_orientation(mesh, uv_attribute_name="uv")
        assert post.negative == 0

    def test_with_explicit_chart_id_attribute(self, make_mesh_with_indexed_uv):
        # Force both triangles into a single chart via an explicit chart-id attribute,
        # even though they have disconnected UVs. The chart's total signed area drives
        # the decision: tri0 is CCW (+0.5), tri1 is CW (-0.5), so the chart's sum is 0
        # and is NOT unflipped.
        mesh = make_mesh_with_indexed_uv(
            vertices=[[0, 0, 0], [1, 0, 0], [0, 1, 0], [1, 1, 0]],
            facets=[[0, 1, 2], [1, 3, 2]],
            uv_values=[[0, 0], [1, 0], [0, 1], [2, 0], [2, 1], [3, 0]],
            uv_indices=[[0, 1, 2], [3, 4, 5]],
        )
        mesh.create_attribute(
            "@my_chart",
            element=lagrange.AttributeElement.Facet,
            usage=lagrange.AttributeUsage.Scalar,
            initial_values=np.array([0, 0], dtype=np.uint32),
        )
        num_unflipped = lagrange.unflip_uv_charts(
            mesh, uv_attribute_name="uv", chart_id_attribute_name="@my_chart"
        )
        assert num_unflipped == 0
