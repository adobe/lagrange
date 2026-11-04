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
"""Integration tests for Python view-error conversion, callbacks, and source ownership."""

import gc
import sys

import lagrange
import numpy as np
import pytest


def _triangle_scene():
    mesh = lagrange.SurfaceMesh()
    mesh.vertices = np.array([[0, 0, 0], [1, 0, 0], [0, 1, 0]], dtype=np.float64)
    mesh.facets = np.array([[0, 1, 2]], dtype=np.uint32)
    return lagrange.scene.mesh_to_simple_scene(mesh)


def _prepare(scene, **kwargs):
    return lagrange.raycasting.OccludedFacetsFilter(
        scene, num_rays=200, batch_size=200, adaptive=False, **kwargs
    )


@pytest.mark.parametrize("outcome", ["failed_baseline", "no_removal", "free_removal"])
def test_result_describes_returned_geometry(outcome):
    scene = _triangle_scene()
    prepared = _prepare(scene)

    def evaluate(keep, views):
        if outcome == "failed_baseline":
            return [0.02, 0.1]
        if outcome == "free_removal":
            return [0.0, 0.0]
        return [0.005, 0.0] if keep[0] else [1.0, 1.0]

    output, result = prepared.apply_with_view_error(
        evaluate,
        num_views=2,
        max_view_error=0 if outcome == "free_removal" else 0.01,
        num_threshold_iterations=4,
        smoothness_tradeoff=None,
    )
    assert result.within_error_budget == (outcome != "failed_baseline")
    assert (
        result.worst_view_error
        == {
            "failed_baseline": 0.1,
            "no_removal": 0.005,
            "free_removal": 0.0,
        }[outcome]
    )
    if outcome == "free_removal":
        assert output.num_meshes == 0
        assert result.removed_fraction == 1
        assert result.num_isolated_facets == 0
        assert result.threshold > prepared.max_visibility_threshold
    else:
        assert output.get_mesh(0).num_facets == 1
        assert result.removed_fraction == 0
        assert result.num_isolated_facets == 1
        assert result.threshold == 0
    assert scene.get_mesh(0).num_facets == 1


def test_callback_arrays_have_correct_types_and_survive_the_call():
    prepared = _prepare(_triangle_scene(), is_occluder=lambda mi, ii: False)
    snapshots = []

    def evaluate(keep, views):
        assert keep.dtype == np.uint8 and keep.shape == (1,)
        assert views.dtype == np.uint64 and views.ndim == 1
        if views.size:
            np.testing.assert_array_equal(views, [0, 1, 2])
        snapshots.append((keep, keep.copy(), views, views.copy()))
        return [0.0] * (5 if views.size == 0 else views.size)

    prepared.apply_with_view_error(
        evaluate,
        num_views=5,
        max_view_error=0,
        num_threshold_iterations=3,
        graphcut_smoothness_candidates=[0.01],
        smoothness_tradeoff=0,
    )
    assert any(views.size for _, _, views, _ in snapshots)
    for keep, expected_keep, views, expected_views in snapshots:
        np.testing.assert_array_equal(keep, expected_keep)
        np.testing.assert_array_equal(views, expected_views)


@pytest.mark.parametrize(
    "options, message",
    [
        pytest.param({"max_view_error": np.nan}, "max_view_error", id="budget"),
        pytest.param(
            {"graphcut_smoothness_candidates": [0]}, "graphcut_smoothness_candidates", id="weights"
        ),
        pytest.param(
            {"num_threshold_iterations": 0}, "num_threshold_iterations", id="threshold-count"
        ),
        pytest.param(
            {"num_refinement_iterations": 0}, "num_refinement_iterations", id="refinement-count"
        ),
        pytest.param({"num_views": 0}, "num_views", id="view-count"),
        pytest.param({"smoothness_tradeoff": 1.1}, "smoothness_tradeoff", id="tradeoff"),
    ],
)
def test_search_keywords_reach_validation_before_evaluation(options, message):
    prepared = _prepare(_triangle_scene())
    calls = []

    def evaluate(keep, views):
        calls.append(True)
        return [0.0]

    with pytest.raises(RuntimeError, match=message):
        prepared.apply_with_view_error(evaluate, **({"num_views": 1} | options))
    assert not calls


@pytest.mark.parametrize(
    "errors, message",
    [
        pytest.param([], "incorrect number", id="too-few"),
        pytest.param([0.0, 0.0], "incorrect number", id="too-many"),
        pytest.param([np.nan], "invalid error", id="invalid-value"),
    ],
)
def test_callback_errors_are_checked(errors, message):
    prepared = _prepare(_triangle_scene())
    with pytest.raises(RuntimeError, match=message):
        prepared.apply_with_view_error(lambda keep, views: errors, num_views=1)


def test_callback_exception_propagates_and_filter_remains_usable():
    prepared = _prepare(_triangle_scene())

    def fail(keep, views):
        raise ValueError("renderer failed")

    with pytest.raises(ValueError, match="renderer failed"):
        prepared.apply_with_view_error(fail, num_views=1)
    output, result = prepared.apply_with_view_error(
        lambda keep, views: [0.0], num_views=1, smoothness_tradeoff=None
    )
    assert output.num_meshes == 0 and result.within_error_budget


def test_repeated_solves_reuse_samples_and_preserve_instance_order():
    scene = lagrange.scene.SimpleScene3D()
    mesh = lagrange.SurfaceMesh()
    mesh.vertices = np.array(
        [[0, 0, 0], [1, 0, 0], [0, 1, 0], [10, 0, 0], [12, 0, 0], [10, 1, 0]], dtype=np.float64
    )
    mesh.facets = np.array([[0, 1, 2], [3, 4, 5]], dtype=np.uint32)
    mesh.create_attribute("facet_id", initial_values=np.array([10, 20], dtype=np.uint32))
    scene.add_mesh(mesh)
    scene.add_mesh(_triangle_scene().get_mesh(0))
    for mi, x in [(0, 1), (0, 3), (1, 5)]:
        instance = lagrange.scene.MeshInstance3D()
        instance.mesh_index = mi
        transform = np.eye(4)
        transform[0, 3] = x
        instance.transform = transform
        scene.add_instance(instance)
    calls = []
    prepared = _prepare(
        scene,
        size_influence=1,
        max_visibility_threshold=0.2,
        is_occluder=lambda mi, ii: calls.append((mi, ii)) or False,
    )
    calls_after_sampling = calls.copy()
    expected_transforms = [scene.get_instance(0, ii).transform.copy() for ii in range(2)]
    expected_transforms.append(scene.get_instance(1, 0).transform.copy())

    def evaluate(keep, views):
        assert keep.shape == (5,)
        return [0.005 if keep[4] else 1.0]

    for budget in [0.01, 0.02]:
        output, result = prepared.apply_with_view_error(
            evaluate,
            num_views=1,
            max_view_error=budget,
            num_threshold_iterations=12,
            smoothness_tradeoff=None,
        )
        assert output.num_meshes == output.total_num_instances == 3
        assert result.threshold > prepared.max_visibility_threshold
        for mi, transform in enumerate(expected_transforms):
            np.testing.assert_array_equal(output.get_instance(mi, 0).transform, transform)
            if mi < 2:
                np.testing.assert_array_equal(
                    output.get_mesh(mi).attribute("facet_id").data.ravel(), [20]
                )
        assert calls == calls_after_sampling
    assert scene.get_mesh(0).num_facets == 2 and not scene.get_mesh(0).has_edges
    np.testing.assert_array_equal(scene.get_mesh(0).attribute("facet_id").data.ravel(), [10, 20])
    with pytest.raises(RuntimeError, match="apply visibility_threshold"):
        prepared.apply(visibility_threshold=result.threshold)


@pytest.fixture
def smoothing_case():
    mesh = lagrange.SurfaceMesh()
    mesh.vertices = np.array(
        [
            [0, 0, 0],
            [1, 0, 0],
            [0, 0.2, 0],
            [10, 0, 0],
            [11, 0, 0],
            [10, 0.8, 0],
            [10, -2, 0],
            [20, 0, 0],
            [21, 0, 0],
            [20, 1.2, 0],
        ],
        dtype=np.float64,
    )
    mesh.facets = np.array([[0, 1, 2], [3, 4, 5], [4, 3, 6], [7, 8, 9]], dtype=np.uint32)
    mesh.create_attribute("facet_id", initial_values=np.arange(4, dtype=np.uint32))
    scene = lagrange.scene.mesh_to_simple_scene(mesh)
    prepared = _prepare(
        scene, size_influence=1, max_visibility_threshold=0.2, is_occluder=lambda mi, ii: False
    )
    measured = prepared.estimate_visibility("visibility")
    weight = measured.get_mesh(0).attribute("visibility").data.ravel()[2] / 2
    return scene, prepared, weight


def _smoothing_error(keep, views):
    return [0.005 if keep[2] and (keep[1] or keep[3]) else 1.0]


def test_none_and_zero_tradeoff_remain_distinct(smoothing_case):
    scene, prepared, weight = smoothing_case
    baseline, baseline_result = prepared.apply_with_view_error(
        _smoothing_error,
        num_views=1,
        graphcut_smoothness_candidates=[weight],
        num_threshold_iterations=16,
        smoothness_tradeoff=None,
    )
    coherent, result = prepared.apply_with_view_error(
        _smoothing_error,
        num_views=1,
        graphcut_smoothness_candidates=[weight],
        num_threshold_iterations=16,
        smoothness_tradeoff=0,
    )
    np.testing.assert_array_equal(baseline.get_mesh(0).attribute("facet_id").data.ravel(), [2, 3])
    np.testing.assert_array_equal(coherent.get_mesh(0).attribute("facet_id").data.ravel(), [1, 2])
    assert baseline_result.graphcut_smoothness == 0 and result.graphcut_smoothness == weight
    assert result.removed_fraction == baseline_result.removed_fraction == 0.5
    assert scene.get_mesh(0).num_facets == 4


def test_free_wrapper_matches_cached_filter(smoothing_case):
    scene, prepared, weight = smoothing_case
    cached, cached_result = prepared.apply_with_view_error(
        _smoothing_error,
        num_views=1,
        graphcut_smoothness_candidates=[weight],
        num_threshold_iterations=16,
        smoothness_tradeoff=0,
    )
    output, result = lagrange.raycasting.remove_occluded_facets_with_view_error(
        scene,
        _smoothing_error,
        num_rays=200,
        batch_size=200,
        adaptive=False,
        size_influence=1,
        threshold=0.2,
        is_occluder=lambda mi, ii: False,
        num_views=1,
        graphcut_smoothness_candidates=[weight],
        num_threshold_iterations=16,
        smoothness_tradeoff=0,
    )
    np.testing.assert_array_equal(
        output.get_mesh(0).attribute("facet_id").data, cached.get_mesh(0).attribute("facet_id").data
    )
    for field in (
        "graphcut_smoothness",
        "threshold",
        "removed_fraction",
        "num_isolated_facets",
        "worst_view_error",
        "within_error_budget",
    ):
        assert getattr(result, field) == getattr(cached_result, field)


def test_filter_keeps_borrowed_scene_alive():
    scene = _triangle_scene()
    references = sys.getrefcount(scene)
    prepared = _prepare(scene)
    assert sys.getrefcount(scene) > references
    del scene
    gc.collect()
    _, result = prepared.apply_with_view_error(lambda keep, views: [0.0], num_views=1)
    assert result.within_error_budget


def test_free_wrapper_validates_before_sampling():
    calls = []
    with pytest.raises(RuntimeError, match="num_refinement_iterations"):
        lagrange.raycasting.remove_occluded_facets_with_view_error(
            _triangle_scene(),
            lambda keep, views: [0.0],
            num_views=1,
            num_refinement_iterations=0,
            is_occluder=lambda mi, ii: calls.append((mi, ii)) or True,
        )
    assert not calls


def test_required_keywords_and_result_surface():
    scene = _triangle_scene()
    prepared = _prepare(scene)

    def evaluate(keep, views):
        return [0.0]

    with pytest.raises(TypeError):
        prepared.apply_with_view_error(evaluate)  # type: ignore[pyrefly:missing-argument]
    with pytest.raises(TypeError):
        lagrange.raycasting.remove_occluded_facets_with_view_error(scene, evaluate)  # type: ignore[ty:missing-argument, pyrefly:missing-argument]
    with pytest.raises(TypeError):
        prepared.apply_with_view_error(evaluate, num_views=1, num_worst_views=1)  # type: ignore[pyrefly:unexpected-keyword]
    _, result = prepared.apply_with_view_error(evaluate, num_views=1, smoothness_tradeoff=None)
    assert isinstance(result, lagrange.raycasting.OccludedFacetViewErrorResult)
    with pytest.raises(AttributeError):
        result.threshold = 2  # type: ignore[pyrefly:read-only]
    for name in (
        "compute_graphcut_mask",
        "solve_graphcut_with_view_error",
        "compute_graph_cut_mask",
        "remove_occluded_facets_by_rmse",
        "GraphCutRmseResult",
    ):
        assert not hasattr(lagrange.raycasting, name)
    for name in ("smoothness", "remove_ratio", "success", "use_all_facets"):
        assert not hasattr(result, name)
