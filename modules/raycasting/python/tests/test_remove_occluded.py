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


class TestRemoveOccluded:
    def test_remove_occluded_facets_single_cube(self, cube_triangular):
        """A single cube has no occluded facets — all 12 should remain."""
        scene = lagrange.scene.mesh_to_simple_scene(cube_triangular)
        result = lagrange.raycasting.remove_occluded_facets(scene, num_rays=100000)
        assert result.num_meshes == 1
        assert result.get_mesh(0).num_facets == 12

    def test_remove_occluded_facets_brute_force(self, cube_triangular):
        """Brute-force mode produces the same result on a non-occluded cube."""
        scene = lagrange.scene.mesh_to_simple_scene(cube_triangular)
        result = lagrange.raycasting.remove_occluded_facets(
            scene, num_rays=100000, brute_force=True
        )
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
            lagrange.raycasting.remove_occluded_facets(scene, 10000)


# ---------------------------------------------------------------------------
# Nested cubes — minimal occlusion sanity test.
# Outer cube fully encloses inner cube. Inner instance must be culled,
# inner facets must all disappear after facet-level filtering.
# ---------------------------------------------------------------------------


class TestNestedCubes:
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
