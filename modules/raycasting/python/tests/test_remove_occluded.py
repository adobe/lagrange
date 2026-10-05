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
from typing import TypedDict

import lagrange
import numpy as np
import pytest


class _RayBudget(TypedDict):
    num_rays: int
    batch_size: int


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
            lagrange.raycasting.remove_occluded_facets(scene, 10000)  # ty: ignore[too-many-positional-arguments]

    def test_facet_until_converged_removed(self, cube_triangular):
        scene = lagrange.scene.mesh_to_simple_scene(cube_triangular)
        with pytest.raises(TypeError):
            lagrange.raycasting.remove_occluded_facets(
                scene,
                num_rays=10_000,
                until_converged=True,  # ty: ignore[unknown-argument]
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
        estimated = lagrange.raycasting.estimate_occluded_facet_measures(
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
        """estimate_occluded_facet_measures returns a de-instanced scene whose meshes carry the named
        per-facet measure attribute; the outer facets are all above threshold, the inner below."""
        est = lagrange.raycasting.estimate_occluded_facet_measures(
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
        est = lagrange.raycasting.estimate_occluded_facet_measures(
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
        est = lagrange.raycasting.estimate_occluded_facet_measures(
            shared_mesh_occlusion_scene, attribute_name="vis", instancing=policy, **self._RAYS
        )
        assert est.num_meshes == 2
        assert est.total_num_instances == 3
        small = est.get_mesh(1)
        assert small.has_attribute("vis")
        vals = np.asarray(small.attribute("vis").data).ravel()
        assert len(vals) == small.num_facets
        assert (vals > 0).all()
