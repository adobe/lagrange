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
    """Factory fixture to create a mesh with an indexed UV attribute."""

    def _factory(vertices, facets, uv_values, uv_indices, name="uv"):
        mesh = lagrange.SurfaceMesh()
        mesh.add_vertices(np.array(vertices, dtype=np.float64))
        for f in facets:
            mesh.add_triangle(*f)
        mesh.create_attribute(
            name,
            element=lagrange.AttributeElement.Indexed,
            usage=lagrange.AttributeUsage.UV,
            initial_values=np.array(uv_values, dtype=np.float64),
            initial_indices=np.array(uv_indices, dtype=np.uint32),
        )
        return mesh

    return _factory


def _uv_bbox(mesh, name="uv"):
    """Return (min_u, min_v, max_u, max_v) over referenced UV values."""
    attr = mesh.indexed_attribute(name)
    values = np.asarray(attr.values.data)
    indices = np.asarray(attr.indices.data).flatten()
    used = values[indices]
    return used[:, 0].min(), used[:, 1].min(), used[:, 0].max(), used[:, 1].max()


def _assert_no_overlap(mesh, name="uv"):
    """Assert that the mesh's UV charts do not overlap."""
    result = lagrange.bvh.compute_uv_overlap(mesh, uv_attribute_name=name)
    assert not result.has_overlap


class TestRepackUVCharts:
    def test_single_triangle_normalized_to_unit_square(self, make_mesh_with_indexed_uv):
        # UVs span [0, 2] x [0, 2] -> should be normalized into ~[0, 1].
        mesh = make_mesh_with_indexed_uv(
            vertices=[[0, 0, 0], [1, 0, 0], [0, 1, 0]],
            facets=[[0, 1, 2]],
            uv_values=[[0, 0], [2, 0], [0, 2]],
            uv_indices=[[0, 1, 2]],
        )
        lagrange.packing.repack_uv_charts(mesh)

        min_u, min_v, max_u, max_v = _uv_bbox(mesh)
        assert abs(min_u) < 1e-6
        assert abs(min_v) < 1e-6
        assert 0.99 < max_u <= 1.0 - 1e-6
        assert 0.99 < max_v <= 1.0 - 1e-6

    def test_two_disconnected_charts_fit_unit_square(self, make_mesh_with_indexed_uv):
        mesh = make_mesh_with_indexed_uv(
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
        lagrange.packing.repack_uv_charts(mesh)

        min_u, min_v, max_u, max_v = _uv_bbox(mesh)
        assert abs(min_u) < 1e-6
        assert abs(min_v) < 1e-6
        assert max_u <= 1.0 - 1e-6
        assert max_v <= 1.0 - 1e-6

        # Repacked charts should not overlap each other.
        _assert_no_overlap(mesh)

    def test_with_chart_attribute_name(self, make_mesh_with_indexed_uv):
        """Force two triangles into separate charts via a chart_id attribute."""
        mesh = make_mesh_with_indexed_uv(
            vertices=[[0, 0, 0], [1, 0, 0], [0, 1, 0], [1, 1, 0]],
            facets=[[0, 1, 2], [1, 3, 2]],
            uv_values=[[0, 0], [1, 0], [0, 1], [1, 1]],
            uv_indices=[[0, 1, 2], [1, 3, 2]],
        )
        mesh.create_attribute(
            "@chart_id",
            element=lagrange.AttributeElement.Facet,
            usage=lagrange.AttributeUsage.Scalar,
            initial_values=np.array([0, 1], dtype=np.uint32),
        )
        lagrange.packing.repack_uv_charts(mesh, chart_attribute_name="@chart_id")

        min_u, min_v, max_u, max_v = _uv_bbox(mesh)
        assert min_u >= -1e-6
        assert min_v >= -1e-6
        assert max_u <= 1.0 + 1e-6
        assert max_v <= 1.0 + 1e-6

        # The two forced-apart charts should not overlap after repacking.
        _assert_no_overlap(mesh)

    def test_named_uv_attribute(self, make_mesh_with_indexed_uv):
        """Explicit uv_attribute_name argument should route to the right attribute."""
        mesh = make_mesh_with_indexed_uv(
            vertices=[[0, 0, 0], [1, 0, 0], [0, 1, 0]],
            facets=[[0, 1, 2]],
            uv_values=[[10, 10], [12, 10], [10, 12]],
            uv_indices=[[0, 1, 2]],
            name="my_uv",
        )
        lagrange.packing.repack_uv_charts(mesh, uv_attribute_name="my_uv")

        min_u, min_v, max_u, max_v = _uv_bbox(mesh, name="my_uv")
        assert abs(min_u) < 1e-6
        assert abs(min_v) < 1e-6
        assert max_u <= 1.0 + 1e-6
        assert max_v <= 1.0 + 1e-6

    def test_normalize_false_preserves_scale(self, make_mesh_with_indexed_uv):
        """With normalize=False, output should preserve the original chart scale."""
        mesh = make_mesh_with_indexed_uv(
            vertices=[[0, 0, 0], [1, 0, 0], [0, 1, 0]],
            facets=[[0, 1, 2]],
            uv_values=[[0, 0], [5, 0], [0, 5]],
            uv_indices=[[0, 1, 2]],
        )
        lagrange.packing.repack_uv_charts(mesh, normalize=False)

        min_u, min_v, max_u, max_v = _uv_bbox(mesh)
        # Min is still shifted to the origin, and the 5x5 chart extent is preserved.
        assert np.isclose(min_u, 0.0, atol=1e-6)
        assert np.isclose(min_v, 0.0, atol=1e-6)
        assert np.isclose(max_u - min_u, 5.0, atol=1e-6)
        assert np.isclose(max_v - min_v, 5.0, atol=1e-6)

    def test_margin_shrinks_packed_region(self, make_mesh_with_indexed_uv):
        """Larger margin should leave more whitespace around the packed chart."""
        mesh = make_mesh_with_indexed_uv(
            vertices=[[0, 0, 0], [1, 0, 0], [0, 1, 0]],
            facets=[[0, 1, 2]],
            uv_values=[[0, 0], [1, 0], [0, 1]],
            uv_indices=[[0, 1, 2]],
        )
        lagrange.packing.repack_uv_charts(mesh, margin=0.1)

        _, _, max_u, max_v = _uv_bbox(mesh)
        assert max_u <= 1.0 + 1e-6
        assert max_v <= 1.0 + 1e-6
        # A 0.1 margin should visibly shrink the chart.
        assert max_u < 0.95 or max_v < 0.95

    def test_arguments_after_mesh_are_keyword_only(self, make_mesh_with_indexed_uv):
        """All arguments after ``mesh`` must be passed as keyword arguments."""
        mesh = make_mesh_with_indexed_uv(
            vertices=[[0, 0, 0], [1, 0, 0], [0, 1, 0]],
            facets=[[0, 1, 2]],
            uv_values=[[0, 0], [1, 0], [0, 1]],
            uv_indices=[[0, 1, 2]],
        )
        with pytest.raises(TypeError):
            lagrange.packing.repack_uv_charts(mesh, "uv")  # ty: ignore[too-many-positional-arguments]
