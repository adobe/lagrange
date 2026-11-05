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
import gc
import sys
from typing import TypedDict

import lagrange
import numpy as np
import pytest


class _RayBudget(TypedDict):
    num_rays: int
    batch_size: int


class _SamplingOptions(_RayBudget):
    adaptive: bool
    size_influence: float
    confidence: float


class _ApplyOptions(TypedDict):
    visibility_threshold: float
    graphcut_smoothness: float | None
    instancing: lagrange.raycasting.InstancingPolicy


class TestRemoveOccluded:
    def test_remove_occluded_facets_single_cube(self, cube_triangular):
        """A single cube has no occluded facets — all 12 should remain."""
        scene = lagrange.scene.mesh_to_simple_scene(cube_triangular)
        result = lagrange.raycasting.remove_occluded_facets(scene, num_rays=100000)
        assert result.num_meshes == 1
        assert result.get_mesh(0).num_facets == 12

    def test_remove_occluded_facets_plain_cosine(self, cube_triangular):
        """Explicit plain-cosine mode produces the same result on a non-occluded cube."""
        scene = lagrange.scene.mesh_to_simple_scene(cube_triangular)
        result = lagrange.raycasting.remove_occluded_facets(scene, num_rays=100000, adaptive=False)
        assert result.get_mesh(0).num_facets == 12

    @pytest.mark.parametrize("graphcut_smoothness", [None, 0.0, 0.1])
    def test_remove_occluded_facets_graphcut_smoothness(self, cube_triangular, graphcut_smoothness):
        """Graph cutting is optional, and an explicit zero weight remains valid."""
        scene = lagrange.scene.mesh_to_simple_scene(cube_triangular)
        result = lagrange.raycasting.remove_occluded_facets(
            scene,
            num_rays=1000,
            adaptive=False,
            graphcut_smoothness=graphcut_smoothness,
        )
        assert result.num_meshes == 1
        assert result.get_mesh(0).num_facets == 12

    @pytest.mark.parametrize("graphcut_smoothness", [-0.1, np.inf, np.nan])
    def test_remove_occluded_facets_invalid_graphcut_smoothness(
        self, cube_triangular, graphcut_smoothness
    ):
        scene = lagrange.scene.mesh_to_simple_scene(cube_triangular)
        with pytest.raises(RuntimeError, match="smoothness must be finite and non-negative"):
            lagrange.raycasting.remove_occluded_facets(
                scene,
                num_rays=1000,
                adaptive=False,
                graphcut_smoothness=graphcut_smoothness,
            )

    def test_remove_occluded_instances_single_cube(self, cube_triangular):
        """A lone instance is not occluded — should remain."""
        scene = lagrange.scene.mesh_to_simple_scene(cube_triangular)
        result = lagrange.raycasting.remove_occluded_instances(scene, num_rays=100000)
        assert result.num_meshes == 1
        assert result.total_num_instances == 1

    def test_estimate_occluded_instances_single_cube(self, cube_triangular):
        """A lone instance is not occluded — list should be empty."""
        scene = lagrange.scene.mesh_to_simple_scene(cube_triangular)
        occluded = lagrange.raycasting.estimate_occluded_instances(scene, num_rays=100000)
        assert occluded == []

    def test_keyword_only_arguments(self, cube_triangular):
        """Optional arguments must be keyword-only."""
        scene = lagrange.scene.mesh_to_simple_scene(cube_triangular)
        lagrange.raycasting.remove_occluded_facets(scene, num_rays=10000)
        with pytest.raises(TypeError):
            lagrange.raycasting.remove_occluded_facets(scene, 10000)  # type: ignore[ty:too-many-positional-arguments, pyrefly:bad-argument-count]

    def test_facet_until_converged_removed(self, cube_triangular):
        scene = lagrange.scene.mesh_to_simple_scene(cube_triangular)
        with pytest.raises(TypeError):
            lagrange.raycasting.remove_occluded_facets(
                scene,
                num_rays=10_000,
                until_converged=True,  # type: ignore[ty:unknown-argument, pyrefly:unexpected-keyword]
            )

    def test_degenerate_geometry_stops_without_rays(self, degenerate_triangle_scene):
        facets = lagrange.raycasting.remove_occluded_facets(
            degenerate_triangle_scene, num_rays=10_000, batch_size=1_000
        )
        instances = lagrange.raycasting.remove_occluded_instances(
            degenerate_triangle_scene, num_rays=10_000, batch_size=1_000
        )
        assert facets.total_num_instances == 0
        assert instances.total_num_instances == 0


class TestOccludedFacetsFilter:
    @staticmethod
    def make_scene():
        mesh = lagrange.SurfaceMesh()
        mesh.vertices = np.array(
            [[0, 0, 0], [14, 0, 0], [0, 1, 0], [0, 10, 0], [66, 10, 0], [0, 11, 0]],
            dtype=np.float64,
        )
        mesh.facets = np.array([[0, 1, 2], [3, 4, 5]])
        mesh.create_attribute("facet_id", initial_values=np.array([0, 1], dtype=np.uint32))
        return lagrange.scene.mesh_to_simple_scene(mesh)

    @staticmethod
    def prepare(scene, **kwargs) -> lagrange.raycasting.OccludedFacetsFilter:
        return lagrange.raycasting.OccludedFacetsFilter(
            scene,
            num_rays=200,
            batch_size=200,
            adaptive=False,
            size_influence=1.0,
            max_visibility_threshold=0.4,
            **kwargs,
        )

    @pytest.mark.parametrize("weight", [None, 0.0, 0.02])
    def test_repeated_filtering_retains_borrowed_scene(self, weight):
        scene = self.make_scene()
        calls = []

        def is_occluder(mesh_index, instance_index):
            calls.append((mesh_index, instance_index))
            return True

        references_before = sys.getrefcount(scene)
        prepared = self.prepare(scene, is_occluder=is_occluder)
        assert sys.getrefcount(scene) > references_before
        assert isinstance(prepared, lagrange.raycasting.OccludedFacetsFilter)
        assert prepared.max_visibility_threshold == 0.4
        calls_after_prepare = len(calls)
        assert calls_after_prepare > 0
        high = prepared.apply(visibility_threshold=0.4, graphcut_smoothness=weight)
        np.testing.assert_array_equal(high.get_mesh(0).attribute("facet_id").data.ravel(), [1])
        one_shot = lagrange.raycasting.remove_occluded_facets(
            scene,
            num_rays=200,
            batch_size=200,
            adaptive=False,
            size_influence=1.0,
            threshold=0.4,
            graphcut_smoothness=weight,
        )
        np.testing.assert_array_equal(high.get_mesh(0).facets, one_shot.get_mesh(0).facets)
        del scene
        gc.collect()
        low = prepared.apply(visibility_threshold=0.3, graphcut_smoothness=weight)
        assert low.get_mesh(0).num_facets == 2
        np.testing.assert_array_equal(low.get_mesh(0).attribute("facet_id").data.ravel(), [0, 1])
        zero = prepared.apply(visibility_threshold=0.0, graphcut_smoothness=weight)
        assert zero.get_mesh(0).num_facets == 2
        again = prepared.apply(visibility_threshold=0.4)
        assert again.get_mesh(0).num_facets == 1
        assert low.get_mesh(0).num_facets == 2
        assert len(calls) == calls_after_prepare

    @pytest.mark.parametrize("weight", [None, 0.0, 0.02])
    @pytest.mark.parametrize(
        "policy",
        [
            lagrange.raycasting.InstancingPolicy.Max,
            lagrange.raycasting.InstancingPolicy.Average,
            lagrange.raycasting.InstancingPolicy.FlattenInstances,
        ],
    )
    def test_apply_export_modes_and_classification_pruning_consistency(self, weight, policy):
        scene = self.make_scene()
        calls = []

        def is_occluder(mesh_index, instance_index):
            calls.append((mesh_index, instance_index))
            return True

        prepared = self.prepare(scene, is_occluder=is_occluder)
        calls_after_sampling = len(calls)
        parameters: _ApplyOptions = {
            "visibility_threshold": 0.4,
            "graphcut_smoothness": weight,
            "instancing": policy,
        }
        measured = prepared.estimate_visibility("visibility", instancing=policy)
        measures = measured.get_mesh(0).attribute("visibility").data.ravel()
        pruned = prepared.apply(**parameters)
        pruned_mesh = pruned.get_mesh(0)
        np.testing.assert_array_equal(pruned_mesh.attribute("facet_id").data.ravel(), [1])
        for visibility in [False, True]:
            for mask in [False, True]:
                result = prepared.apply(
                    **parameters,
                    visibility_attribute_name="visibility" if visibility else None,
                    keep_attribute_name="keep" if mask else None,
                )
                assert result.num_meshes == 1
                assert result.num_instances(0) == 1
                output = result.get_mesh(0)
                assert output.num_facets == (2 if mask else 1)
                assert output.has_attribute("visibility") == visibility
                assert output.has_attribute("keep") == mask
                np.testing.assert_array_equal(output.vertices, scene.get_mesh(0).vertices)
                if visibility:
                    attribute = output.attribute("visibility")
                    assert attribute.element_type == lagrange.AttributeElement.Facet
                    assert attribute.usage == lagrange.AttributeUsage.Scalar
                    assert attribute.num_channels == 1
                    assert attribute.data.dtype == np.float64
                    assert attribute.data.ndim == 1
                    assert attribute.data.shape == (output.num_facets,)
                    np.testing.assert_array_equal(
                        attribute.data.ravel(), measures if mask else measures[[1]]
                    )
                if mask:
                    attribute = output.attribute("keep")
                    assert attribute.element_type == lagrange.AttributeElement.Facet
                    assert attribute.usage == lagrange.AttributeUsage.Scalar
                    assert attribute.num_channels == 1
                    assert attribute.data.dtype == np.uint8
                    assert attribute.data.ndim == 1
                    assert attribute.data.shape == (output.num_facets,)
                    np.testing.assert_array_equal(attribute.data.ravel(), [0, 1])
                    np.testing.assert_array_equal(output.facets, scene.get_mesh(0).facets)
                    selected = attribute.data.ravel().astype(bool)
                    np.testing.assert_array_equal(output.facets[selected], pruned_mesh.facets)
                    np.testing.assert_array_equal(
                        output.attribute("facet_id").data.ravel()[selected],
                        pruned_mesh.attribute("facet_id").data.ravel(),
                    )
                else:
                    np.testing.assert_array_equal(output.facets, pruned_mesh.facets)
                assert len(calls) == calls_after_sampling
                assert scene.get_mesh(0).num_facets == 2
                assert not scene.get_mesh(0).has_attribute("visibility")
                assert not scene.get_mesh(0).has_attribute("keep")
        assert prepared.apply(visibility_threshold=0.3).get_mesh(0).num_facets == 2
        assert not hasattr(prepared, "filter")

    @staticmethod
    def make_mixed_empty_scene():
        triangle = lagrange.SurfaceMesh()
        triangle.vertices = np.array([[0, 0, 0], [1, 0, 0], [0, 1, 0]], dtype=np.float64)
        triangle.facets = np.array([[0, 1, 2]], dtype=np.uint32)
        scene = lagrange.scene.mesh_to_simple_scene(triangle)
        vertices_only = lagrange.SurfaceMesh()
        vertices_only.vertices = np.array([[2, 3, 4], [5, 6, 7]], dtype=np.float64)
        vertices_only.create_attribute(
            "marker",
            element=lagrange.AttributeElement.Vertex,
            initial_values=np.array([17.0, 23.0], dtype=np.float64),
        )
        vertices_only.create_attribute(
            "empty_collision",
            element=lagrange.AttributeElement.Facet,
            usage=lagrange.AttributeUsage.Scalar,
            dtype=np.float64,
            num_channels=1,
        )
        mesh_index = scene.add_mesh(vertices_only)
        for translation in [5.0, -5.0]:
            instance = lagrange.scene.MeshInstance3D()
            instance.mesh_index = mesh_index
            transform = np.eye(4)
            transform[0, 3] = translation
            instance.transform = transform
            scene.add_instance(instance)
        return scene

    @pytest.mark.parametrize("weight", [None, 0.0, 0.01])
    @pytest.mark.parametrize("visibility", [False, True])
    @pytest.mark.parametrize(
        "policy",
        [
            lagrange.raycasting.InstancingPolicy.Max,
            lagrange.raycasting.InstancingPolicy.Average,
            lagrange.raycasting.InstancingPolicy.FlattenInstances,
        ],
    )
    def test_keep_export_retains_initially_zero_facet_instances(self, weight, visibility, policy):
        scene = self.make_mixed_empty_scene()
        original_vertices = scene.get_mesh(1).vertices.copy()
        original_markers = scene.get_mesh(1).attribute("marker").data.copy()
        original_transforms = [scene.get_instance(1, ii).transform.copy() for ii in range(2)]
        prepared = lagrange.raycasting.OccludedFacetsFilter(
            scene,
            num_rays=100,
            batch_size=100,
            adaptive=False,
            size_influence=0.0,
            max_visibility_threshold=0.2,
        )
        parameters: _ApplyOptions = {
            "visibility_threshold": 0.1,
            "graphcut_smoothness": weight,
            "instancing": policy,
        }
        result = prepared.apply(
            **parameters,
            visibility_attribute_name="visibility" if visibility else None,
            keep_attribute_name="keep",
        )
        assert result.num_meshes == (
            3 if policy == lagrange.raycasting.InstancingPolicy.FlattenInstances else 2
        )
        assert result.total_num_instances == 3
        retained_transforms = []
        for mi in range(result.num_meshes):
            output = result.get_mesh(mi)
            keep = output.attribute("keep").data
            assert keep.dtype == np.uint8
            assert keep.ndim == 1
            assert keep.shape == (output.num_facets,)
            assert output.has_attribute("visibility") == visibility
            if visibility:
                values = output.attribute("visibility").data
                assert values.dtype == np.float64
                assert values.ndim == 1
                assert values.shape == (output.num_facets,)
            if output.num_facets != 0:
                continue
            assert output.num_vertices == 2
            np.testing.assert_array_equal(output.vertices, original_vertices)
            np.testing.assert_array_equal(output.attribute("marker").data, original_markers)
            for ii in range(result.num_instances(mi)):
                retained_transforms.append(result.get_instance(mi, ii).transform.copy())
        assert len(retained_transforms) == 2
        for actual, expected in zip(
            sorted(retained_transforms, key=lambda transform: transform[0, 3]),
            sorted(original_transforms, key=lambda transform: transform[0, 3]),
        ):
            np.testing.assert_array_equal(actual, expected)
        measured = prepared.estimate_visibility("visibility", instancing=policy)
        assert measured.num_meshes == 1
        assert measured.total_num_instances == 1
        assert prepared.estimate_visibility("empty_collision", instancing=policy).num_meshes == 1
        pruned = prepared.apply(
            **parameters, visibility_attribute_name="visibility" if visibility else None
        )
        assert pruned.num_meshes == 1
        assert pruned.total_num_instances == 1
        assert pruned.get_mesh(0).num_facets == 1
        for visibility_name, keep_name in [
            ("empty_collision", "keep"),
            ("visibility", "empty_collision"),
        ]:
            with pytest.raises(RuntimeError, match="already exist"):
                prepared.apply(
                    **parameters,
                    visibility_attribute_name=visibility_name,
                    keep_attribute_name=keep_name,
                )
        assert (
            prepared.apply(**parameters, visibility_attribute_name="empty_collision").num_meshes
            == 1
        )
        assert scene.num_meshes == 2
        assert scene.total_num_instances == 3
        assert scene.get_mesh(0).num_facets == 1
        assert scene.get_mesh(1).num_facets == 0
        np.testing.assert_array_equal(scene.get_mesh(1).vertices, original_vertices)
        np.testing.assert_array_equal(scene.get_mesh(1).attribute("marker").data, original_markers)
        for ii, expected in enumerate(original_transforms):
            np.testing.assert_array_equal(scene.get_instance(1, ii).transform, expected)
        for mi in range(scene.num_meshes):
            assert not scene.get_mesh(mi).has_attribute("visibility")
            assert not scene.get_mesh(mi).has_attribute("keep")
        assert scene.get_mesh(1).attribute("empty_collision").data.shape == (0,)

    @pytest.mark.parametrize(
        ("visibility_name", "keep_name"),
        [("same", "same"), ("facet_id", None), (None, "facet_id"), ("visibility", "facet_id")],
    )
    def test_apply_export_name_collisions(self, visibility_name, keep_name):
        scene = self.make_scene()
        prepared = self.prepare(scene)
        with pytest.raises(RuntimeError, match="distinct|already exist"):
            prepared.apply(
                visibility_threshold=0.4,
                visibility_attribute_name=visibility_name,
                keep_attribute_name=keep_name,
            )
        assert scene.get_mesh(0).num_facets == 2
        np.testing.assert_array_equal(scene.get_mesh(0).attribute("facet_id").data.ravel(), [0, 1])
        assert not scene.get_mesh(0).has_attribute("visibility")
        with pytest.raises(RuntimeError, match="already exist"):
            prepared.estimate_visibility("facet_id")

    def test_apply_present_empty_name_exports(self):
        scene = self.make_scene()
        prepared = self.prepare(scene)
        visible = prepared.apply(visibility_threshold=0.4, visibility_attribute_name="")
        assert visible.get_mesh(0).has_attribute("")
        assert visible.get_mesh(0).num_facets == 1
        assert prepared.estimate_visibility("").get_mesh(0).has_attribute("")
        masked = prepared.apply(visibility_threshold=0.4, keep_attribute_name="")
        assert masked.get_mesh(0).num_facets == 2
        np.testing.assert_array_equal(masked.get_mesh(0).attribute("").data.ravel(), [0, 1])
        with pytest.raises(RuntimeError, match="distinct"):
            prepared.apply(
                visibility_threshold=0.4, visibility_attribute_name="", keep_attribute_name=""
            )
        with pytest.raises(RuntimeError, match="reserved"):
            prepared.apply(visibility_threshold=0.4, visibility_attribute_name="$reserved")
        assert not scene.get_mesh(0).has_attribute("")

    @pytest.mark.parametrize("weight", [None, 0.0, 0.02])
    def test_partial_filtering_preserves_external_source_buffers(self, weight):
        vertices = np.array(
            [[0, 0, 0], [14, 0, 0], [0, 1, 0], [0, 10, 0], [66, 10, 0], [0, 11, 0]],
            dtype=np.float64,
        )
        facets = np.array([[0, 1, 2], [3, 4, 5]], dtype=np.uint32)
        marker = np.array([7.0, 11.0], dtype=np.float64)
        original_vertices = vertices.copy()
        original_facets = facets.copy()
        original_marker = marker.copy()
        mesh = lagrange.SurfaceMesh()
        mesh.vertices = vertices
        mesh.facets = facets
        mesh.create_attribute("marker", initial_values=marker)
        mesh.attribute("marker").data = marker
        mesh.attribute("marker").copy_policy = lagrange.AttributeCopyPolicy.CopyIfExternal
        mesh.attribute(
            mesh.attr_name_corner_to_vertex
        ).copy_policy = lagrange.AttributeCopyPolicy.CopyIfExternal
        scene = lagrange.scene.mesh_to_simple_scene(mesh)
        facet_filter = self.prepare(scene)
        for threshold, expected in [(0.4, [11.0]), (0.3, [7.0, 11.0]), (0.4, [11.0])]:
            result = facet_filter.apply(visibility_threshold=threshold, graphcut_smoothness=weight)
            np.testing.assert_array_equal(
                result.get_mesh(0).attribute("marker").data.ravel(), expected
            )
            np.testing.assert_array_equal(vertices, original_vertices)
            np.testing.assert_array_equal(facets, original_facets)
            np.testing.assert_array_equal(marker, original_marker)

    @pytest.mark.parametrize("weight", [None, 0.0, 0.02])
    @pytest.mark.parametrize("visibility", [False, True])
    @pytest.mark.parametrize("mask", [False, True])
    def test_apply_exports_respect_external_buffer_policies(self, weight, visibility, mask):
        vertices = np.array(
            [[0, 0, 0], [14, 0, 0], [0, 1, 0], [0, 10, 0], [66, 10, 0], [0, 11, 0]],
            dtype=np.float64,
        )
        facets = np.array([[0, 1, 2], [3, 4, 5]], dtype=np.uint32)
        marker = np.array([7.0, 11.0], dtype=np.float64)
        originals = [array.copy() for array in [vertices, facets, marker]]
        mesh = lagrange.SurfaceMesh()
        mesh.vertices = vertices
        mesh.facets = facets
        mesh.create_attribute("marker", initial_values=marker)
        mesh.attribute("marker").data = marker
        for name in [mesh.attr_name_vertex_to_position, mesh.attr_name_corner_to_vertex, "marker"]:
            mesh.attribute(name).copy_policy = lagrange.AttributeCopyPolicy.CopyIfExternal
            mesh.attribute(name).growth_policy = lagrange.AttributeGrowthPolicy.SilentCopy
        scene = lagrange.scene.mesh_to_simple_scene(mesh)
        prepared = self.prepare(scene)
        result = prepared.apply(
            visibility_threshold=0.4,
            graphcut_smoothness=weight,
            visibility_attribute_name="visibility" if visibility else None,
            keep_attribute_name="keep" if mask else None,
        )
        output = result.get_mesh(0)
        assert output.num_facets == (2 if mask else 1)
        np.testing.assert_array_equal(
            output.attribute("marker").data.ravel(), [7.0, 11.0] if mask else [11.0]
        )
        for array, original in zip([vertices, facets, marker], originals):
            np.testing.assert_array_equal(array, original)
        for name in [mesh.attr_name_vertex_to_position, mesh.attr_name_corner_to_vertex, "marker"]:
            attribute = scene.get_mesh(0).attribute(name)
            assert attribute.external
            assert attribute.copy_policy == lagrange.AttributeCopyPolicy.CopyIfExternal
            assert attribute.growth_policy == lagrange.AttributeGrowthPolicy.SilentCopy
        assert not scene.get_mesh(0).has_attribute("visibility")
        assert not scene.get_mesh(0).has_attribute("keep")
        with pytest.raises(RuntimeError, match="already exist"):
            prepared.apply(
                visibility_threshold=0.4,
                visibility_attribute_name="visibility",
                keep_attribute_name="marker",
            )
        np.testing.assert_array_equal(marker, originals[2])
        assert not scene.get_mesh(0).has_attribute("visibility")

    def test_const_member_operations_and_lazy_graph_data(self):
        scene = self.make_scene()
        calls = []

        def is_occluder(mesh_index, instance_index):
            calls.append((mesh_index, instance_index))
            return True

        facet_filter = self.prepare(scene, is_occluder=is_occluder)
        calls_after_construction = len(calls)
        assert calls_after_construction > 0
        measured = facet_filter.estimate_visibility("visibility")
        legacy = lagrange.raycasting.estimate_facet_visibility(
            scene,
            attribute_name="visibility",
            num_rays=200,
            batch_size=200,
            adaptive=False,
            size_influence=1.0,
            threshold=0.4,
        )
        np.testing.assert_array_equal(
            measured.get_mesh(0).attribute("visibility").data,
            legacy.get_mesh(0).attribute("visibility").data,
        )
        for weight in [None, 0.0, 0.02, 0.03, 0.0]:
            result = facet_filter.apply(visibility_threshold=0.4, graphcut_smoothness=weight)
            assert result.get_mesh(0).num_facets == 1
        assert measured.get_mesh(0).num_facets == 2
        assert len(calls) == calls_after_construction
        assert not hasattr(facet_filter, "prepare_graphcut")
        assert not hasattr(lagrange.raycasting, "PreparedOccludedFacets")
        assert not hasattr(lagrange.raycasting, "prepare_occluded_facets")
        assert not hasattr(lagrange.raycasting, "filter_occluded_facets")

    def test_sampling_constructor_keywords(self):
        scene = self.make_scene()
        with pytest.raises(TypeError):
            lagrange.raycasting.OccludedFacetsFilter()  # type: ignore[ty:missing-argument, pyrefly:missing-argument]
        with pytest.raises(TypeError):
            lagrange.raycasting.OccludedFacetsFilter(scene, 200)  # type: ignore[ty:too-many-positional-arguments, pyrefly:bad-argument-count]
        with pytest.raises(TypeError):
            self.prepare(scene, graphcut_smoothness=0.02)
        with pytest.raises(TypeError):
            self.prepare(scene, instancing=lagrange.raycasting.InstancingPolicy.Max)

    @pytest.mark.parametrize(
        "policy",
        [
            lagrange.raycasting.InstancingPolicy.Max,
            lagrange.raycasting.InstancingPolicy.Average,
            lagrange.raycasting.InstancingPolicy.FlattenInstances,
        ],
    )
    def test_measure_instancing(self, shared_mesh_occlusion_scene, policy):
        facet_filter = lagrange.raycasting.OccludedFacetsFilter(
            shared_mesh_occlusion_scene,
            num_rays=10000,
            batch_size=10000,
            adaptive=False,
            size_influence=0.0,
            max_visibility_threshold=0.4,
        )
        measured = facet_filter.estimate_visibility("visibility", instancing=policy)
        legacy = lagrange.raycasting.estimate_facet_visibility(
            shared_mesh_occlusion_scene,
            attribute_name="visibility",
            num_rays=10000,
            batch_size=10000,
            adaptive=False,
            size_influence=0.0,
            threshold=0.4,
            instancing=policy,
        )
        assert measured.num_meshes == legacy.num_meshes
        for mi in range(measured.num_meshes):
            assert measured.num_instances(mi) == legacy.num_instances(mi)
            np.testing.assert_array_equal(
                measured.get_mesh(mi).attribute("visibility").data,
                legacy.get_mesh(mi).attribute("visibility").data,
            )

    @pytest.mark.parametrize("threshold", [-0.1, 0.41, np.inf, np.nan])
    def test_invalid_threshold(self, threshold):
        prepared = self.prepare(self.make_scene())
        with pytest.raises(RuntimeError, match="apply visibility_threshold must be finite"):
            prepared.apply(visibility_threshold=threshold)

    @pytest.mark.parametrize("weight", [-0.1, np.inf, np.nan])
    def test_invalid_weight(self, weight):
        prepared = self.prepare(self.make_scene())
        with pytest.raises(RuntimeError, match="smoothness must be finite"):
            prepared.apply(visibility_threshold=0.4, graphcut_smoothness=weight)

    def test_apply_threshold_required_and_keyword_only(self):
        prepared = self.prepare(self.make_scene())
        with pytest.raises(TypeError):
            prepared.apply()  # type: ignore[ty:missing-argument, pyrefly:missing-argument]
        with pytest.raises(TypeError):
            prepared.apply(keep_attribute_name="keep")  # type: ignore[ty:missing-argument, pyrefly:missing-argument]
        with pytest.raises(TypeError):
            prepared.apply(0.3)  # type: ignore[ty:missing-argument, ty:too-many-positional-arguments, pyrefly:unexpected-positional-argument]
        with pytest.raises(AttributeError):
            prepared.max_visibility_threshold = 0.1  # type: ignore[ty:invalid-assignment, pyrefly:read-only]

    @pytest.mark.parametrize("weight", [None, 0.0, 0.01])
    def test_switch_instancing(self, shared_mesh_occlusion_scene, weight):
        prepared = lagrange.raycasting.OccludedFacetsFilter(
            shared_mesh_occlusion_scene,
            num_rays=10000,
            batch_size=10000,
            adaptive=False,
            size_influence=0.0,
            max_visibility_threshold=0.4,
        )
        for policy, expected in [
            (lagrange.raycasting.InstancingPolicy.Max, 2),
            (lagrange.raycasting.InstancingPolicy.Average, 0),
            (lagrange.raycasting.InstancingPolicy.FlattenInstances, 1),
        ]:
            result = prepared.apply(
                visibility_threshold=0.3, graphcut_smoothness=weight, instancing=policy
            )
            small_instances = sum(
                result.num_instances(mi)
                for mi in range(result.num_meshes)
                if np.ptp(result.get_mesh(mi).vertices, axis=0).max() < 1.0
            )
            assert small_instances == expected
            classified = prepared.apply(
                visibility_threshold=0.3,
                graphcut_smoothness=weight,
                instancing=policy,
                visibility_attribute_name="visibility",
                keep_attribute_name="keep",
            )
            assert classified.num_meshes == (
                3 if policy == lagrange.raycasting.InstancingPolicy.FlattenInstances else 2
            )
            measured = prepared.estimate_visibility("visibility", instancing=policy)
            assert classified.num_meshes == measured.num_meshes
            retained_instances = 0
            selected_instances = 0
            for mi in range(classified.num_meshes):
                output = classified.get_mesh(mi)
                assert output.has_attribute("visibility")
                assert output.has_attribute("keep")
                np.testing.assert_array_equal(
                    output.attribute("visibility").data,
                    measured.get_mesh(mi).attribute("visibility").data,
                )
                if np.ptp(output.vertices, axis=0).max() >= 1.0:
                    continue
                assert output.num_facets == 12
                retained_instances += classified.num_instances(mi)
                keep = output.attribute("keep").data.ravel()
                assert np.all(keep == keep[0])
                if keep[0]:
                    selected_instances += classified.num_instances(mi)
            assert retained_instances == 2
            assert selected_instances == expected
            for mi in range(shared_mesh_occlusion_scene.num_meshes):
                assert not shared_mesh_occlusion_scene.get_mesh(mi).has_attribute("visibility")
                assert not shared_mesh_occlusion_scene.get_mesh(mi).has_attribute("keep")

    def test_connected_weight_changes_reuse_cached_edges(self, cube_triangular):
        mesh = lagrange.SurfaceMesh()
        mesh.vertices = np.array(
            [[-0.5, -0.5, 0.95], [0.5, -0.5, 0.95], [-0.5, 0.5, 1.05]],
            dtype=np.float64,
        )
        mesh.facets = np.array([[0, 1, 2], [0, 2, 1]], dtype=np.uint32)
        mesh.create_attribute("facet_id", initial_values=np.array([0, 1], dtype=np.uint32))
        scene = lagrange.scene.mesh_to_simple_scene(mesh)
        cube = cube_triangular.clone()
        cube.vertices = np.asarray(cube.vertices) * 2.0 - 1.0
        cube_id = scene.add_mesh(cube)
        cube_instance = lagrange.scene.MeshInstance3D()
        cube_instance.mesh_index = cube_id
        scene.add_instance(cube_instance)
        sampling: _SamplingOptions = {
            "num_rays": 1400,
            "batch_size": 1400,
            "adaptive": False,
            "size_influence": 0.0,
            "confidence": np.nextafter(1.0, 0.0),
        }
        max_visibility_threshold = 0.35
        estimated = lagrange.raycasting.estimate_facet_visibility(
            scene, attribute_name="score", threshold=max_visibility_threshold, **sampling
        )
        scores = np.asarray(estimated.get_mesh(0).attribute("score").data).ravel()
        difference = abs(scores[0] - scores[1])
        assert difference > 0
        threshold = float(scores.mean() - difference / 8.0)
        prepared = lagrange.raycasting.OccludedFacetsFilter(
            scene, max_visibility_threshold=max_visibility_threshold, **sampling
        )
        assert prepared.max_visibility_threshold == 0.35
        assert 0 <= threshold <= prepared.max_visibility_threshold

        for weight in [difference / 4.0, difference, difference / 4.0, 0.0, None]:
            result = prepared.apply(visibility_threshold=threshold, graphcut_smoothness=weight)
            ids = np.asarray(result.get_mesh(0).attribute("facet_id").data).ravel()
            expected = [0, 1] if weight == difference else [int(np.argmax(scores))]
            np.testing.assert_array_equal(ids, expected)

    @pytest.mark.parametrize("weight", [None, 0.0, 0.1])
    def test_constructor_retains_scene_and_numpy_storage(self, weight):
        def prepare():
            vertices = np.array([[0, 0, 0], [1, 0, 0], [0, 1, 0]], dtype=np.float64)
            facets = np.array([[0, 1, 2]], dtype=np.uint32)
            expected_vertices = vertices.copy()
            expected_facets = facets.copy()
            mesh = lagrange.SurfaceMesh()
            mesh.vertices = vertices
            mesh.facets = facets
            scene = lagrange.scene.mesh_to_simple_scene(mesh)
            references_before = sys.getrefcount(scene)
            facet_filter = lagrange.raycasting.OccludedFacetsFilter(
                scene, max_visibility_threshold=0.2, num_rays=100, batch_size=100, adaptive=False
            )
            assert sys.getrefcount(scene) > references_before
            measured = facet_filter.estimate_visibility("visibility")
            assert not scene.get_mesh(0).has_attribute("visibility")
            assert measured.get_mesh(0).attribute("visibility").data[0] > 0.2
            result = facet_filter.apply(visibility_threshold=0.1, graphcut_smoothness=weight)
            np.testing.assert_array_equal(result.get_mesh(0).vertices, expected_vertices)
            np.testing.assert_array_equal(result.get_mesh(0).facets, expected_facets)
            np.testing.assert_array_equal(vertices, expected_vertices)
            np.testing.assert_array_equal(facets, expected_facets)
            return facet_filter, expected_vertices, expected_facets

        facet_filter, expected_vertices, expected_facets = prepare()
        gc.collect()
        measured = facet_filter.estimate_visibility("visibility")
        assert measured.get_mesh(0).num_facets == 1
        result = facet_filter.apply(visibility_threshold=0.1, graphcut_smoothness=weight)
        np.testing.assert_array_equal(result.get_mesh(0).vertices, expected_vertices)
        np.testing.assert_array_equal(result.get_mesh(0).facets, expected_facets)

    @pytest.mark.parametrize("weight", [None, 0.0])
    def test_zero_weight_nonmanifold(self, weight):
        num_facets = 1000
        mesh = lagrange.SurfaceMesh()
        angles = np.arange(num_facets) * (2.0 * np.pi / num_facets)
        mesh.vertices = np.vstack(
            (
                [0, 0, 0],
                [1, 0, 0],
                np.column_stack((np.full(num_facets, 0.5), np.cos(angles), np.sin(angles))),
            )
        )
        mesh.facets = np.column_stack(
            (
                np.zeros(num_facets, dtype=np.uint32),
                np.ones(num_facets, dtype=np.uint32),
                np.arange(2, num_facets + 2, dtype=np.uint32),
            )
        )
        prepared = lagrange.raycasting.OccludedFacetsFilter(
            lagrange.scene.mesh_to_simple_scene(mesh),
            num_rays=1000,
            adaptive=False,
            max_visibility_threshold=0.0,
        )
        result = prepared.apply(visibility_threshold=0.0, graphcut_smoothness=weight)
        assert result.get_mesh(0).num_facets == num_facets
        annotated = prepared.apply(
            visibility_threshold=0.0,
            graphcut_smoothness=weight,
            visibility_attribute_name="visibility",
            keep_attribute_name="keep",
        )
        assert annotated.get_mesh(0).num_facets == num_facets
        np.testing.assert_array_equal(
            annotated.get_mesh(0).attribute("keep").data.ravel(),
            np.ones(num_facets, dtype=np.uint8),
        )


# ---------------------------------------------------------------------------
# Nested cubes — minimal occlusion sanity test.
# Outer cube fully encloses inner cube. Inner instance must be culled,
# inner facets must all disappear after facet-level filtering.
# ---------------------------------------------------------------------------


class TestNestedCubes:
    def test_instance_until_converged_keeps_legacy_api(self, nested_cubes_scene):
        """The production instance API keeps convergence mode, now based on retirements."""
        result = lagrange.raycasting.remove_occluded_instances(
            nested_cubes_scene,
            num_rays=0,
            batch_size=20_000,
            until_converged=True,
        )
        assert result.num_meshes == 1
        assert result.total_num_instances == 1

    def test_estimate_occluded_instances(self, nested_cubes_scene):
        """Inner cube has zero escape directions → must be reported as occluded."""
        occluded = lagrange.raycasting.estimate_occluded_instances(
            nested_cubes_scene, num_rays=200_000, batch_size=20_000
        )
        # Mesh 1 (inner cube), instance 0 — that's the only occluded one.
        assert occluded == [(1, 0)]

    def test_remove_occluded_instances(self, nested_cubes_scene):
        """remove_occluded_instances drops the inner instance, keeps the outer."""
        result = lagrange.raycasting.remove_occluded_instances(
            nested_cubes_scene, num_rays=200_000, batch_size=20_000
        )
        assert result.num_meshes == 1
        assert result.total_num_instances == 1
        # The surviving mesh is the outer cube (extent 2 → coords reach ±1).
        kept = result.get_mesh(0)
        assert kept.num_facets == 12
        np.testing.assert_allclose(np.abs(kept.vertices).max(), 1.0)

    def test_remove_occluded_facets(self, nested_cubes_scene):
        """All facets of the inner cube are occluded → inner mesh dropped entirely."""
        result = lagrange.raycasting.remove_occluded_facets(
            nested_cubes_scene, num_rays=2_000_000, batch_size=200_000
        )
        # Outer cube survives with all 12 facets; inner cube's mesh is dropped
        # because every one of its facets failed to escape.
        assert result.num_meshes == 1
        kept = result.get_mesh(0)
        assert kept.num_facets == 12
        np.testing.assert_allclose(np.abs(kept.vertices).max(), 1.0)

    def test_remove_occluded_facets_plain_cosine(self, nested_cubes_scene):
        """The adaptive opt-out reaches the same decision using plain cosine sampling."""
        result = lagrange.raycasting.remove_occluded_facets(
            nested_cubes_scene, num_rays=2_000_000, batch_size=200_000, adaptive=False
        )
        assert result.num_meshes == 1
        kept = result.get_mesh(0)
        assert kept.num_facets == 12
        np.testing.assert_allclose(np.abs(kept.vertices).max(), 1.0)

    def test_remove_occluded_facets_adaptive_options(self, nested_cubes_scene):
        """Non-default adaptive knobs (num_adaptive_per_cosine, vmf_kappa) are accepted and
        still reach the correct decision."""
        result = lagrange.raycasting.remove_occluded_facets(
            nested_cubes_scene,
            num_rays=2_000_000,
            batch_size=200_000,
            adaptive=True,
            num_adaptive_per_cosine=2,
            vmf_kappa=8.0,
        )
        assert result.num_meshes == 1
        assert result.get_mesh(0).num_facets == 12


# ---------------------------------------------------------------------------
# Output fidelity — remove_occluded_instances must return the input geometry
# untouched: instance transforms preserved (not baked into vertices) and user
# normals carried through verbatim.
# ---------------------------------------------------------------------------


class TestInstanceOutputFidelity:
    def test_normals_and_matrices_preserved(self, instanced_normals_scene):
        """Well-separated instances are all kept; their transforms and the mesh's
        indexed "normal" attribute must round-trip unchanged."""
        scene, transforms = instanced_normals_scene
        source = scene.get_mesh(0)
        expected_vertices = np.array(source.vertices)
        source_normal = source.indexed_attribute("normal")
        expected_values = np.array(source_normal.values.data)
        expected_indices = np.array(source_normal.indices.data)

        # size_influence=0 keeps every exposed instance regardless of relative size.
        result = lagrange.raycasting.remove_occluded_instances(
            scene, num_rays=200_000, batch_size=20_000, size_influence=0.0
        )

        # Instancing preserved: one shared mesh, all instances kept, in order.
        assert result.num_meshes == 1
        assert result.total_num_instances == len(transforms)
        for k, expected in enumerate(transforms):
            np.testing.assert_allclose(result.get_instance(0, k).transform, expected)

        # Geometry not baked and normals carried through verbatim.
        kept = result.get_mesh(0)
        np.testing.assert_allclose(kept.vertices, expected_vertices)
        assert kept.has_attribute("normal")
        kept_normal = kept.indexed_attribute("normal")
        assert kept_normal.usage == lagrange.AttributeUsage.Normal
        np.testing.assert_allclose(kept_normal.values.data, expected_values)
        np.testing.assert_array_equal(kept_normal.indices.data, expected_indices)


# ---------------------------------------------------------------------------
# Non-occluder instance flag — same nested-cube setup, but mark the OUTER cube
# as a non-occluder. Rays from the inner cube now pass through the outer cube
# unobstructed and escape to infinity, so the inner cube is no longer culled.
# ---------------------------------------------------------------------------


class TestNonOccluderInstances:
    def test_default_outer_occludes_inner(self, nested_cubes_scene):
        """Baseline: with default occluder semantics, inner cube is culled."""
        occluded = lagrange.raycasting.estimate_occluded_instances(
            nested_cubes_scene, num_rays=200_000, batch_size=20_000
        )
        assert occluded == [(1, 0)]  # inner cube only

    def test_outer_marked_non_occluder_keeps_inner(self, nested_cubes_scene):
        """Marking the outer cube as a non-occluder via the `is_occluder` predicate lets
        the inner cube's rays escape; no instance should be reported as occluded."""
        occluded = lagrange.raycasting.estimate_occluded_instances(
            nested_cubes_scene,
            num_rays=200_000,
            batch_size=20_000,
            is_occluder=lambda mi, ii: (mi, ii) != (0, 0),  # outer cube → non-occluder
        )
        assert occluded == []

    def test_remove_outer_marked_non_occluder_keeps_both(self, nested_cubes_scene):
        """Same but via remove_occluded_instances: both instances survive."""
        result = lagrange.raycasting.remove_occluded_instances(
            nested_cubes_scene,
            num_rays=200_000,
            batch_size=20_000,
            is_occluder=lambda mi, ii: (mi, ii) != (0, 0),
        )
        assert result.num_meshes == 2
        assert result.total_num_instances == 2

    def test_is_occluder_invocation_count(self, nested_cubes_scene):
        """is_occluder should be called exactly once per instance during sampler ctor."""
        calls: list[tuple[int, int]] = []

        def predicate(mi: int, ii: int) -> bool:
            calls.append((mi, ii))
            return True

        lagrange.raycasting.estimate_occluded_instances(
            nested_cubes_scene, num_rays=10_000, batch_size=5_000, is_occluder=predicate
        )
        assert sorted(calls) == [(0, 0), (1, 0)]


# ---------------------------------------------------------------------------
# Estimate-then-filter workflow — the new measure-returning functions let a
# caller re-decide at any threshold <= the sampled one without re-tracing.
# ---------------------------------------------------------------------------


def _kept_instances(measures, threshold):
    return {
        (mi, ii) for mi, row in enumerate(measures) for ii, m in enumerate(row) if m >= threshold
    }


class TestEstimateMeasures:
    def test_instance_remove_matches_measure_threshold(self, far_apart_triangles_scene):
        """A derived mean-visibility threshold above one must not be relaxed internally."""
        threshold = 1e-3
        measures = lagrange.raycasting.estimate_occluded_instance_measures(
            far_apart_triangles_scene,
            num_rays=10_000,
            batch_size=10_000,
            threshold=threshold,
            size_influence=0.5,
        )
        assert all(m < threshold for row in measures for m in row)

        removed = lagrange.raycasting.remove_occluded_instances(
            far_apart_triangles_scene,
            num_rays=10_000,
            batch_size=10_000,
            threshold=threshold,
            size_influence=0.5,
        )
        assert removed.total_num_instances == 0

    def test_facet_remove_matches_measure_threshold(self, mixed_scale_triangles_scene):
        """A small visible facet stays removable when its mean-visibility threshold exceeds one."""
        threshold = 1e-3
        estimated = lagrange.raycasting.estimate_facet_visibility(
            mixed_scale_triangles_scene,
            attribute_name="vis",
            num_rays=10_000,
            batch_size=10_000,
            threshold=threshold,
            size_influence=0.5,
        )
        measures = np.asarray(estimated.get_mesh(0).attribute("vis").data).ravel()
        assert measures[0] >= threshold
        assert measures[1] < threshold

        removed = lagrange.raycasting.remove_occluded_facets(
            mixed_scale_triangles_scene,
            num_rays=10_000,
            batch_size=10_000,
            threshold=threshold,
            size_influence=0.5,
        )
        assert removed.get_mesh(0).num_facets == int((measures >= threshold).sum()) == 1

    def test_instance_measures_shape_and_ordering(self, nested_cubes_scene):
        """measures[mesh][instance]: inner cube's measure is strictly below the outer's."""
        measures = lagrange.raycasting.estimate_occluded_instance_measures(
            nested_cubes_scene, num_rays=200_000, batch_size=20_000
        )
        assert [len(row) for row in measures] == [1, 1]  # one instance per mesh
        outer = measures[0][0]
        inner = measures[1][0]
        assert inner < outer

    def test_instance_measures_monotone_refilter(self, nested_cubes_scene):
        """Re-filtering the measures at a higher threshold yields a nested (subset) kept-set;
        a threshold between inner and outer culls only the inner instance."""
        measures = lagrange.raycasting.estimate_occluded_instance_measures(
            nested_cubes_scene, num_rays=200_000, batch_size=20_000
        )
        outer = measures[0][0]
        inner = measures[1][0]
        kept_low = _kept_instances(measures, 0.0)
        kept_high = _kept_instances(measures, 0.5 * (inner + outer))
        assert kept_high <= kept_low
        assert (1, 0) in kept_low
        assert (1, 0) not in kept_high
        assert (0, 0) in kept_high

    def test_facet_estimate_writes_attribute(self, nested_cubes_scene):
        """estimate_facet_visibility returns a de-instanced scene whose meshes carry the named
        per-facet measure attribute; the outer facets are all above threshold, the inner below."""
        est = lagrange.raycasting.estimate_facet_visibility(
            nested_cubes_scene,
            attribute_name="vis",
            num_rays=2_000_000,
            batch_size=200_000,
        )
        assert est.num_meshes == 2
        # Outer mesh (extent 2 → coords reach ±1) vs inner mesh, identified by vertex extent.
        threshold = 5e-6
        survivors = 0
        for i in range(est.num_meshes):
            mesh = est.get_mesh(i)
            assert mesh.has_attribute("vis")
            vals = np.asarray(mesh.attribute("vis").data).ravel()
            assert len(vals) == mesh.num_facets
            survivors += int((vals >= threshold).sum())
        assert survivors == 12  # only the outer cube's 12 facets clear the threshold

    def test_facet_estimate_matches_remove(self, nested_cubes_scene):
        """Removing facets below threshold on the estimate reproduces remove_occluded_facets."""
        est = lagrange.raycasting.estimate_facet_visibility(
            nested_cubes_scene,
            attribute_name="vis",
            num_rays=2_000_000,
            batch_size=200_000,
        )
        removed = lagrange.raycasting.remove_occluded_facets(
            nested_cubes_scene, num_rays=2_000_000, batch_size=200_000
        )
        survivors = sum(
            int((np.asarray(est.get_mesh(i).attribute("vis").data) >= 5e-6).sum())
            for i in range(est.num_meshes)
        )
        removed_total = sum(removed.get_mesh(i).num_facets for i in range(removed.num_meshes))
        assert survivors == removed_total


class TestFacetInstancingPolicy:
    """Facet de-instancing over a mesh shared by an occluded and a visible instance."""

    _RAYS: _RayBudget = {"num_rays": 2_000_000, "batch_size": 200_000}

    def test_flatten_splits_shared_mesh(self, shared_mesh_occlusion_scene):
        """FlattenInstances splits the shared mesh: exposed survives, enclosed is dropped."""
        policy = lagrange.raycasting.InstancingPolicy.FlattenInstances
        result = lagrange.raycasting.remove_occluded_facets(
            shared_mesh_occlusion_scene, instancing=policy, **self._RAYS
        )
        assert result.num_meshes == 2
        assert result.total_num_instances == 2

    def test_max_keeps_when_any_instance_visible(self, shared_mesh_occlusion_scene):
        """Max keeps the shared mesh: a facet visible in the exposed instance survives for both."""
        policy = lagrange.raycasting.InstancingPolicy.Max
        result = lagrange.raycasting.remove_occluded_facets(
            shared_mesh_occlusion_scene, instancing=policy, **self._RAYS
        )
        assert result.num_meshes == 2
        assert result.total_num_instances == 3
        assert result.get_mesh(1).num_facets == 12

    def test_average_preserves_instancing(self, shared_mesh_occlusion_scene):
        """Average keeps the shared mesh above threshold and preserves both of its instances."""
        policy = lagrange.raycasting.InstancingPolicy.Average
        result = lagrange.raycasting.remove_occluded_facets(
            shared_mesh_occlusion_scene, instancing=policy, **self._RAYS
        )
        assert result.num_meshes == 2
        assert result.total_num_instances == 3
        assert result.get_mesh(1).num_facets == 12

    def test_estimate_average_writes_shared_attribute(self, shared_mesh_occlusion_scene):
        """Average estimate keeps instancing; writes the aggregated measure on the shared mesh."""
        policy = lagrange.raycasting.InstancingPolicy.Average
        est = lagrange.raycasting.estimate_facet_visibility(
            shared_mesh_occlusion_scene, attribute_name="vis", instancing=policy, **self._RAYS
        )
        assert est.num_meshes == 2
        assert est.total_num_instances == 3
        small = est.get_mesh(1)
        assert small.has_attribute("vis")
        vals = np.asarray(small.attribute("vis").data).ravel()
        assert len(vals) == small.num_facets
        assert (vals > 0).all()
