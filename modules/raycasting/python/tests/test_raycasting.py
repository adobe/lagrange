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
import copy

import lagrange
import numpy as np
import pytest


# ---------------------------------------------------------------------------
# Fixtures
# ---------------------------------------------------------------------------


@pytest.fixture
def unit_cube():
    """Axis-aligned unit cube [0,1]^3 with 12 triangles."""
    vertices = np.array(
        [
            [0, 0, 0],
            [1, 0, 0],
            [1, 1, 0],
            [0, 1, 0],
            [0, 0, 1],
            [1, 0, 1],
            [1, 1, 1],
            [0, 1, 1],
        ],
        dtype=float,
    )
    facets = np.array(
        [
            [0, 3, 2],
            [2, 1, 0],
            [4, 5, 6],
            [6, 7, 4],
            [1, 2, 6],
            [6, 5, 1],
            [4, 7, 3],
            [3, 0, 4],
            [2, 3, 7],
            [7, 6, 2],
            [0, 1, 5],
            [5, 4, 0],
        ],
        dtype=np.uint32,
    )
    mesh = lagrange.SurfaceMesh()
    mesh.vertices = vertices
    mesh.facets = facets
    return mesh


@pytest.fixture
def shifted_cube():
    """Unit cube shifted by (0.1, 0.1, 0.1) so no vertex coincides with unit_cube."""
    vertices = np.array(
        [
            [0.1, 0.1, 0.1],
            [1.1, 0.1, 0.1],
            [1.1, 1.1, 0.1],
            [0.1, 1.1, 0.1],
            [0.1, 0.1, 1.1],
            [1.1, 0.1, 1.1],
            [1.1, 1.1, 1.1],
            [0.1, 1.1, 1.1],
        ],
        dtype=float,
    )
    facets = np.array(
        [
            [0, 3, 2],
            [2, 1, 0],
            [4, 5, 6],
            [6, 7, 4],
            [1, 2, 6],
            [6, 5, 1],
            [4, 7, 3],
            [3, 0, 4],
            [2, 3, 7],
            [7, 6, 2],
            [0, 1, 5],
            [5, 4, 0],
        ],
        dtype=np.uint32,
    )
    mesh = lagrange.SurfaceMesh()
    mesh.vertices = vertices
    mesh.facets = facets
    return mesh


def _add_vertex_color(mesh, name="color"):
    """Add a per-vertex RGB color attribute whose value equals the vertex position."""
    attr_id = mesh.create_attribute(
        name,
        element=lagrange.AttributeElement.Vertex,
        usage=lagrange.AttributeUsage.Color,
        initial_values=mesh.vertices.astype(np.float64),
        num_channels=3,
    )
    return attr_id


# ---------------------------------------------------------------------------
# Enum smoke tests
# ---------------------------------------------------------------------------


class TestEnums:
    def test_scene_flags_defaults(self):
        empty_flag = lagrange.raycasting.SceneFlags.Empty
        assert int(empty_flag) == 0
        # Arithmetic flag operations (nb::is_arithmetic)
        combined = int(lagrange.raycasting.SceneFlags.Robust) | int(
            lagrange.raycasting.SceneFlags.Filter
        )
        assert combined != 0

    def test_build_quality(self):
        assert lagrange.raycasting.BuildQuality.Low is not None
        assert lagrange.raycasting.BuildQuality.Medium is not None
        assert lagrange.raycasting.BuildQuality.High is not None

    def test_cast_mode(self):
        assert lagrange.raycasting.CastMode.OneWay is not None
        assert lagrange.raycasting.CastMode.BothWays is not None

    def test_fallback_mode(self):
        assert lagrange.raycasting.FallbackMode.Constant is not None
        assert lagrange.raycasting.FallbackMode.ClosestVertex is not None
        assert lagrange.raycasting.FallbackMode.ClosestPoint is not None

    def test_project_mode(self):
        assert lagrange.raycasting.ProjectMode.ClosestVertex is not None
        assert lagrange.raycasting.ProjectMode.ClosestPoint is not None
        assert lagrange.raycasting.ProjectMode.RayCasting is not None


# ---------------------------------------------------------------------------
# RayCaster construction
# ---------------------------------------------------------------------------


class TestRayCaster:
    def test_default_construction(self):
        rc = lagrange.raycasting.RayCaster()
        assert rc is not None

    def test_construction_with_args(self):
        rc = lagrange.raycasting.RayCaster(
            scene_flags=int(lagrange.raycasting.SceneFlags.Robust),
            build_quality=lagrange.raycasting.BuildQuality.High,
        )
        assert rc is not None

    def test_add_mesh_and_commit(self, unit_cube):
        rc = lagrange.raycasting.RayCaster()
        idx = rc.add_mesh(unit_cube)
        assert idx == 0
        rc.commit_updates()

    def test_add_mesh_without_instance(self, unit_cube):
        """add_mesh with transform=None adds the mesh without creating an instance."""
        rc = lagrange.raycasting.RayCaster()
        mesh_idx = rc.add_mesh(unit_cube, transform=None)
        assert mesh_idx == 0
        # Manually add an instance — should be instance 0 since none was created above
        inst_idx = rc.add_instance(mesh_idx, np.eye(4, dtype=np.float32))
        assert inst_idx == 0
        rc.commit_updates()

    def test_add_instance(self, unit_cube):
        rc = lagrange.raycasting.RayCaster()
        mesh_idx = rc.add_mesh(unit_cube)
        transform = np.eye(4, dtype=np.float32)
        transform[0, 3] = 2.0  # translate x by 2
        inst_idx = rc.add_instance(mesh_idx, transform)
        assert inst_idx == 1  # instance 0 is the original from add_mesh
        rc.commit_updates()

    def test_add_scene(self, unit_cube):
        """add_scene should accept a SimpleScene3D."""
        scene = lagrange.scene.SimpleScene3D()
        scene.add_mesh(unit_cube)
        inst = lagrange.scene.MeshInstance3D()
        inst.mesh_index = 0
        inst.transform = np.eye(4, dtype=np.float32)
        scene.add_instance(inst)
        rc = lagrange.raycasting.RayCaster()
        rc.add_scene(scene)
        rc.commit_updates()

    def test_get_set_transform(self, unit_cube):
        rc = lagrange.raycasting.RayCaster()
        mesh_idx = rc.add_mesh(unit_cube)
        rc.commit_updates()

        t = rc.get_transform(mesh_idx, 0)
        assert t.shape == (4, 4)
        np.testing.assert_allclose(t, np.eye(4, dtype=np.float32), atol=1e-6)

        new_t = np.eye(4, dtype=np.float32)
        new_t[1, 3] = 5.0
        rc.update_transform(mesh_idx, 0, new_t)
        rc.commit_updates()
        got = rc.get_transform(mesh_idx, 0)
        np.testing.assert_allclose(got, new_t, atol=1e-6)

    def test_get_set_visibility(self, unit_cube):
        rc = lagrange.raycasting.RayCaster()
        mesh_idx = rc.add_mesh(unit_cube)
        rc.commit_updates()

        assert rc.get_visibility(mesh_idx, 0) is True
        rc.update_visibility(mesh_idx, 0, False)
        rc.commit_updates()
        assert rc.get_visibility(mesh_idx, 0) is False

    def test_update_mesh(self, unit_cube, shifted_cube):
        rc = lagrange.raycasting.RayCaster()
        mesh_idx = rc.add_mesh(unit_cube)
        rc.commit_updates()

        rc.update_mesh(mesh_idx, shifted_cube)
        rc.commit_updates()

    def test_update_vertices(self, unit_cube):
        rc = lagrange.raycasting.RayCaster()
        mesh_idx = rc.add_mesh(unit_cube)
        rc.commit_updates()

        rc.update_vertices(mesh_idx, unit_cube)
        rc.commit_updates()


# ---------------------------------------------------------------------------
# project_closest_point
# ---------------------------------------------------------------------------


class TestProjectClosestPoint:
    def test_self_transfer(self, unit_cube):
        """Projecting a mesh onto itself should reproduce the attribute exactly."""
        attr_id = _add_vertex_color(unit_cube)
        target = copy.copy(unit_cube)
        lagrange.raycasting.project_closest_point(
            unit_cube, target, attribute_ids=[attr_id], project_vertices=False
        )
        assert target.has_attribute("color")
        src_data = unit_cube.attribute(attr_id).data
        dst_data = target.attribute("color").data
        np.testing.assert_allclose(dst_data, src_data, atol=1e-5)

    def test_with_cached_raycaster(self, unit_cube):
        """Using a pre-built RayCaster should give the same result."""
        attr_id = _add_vertex_color(unit_cube)
        rc = lagrange.raycasting.RayCaster()
        rc.add_mesh(unit_cube)
        rc.commit_updates()

        target = copy.copy(unit_cube)
        lagrange.raycasting.project_closest_point(
            unit_cube, target, attribute_ids=[attr_id], project_vertices=False, ray_caster=rc
        )
        src_data = unit_cube.attribute(attr_id).data
        dst_data = target.attribute("color").data
        np.testing.assert_allclose(dst_data, src_data, atol=1e-5)


# ---------------------------------------------------------------------------
# project_closest_vertex
# ---------------------------------------------------------------------------


class TestProjectClosestVertex:
    def test_self_transfer(self, unit_cube):
        """Closest vertex on self → exact copy."""
        attr_id = _add_vertex_color(unit_cube)
        target = copy.copy(unit_cube)
        lagrange.raycasting.project_closest_vertex(
            unit_cube, target, attribute_ids=[attr_id], project_vertices=False
        )
        src_data = unit_cube.attribute(attr_id).data
        dst_data = target.attribute("color").data
        np.testing.assert_allclose(dst_data, src_data, atol=1e-5)


# ---------------------------------------------------------------------------
# project_directional
# ---------------------------------------------------------------------------


class TestProjectDirectional:
    def test_along_z(self, unit_cube):
        """Project along z onto the unit cube; every cube vertex should get a hit."""
        attr_id = _add_vertex_color(unit_cube)
        target = copy.copy(unit_cube)
        lagrange.raycasting.project_directional(
            unit_cube,
            target,
            attribute_ids=[attr_id],
            project_vertices=False,
            direction=np.array([0, 0, 1], dtype=np.float32),
        )
        dst_data = target.attribute("color").data
        # All vertices lie on the cube, so every ray should hit and produce valid colors.
        assert dst_data.shape == (unit_cube.num_vertices, 3)
        assert not np.any(np.isnan(dst_data))


# ---------------------------------------------------------------------------
# project  (combined convenience function)
# ---------------------------------------------------------------------------


class TestProject:
    def test_closest_point_mode(self, unit_cube):
        attr_id = _add_vertex_color(unit_cube)
        target = copy.copy(unit_cube)
        lagrange.raycasting.project(
            unit_cube,
            target,
            attribute_ids=[attr_id],
            project_vertices=False,
            project_mode=lagrange.raycasting.ProjectMode.ClosestPoint,
        )
        src_data = unit_cube.attribute(attr_id).data
        dst_data = target.attribute("color").data
        np.testing.assert_allclose(dst_data, src_data, atol=1e-5)

    def test_closest_vertex_mode(self, unit_cube):
        attr_id = _add_vertex_color(unit_cube)
        target = copy.copy(unit_cube)
        lagrange.raycasting.project(
            unit_cube,
            target,
            attribute_ids=[attr_id],
            project_vertices=False,
            project_mode=lagrange.raycasting.ProjectMode.ClosestVertex,
        )
        src_data = unit_cube.attribute(attr_id).data
        dst_data = target.attribute("color").data
        np.testing.assert_allclose(dst_data, src_data, atol=1e-5)

    def test_raycasting_mode(self, unit_cube):
        attr_id = _add_vertex_color(unit_cube)
        target = copy.copy(unit_cube)
        lagrange.raycasting.project(
            unit_cube,
            target,
            attribute_ids=[attr_id],
            project_vertices=False,
            project_mode=lagrange.raycasting.ProjectMode.RayCasting,
            direction=np.array([0, 0, 1], dtype=np.float32),
        )
        dst_data = target.attribute("color").data
        assert dst_data.shape == (unit_cube.num_vertices, 3)


# ---------------------------------------------------------------------------
# NumPy-array overloads
# ---------------------------------------------------------------------------


class TestProjectClosestPointNumpy:
    def test_project_points_onto_cube(self, unit_cube):
        """Query points outside the cube are projected to the closest surface point."""
        query = np.array(
            [
                [0.5, 0.5, 2.0],  # above top face → should land at z=1
                [0.5, 0.5, -1.0],  # below bottom face → z=0
                [2.0, 0.5, 0.5],  # right of cube → x=1
            ],
            dtype=np.float64,
        )
        result = lagrange.raycasting.project_closest_point(unit_cube, query)
        assert isinstance(result, np.ndarray)
        assert result.shape == (3, 3)
        np.testing.assert_allclose(result[0], [0.5, 0.5, 1.0], atol=1e-5)
        np.testing.assert_allclose(result[1], [0.5, 0.5, 0.0], atol=1e-5)
        np.testing.assert_allclose(result[2], [1.0, 0.5, 0.5], atol=1e-5)

    def test_modifies_input_in_place(self, unit_cube):
        """The input query array is modified in place with projected positions."""
        query = np.array([[0.5, 0.5, 2.0]], dtype=np.float64)
        result = lagrange.raycasting.project_closest_point(unit_cube, query)
        np.testing.assert_allclose(query[0], [0.5, 0.5, 1.0], atol=1e-5)
        np.testing.assert_array_equal(result, query)

    def test_with_raycaster(self, unit_cube):
        """Pre-built RayCaster should give the same result."""
        rc = lagrange.raycasting.RayCaster()
        rc.add_mesh(unit_cube)
        rc.commit_updates()

        query = np.array([[0.5, 0.5, 2.0]], dtype=np.float64)
        result = lagrange.raycasting.project_closest_point(unit_cube, query, ray_caster=rc)
        assert isinstance(result, np.ndarray)
        np.testing.assert_allclose(result[0], [0.5, 0.5, 1.0], atol=1e-5)


class TestProjectClosestVertexNumpy:
    def test_snap_to_vertex(self, unit_cube):
        """Query points are snapped to the nearest cube vertex."""
        query = np.array(
            [
                [0.1, 0.1, 0.1],  # nearest vertex is (0,0,0)
                [0.9, 0.9, 0.9],  # nearest vertex is (1,1,1)
            ],
            dtype=np.float64,
        )
        result = lagrange.raycasting.project_closest_vertex(unit_cube, query)
        assert isinstance(result, np.ndarray)
        assert result.shape == (2, 3)
        np.testing.assert_allclose(result[0], [0.0, 0.0, 0.0], atol=1e-5)
        np.testing.assert_allclose(result[1], [1.0, 1.0, 1.0], atol=1e-5)


class TestProjectDirectionalNumpy:
    def test_along_z(self, unit_cube):
        """Points above the cube, projecting along -z, should hit z=1."""
        query = np.array(
            [
                [0.5, 0.5, 5.0],
                [0.25, 0.75, 5.0],
            ],
            dtype=np.float64,
        )
        result = lagrange.raycasting.project_directional(
            unit_cube,
            query,
            direction=np.array([0, 0, -1], dtype=np.float32),
            cast_mode=lagrange.raycasting.CastMode.OneWay,
        )
        assert isinstance(result, np.ndarray)
        assert result.shape == (2, 3)
        np.testing.assert_allclose(result[:, 2], [1.0, 1.0], atol=1e-5)

    def test_both_ways_fallback(self, unit_cube):
        """BothWays + ClosestPoint fallback should still produce valid output."""
        query = np.array([[0.5, 0.5, 0.5]], dtype=np.float64)
        result = lagrange.raycasting.project_directional(
            unit_cube,
            query,
            direction=np.array([0, 0, 1], dtype=np.float32),
            cast_mode=lagrange.raycasting.CastMode.BothWays,
            fallback_mode=lagrange.raycasting.FallbackMode.ClosestPoint,
        )
        assert isinstance(result, np.ndarray)
        assert result.shape == (1, 3)


class TestProjectNumpy:
    def test_closest_point_mode(self, unit_cube):
        query = np.array([[0.5, 0.5, 2.0]], dtype=np.float64)
        result = lagrange.raycasting.project(
            unit_cube,
            query,
            project_mode=lagrange.raycasting.ProjectMode.ClosestPoint,
        )
        assert isinstance(result, np.ndarray)
        np.testing.assert_allclose(result[0], [0.5, 0.5, 1.0], atol=1e-5)

    def test_closest_vertex_mode(self, unit_cube):
        query = np.array([[0.1, 0.1, 0.1]], dtype=np.float64)
        result = lagrange.raycasting.project(
            unit_cube,
            query,
            project_mode=lagrange.raycasting.ProjectMode.ClosestVertex,
        )
        assert isinstance(result, np.ndarray)
        np.testing.assert_allclose(result[0], [0.0, 0.0, 0.0], atol=1e-5)

    def test_raycasting_mode(self, unit_cube):
        query = np.array([[0.5, 0.5, 5.0]], dtype=np.float64)
        result = lagrange.raycasting.project(
            unit_cube,
            query,
            project_mode=lagrange.raycasting.ProjectMode.RayCasting,
            direction=np.array([0, 0, -1], dtype=np.float32),
        )
        assert isinstance(result, np.ndarray)
        np.testing.assert_allclose(result[0, 2], 1.0, atol=1e-5)


# ---------------------------------------------------------------------------
# compute_local_feature_size
# ---------------------------------------------------------------------------


class TestComputeLocalFeatureSize:
    def test_default_parameters(self, unit_cube):
        """Test compute_local_feature_size with default parameters."""
        attr_id = lagrange.raycasting.compute_local_feature_size(unit_cube)
        assert unit_cube.has_attribute("@lfs")
        lfs_data = unit_cube.attribute(attr_id).data
        assert lfs_data.shape == (unit_cube.num_vertices,)
        # All LFS values should be finite for a closed cube
        assert np.all(np.isfinite(lfs_data))

    def test_interior_mode(self, unit_cube):
        """Test with interior ray casting mode."""
        # Test with string
        attr_id = lagrange.raycasting.compute_local_feature_size(
            unit_cube,
            output_attribute_name="@lfs_interior",
            direction_mode="interior",
        )
        assert unit_cube.has_attribute("@lfs_interior")
        lfs_data = unit_cube.attribute(attr_id).data
        assert np.all(np.isfinite(lfs_data))
        assert np.all(lfs_data > 0)

        # Test with enum
        attr_id_enum = lagrange.raycasting.compute_local_feature_size(
            unit_cube,
            output_attribute_name="@lfs_interior_enum",
            direction_mode="interior",
        )
        assert unit_cube.has_attribute("@lfs_interior_enum")
        lfs_data_enum = unit_cube.attribute(attr_id_enum).data
        assert np.all(np.isfinite(lfs_data_enum))

    def test_exterior_mode(self, unit_cube):
        """Test with exterior ray casting mode using string."""
        lagrange.raycasting.compute_local_feature_size(
            unit_cube,
            output_attribute_name="@lfs_exterior",
            direction_mode="exterior",
        )
        assert unit_cube.has_attribute("@lfs_exterior")
        # Note: exterior rays may not always hit for a finite mesh
        # so we just check that the attribute was created

    def test_both_mode(self, unit_cube):
        """Test with both directions ray casting mode using string."""
        attr_id = lagrange.raycasting.compute_local_feature_size(
            unit_cube,
            output_attribute_name="@lfs_both",
            direction_mode="both",
        )
        assert unit_cube.has_attribute("@lfs_both")
        lfs_data = unit_cube.attribute(attr_id).data
        assert np.all(np.isfinite(lfs_data))

    def test_with_cached_raycaster(self, unit_cube):
        """Test using a pre-built RayCaster."""
        rc = lagrange.raycasting.RayCaster()
        rc.add_mesh(unit_cube)
        rc.commit_updates()

        attr_id = lagrange.raycasting.compute_local_feature_size(
            unit_cube,
            output_attribute_name="@lfs_cached",
            direction_mode="interior",
            ray_caster=rc,
        )
        assert unit_cube.has_attribute("@lfs_cached")
        lfs_data = unit_cube.attribute(attr_id).data
        assert np.all(np.isfinite(lfs_data))

    def test_medial_axis_tolerance(self, unit_cube):
        """Test the medial axis tolerance parameter."""
        # With loose tolerance
        attr_id_loose = lagrange.raycasting.compute_local_feature_size(
            unit_cube,
            output_attribute_name="@lfs_loose",
            direction_mode="interior",
            medial_axis_tolerance=1e-3,
        )
        assert unit_cube.has_attribute("@lfs_loose")
        lfs_loose = unit_cube.attribute(attr_id_loose).data
        assert np.all(np.isfinite(lfs_loose))

        # With tight tolerance
        attr_id_tight = lagrange.raycasting.compute_local_feature_size(
            unit_cube,
            output_attribute_name="@lfs_tight",
            direction_mode="interior",
            medial_axis_tolerance=1e-5,
        )
        assert unit_cube.has_attribute("@lfs_tight")
        lfs_tight = unit_cube.attribute(attr_id_tight).data
        assert np.all(np.isfinite(lfs_tight))

        # Both should produce valid results
        # (values may differ slightly due to different convergence)

    def test_default_lfs(self, unit_cube):
        """Test the default_lfs parameter as fallback value."""
        attr_id = lagrange.raycasting.compute_local_feature_size(
            unit_cube,
            output_attribute_name="@lfs_default",
            default_lfs=999.0,
        )
        assert unit_cube.has_attribute("@lfs_default")
        lfs_data = unit_cube.attribute(attr_id).data
        # For a closed cube with interior mode, all rays should hit
        # so we should get valid computed values (not the default)
        assert np.all(lfs_data < 999.0)
        assert lfs_data.shape == (unit_cube.num_vertices,)

    def test_invalid_direction_mode(self, unit_cube):
        """Test that invalid direction mode string raises an error."""
        with pytest.raises(RuntimeError, match="Invalid direction_mode"):
            lagrange.raycasting.compute_local_feature_size(
                unit_cube,
                direction_mode="invalid_mode",
            )

    def test_keyword_only_arguments(self, unit_cube):
        """Test that optional arguments are keyword-only."""
        # This should work
        lagrange.raycasting.compute_local_feature_size(
            unit_cube,
            medial_axis_tolerance=1e-4,
        )
        assert unit_cube.has_attribute("@lfs")

        # This should fail because we're passing positional args after mesh
        with pytest.raises(TypeError):
            lagrange.raycasting.compute_local_feature_size(unit_cube, 1e-4)
