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
from lagrange.volume import Grid
import numpy as np
import tempfile
from typing import Literal
from pathlib import Path
import pytest

GRID_TYPES: tuple[Literal["vdb", "nvdb"], ...] = ("vdb", "nvdb")


class TestMeshToVolume:
    def test_bbox(self, cube):
        mesh = cube.clone()
        for dtype in [np.float32, np.float64, float]:
            grid = Grid.from_mesh(mesh, dtype=dtype)
            assert grid.voxel_size.shape == (3,)
            assert grid.num_active_voxels > 0
            assert grid.bbox_index.dtype == np.dtype(np.int32)
            assert grid.bbox_world.dtype == np.dtype(np.float64)
            assert grid.index_to_world(grid.bbox_index.astype(np.float64)).dtype == np.dtype(
                np.float64
            )
            assert np.allclose(grid.bbox_index, grid.world_to_index(grid.bbox_world))
            assert np.allclose(grid.bbox_world, grid.index_to_world(grid.bbox_index))
            assert np.allclose(
                grid.bbox_world,
                grid.index_to_world(grid.bbox_index.astype(np.float64)),
            )

    def test_sampling(self, cube):
        mesh = cube.clone()
        for grid_dtype in [np.float32, np.float64]:
            for pts_dtype in [np.float32, np.float64]:
                grid = Grid.from_mesh(mesh, dtype=grid_dtype)
                points_index = np.array(
                    [
                        [0, 0, 0],
                        [5, 5, 5],
                        [10, 10, 10],
                        [15, 15, 15],
                    ],
                    dtype=np.int32,
                )
                values_is_i = grid.sample_trilinear_index_space(points_index)
                values_is_s = grid.sample_trilinear_index_space(points_index.astype(pts_dtype))
                assert values_is_i.dtype == grid_dtype
                assert values_is_s.dtype == grid_dtype
                assert np.allclose(values_is_i, values_is_s)
                points_world_i = grid.index_to_world(points_index)
                points_world_s = grid.index_to_world(points_index.astype(pts_dtype))
                assert points_world_i.dtype == np.dtype(np.float64)
                assert points_world_s.dtype == pts_dtype
                values_ws_i = grid.sample_trilinear_world_space(points_world_i)
                values_ws_s = grid.sample_trilinear_world_space(points_world_s)
                assert values_ws_i.dtype == grid_dtype
                assert values_ws_s.dtype == grid_dtype
                assert np.allclose(values_is_i, values_ws_i)
                assert np.allclose(values_ws_i, values_ws_s)

    def test_cube(self, cube):
        mesh = cube.clone()
        with tempfile.TemporaryDirectory() as tmp_dir:
            tmp_dir = Path(tmp_dir)
            for ext in GRID_TYPES:
                for comp in [
                    lagrange.volume.Compression.Uncompressed,
                    lagrange.volume.Compression.Zip,
                    lagrange.volume.Compression.Blosc,
                ]:
                    grid = Grid.from_mesh(mesh)
                    tmp_path = tmp_dir / f"out_{comp}.{ext}"
                    grid.save(tmp_path, compression=comp)
                    buffer = grid.to_buffer(grid_type=ext, compression=comp)
                    assert type(buffer) is bytes
                    mesh1 = Grid.load(tmp_path).to_mesh()
                    mesh2 = Grid.load(buffer).to_mesh()
                    assert mesh1.num_vertices > 0
                    assert mesh1.num_facets > 0
                    assert mesh1.num_vertices == mesh2.num_vertices
                    assert mesh1.num_facets == mesh2.num_facets

    @pytest.mark.parametrize("model_name", ["cube", "triangle", "house"])
    def test_offset_in_place(self, model_name, request):
        mesh = request.getfixturevalue(model_name).clone()
        with tempfile.TemporaryDirectory() as tmp_dir:
            tmp_dir = Path(tmp_dir)
            for signing in [
                lagrange.volume.Sign.FloodFill,
                lagrange.volume.Sign.WindingNumber,
                lagrange.volume.Sign.Unsigned,
            ]:
                for offset in [-1.0, 1.0]:
                    grid = Grid.from_mesh(mesh, signing_method=signing)
                    active_before = grid.num_active_voxels
                    grid.offset_in_place(offset, relative=True)
                    active_after = grid.num_active_voxels
                    signing_names = {
                        lagrange.volume.Sign.FloodFill: "flood_fill",
                        lagrange.volume.Sign.WindingNumber: "winding_number",
                        lagrange.volume.Sign.Unsigned: "unsigned",
                    }
                    offset_names = {-1.0: "erode", 1.0: "dilate"}
                    tmp_path = (
                        tmp_dir
                        / f"offset_{model_name}_{signing_names[signing]}_{offset_names[offset]}.vdb"
                    )
                    grid.save(tmp_path)
                    if signing == lagrange.volume.Sign.Unsigned:
                        assert active_before == active_after
                    else:
                        if offset < 0:
                            assert active_after > active_before
                        else:
                            assert active_after < active_before

    def test_from_points(self):
        points = np.array(
            [
                [0, 0, 0],
                [1, 2, 3],
                [-2, 5, 7],
            ],
            dtype=np.int32,
        )
        values = np.array([1.0, -2.5, 3.25], dtype=np.float32)

        for grid_dtype in [np.float32, np.float64, float]:
            for values_dtype in [np.float32, np.float64]:
                grid = Grid.from_points(points, values.astype(values_dtype), dtype=grid_dtype)
                assert grid.num_active_voxels == points.shape[0]
                assert np.array_equal(grid.bbox_index[0], points.min(axis=0))
                assert np.array_equal(grid.bbox_index[1], points.max(axis=0))
                sampled = grid.sample_trilinear_index_space(points)
                assert sampled.dtype == np.dtype(grid_dtype)
                assert np.allclose(sampled, values)

    def test_from_points_length_mismatch(self):
        import pytest

        points = np.zeros((3, 3), dtype=np.int32)
        values = np.zeros(2, dtype=np.float32)
        with pytest.raises(RuntimeError):
            Grid.from_points(points, values)

    def test_set_name(self, cube):
        grid = Grid.from_mesh(cube.clone())
        grid.name = "my_grid"
        assert grid.name == "my_grid"
        with tempfile.TemporaryDirectory() as tmp_dir:
            tmp_path = Path(tmp_dir) / "named.vdb"
            grid.save(tmp_path)
            grid_ = Grid.load(tmp_path)
            assert grid_.name == "my_grid"

    def test_set_transform(self):
        points = np.array([[0, 0, 0], [2, 0, 0], [0, 4, 0]], dtype=np.int32)
        values = np.array([1.0, 2.0, 3.0], dtype=np.float32)

        grid = Grid.from_points(points, values)
        # Identity transform -> unit voxel size, index == world.
        assert np.allclose(grid.voxel_size, [1.0, 1.0, 1.0])
        assert np.allclose(grid.index_to_world(points), points.astype(np.float64))

        # Uniform scale 0.5 + translation [1, 2, 3], column-vector convention.
        # Accepts both float32 and float64 matrices.
        for mat_dtype in [np.float32, np.float64]:
            xform = np.eye(4, dtype=mat_dtype)
            xform[:3, :3] *= 0.5
            xform[:3, 3] = [1.0, 2.0, 3.0]
            grid.transform = xform
            assert np.allclose(grid.transform, xform)
            assert np.allclose(grid.voxel_size, [0.5, 0.5, 0.5])
            expected_world = points.astype(np.float64) * 0.5 + np.array([1.0, 2.0, 3.0])
            assert np.allclose(grid.index_to_world(points), expected_world)
            assert np.allclose(grid.world_to_index(expected_world), points.astype(np.float64))

    def test_set_background(self):
        points = np.array([[0, 0, 0]], dtype=np.int32)
        values = np.array([1.0], dtype=np.float32)
        inactive = np.array([[100, 100, 100]], dtype=np.int32)

        for grid_dtype in [np.float32, np.float64]:
            grid = Grid.from_points(points, values, dtype=grid_dtype)
            assert grid.background == 0.0

            grid.background = -7.5
            assert grid.background == -7.5
            # Inactive voxels return the new background.
            assert np.allclose(grid.sample_trilinear_index_space(inactive), -7.5)
            # Active voxel still holds its set value.
            assert np.allclose(grid.sample_trilinear_index_space(points), values)

    def test_set_grid_class(self, cube):
        for grid_class in [
            lagrange.volume.GridClass.Unknown,
            lagrange.volume.GridClass.LevelSet,
            lagrange.volume.GridClass.FogVolume,
            lagrange.volume.GridClass.Staggered,
        ]:
            grid = Grid.from_mesh(cube.clone())
            grid.grid_class = grid_class
            assert grid.grid_class == grid_class

    def test_prune(self, cube):
        grid = Grid.from_mesh(cube.clone())
        active_before = grid.num_active_voxels
        grid.prune()
        active_after = grid.num_active_voxels
        assert active_after <= active_before

    def test_csg(self, cube):
        # Build two overlapping level-set cubes and verify each CSG op changes
        # the active voxel count in the expected direction relative to the
        # individual inputs.
        for op_name, predicate in [
            ("csg_union", lambda u, a, b: u >= max(a, b)),
            ("csg_intersection", lambda i, a, b: i <= min(a, b)),
            ("csg_difference", lambda d, a, b: d <= a),
        ]:
            a = Grid.from_mesh(cube.clone())
            b = Grid.from_mesh(cube.clone())
            count_a = a.num_active_voxels
            count_b = b.num_active_voxels
            getattr(a, op_name)(b)
            assert predicate(a.num_active_voxels, count_a, count_b), op_name

    def test_csg_dtype_mismatch(self, cube):
        import pytest

        a = Grid.from_mesh(cube.clone(), dtype=np.float32)
        b = Grid.from_mesh(cube.clone(), dtype=np.float64)
        with pytest.raises(Exception):
            a.csg_union(b)

    def test_comp_ops(self):
        # Two grids with one shared voxel and one disjoint voxel each, so we
        # can validate per-voxel arithmetic.
        shared = np.array([[0, 0, 0]], dtype=np.int32)
        only_a = np.array([[1, 0, 0]], dtype=np.int32)
        only_b = np.array([[0, 1, 0]], dtype=np.int32)

        def make_pair(va_shared, va_only, vb_shared, vb_only):
            a = Grid.from_points(
                np.vstack([shared, only_a]),
                np.array([va_shared, va_only], dtype=np.float32),
            )
            b = Grid.from_points(
                np.vstack([shared, only_b]),
                np.array([vb_shared, vb_only], dtype=np.float32),
            )
            return a, b

        # comp_min
        a, b = make_pair(3.0, 5.0, 1.0, 7.0)
        a.comp_min(b)
        assert np.allclose(a.sample_trilinear_index_space(shared), 1.0)

        # comp_max
        a, b = make_pair(3.0, 5.0, 1.0, 7.0)
        a.comp_max(b)
        assert np.allclose(a.sample_trilinear_index_space(shared), 3.0)

        # comp_sum
        a, b = make_pair(3.0, 5.0, 1.0, 7.0)
        a.comp_sum(b)
        assert np.allclose(a.sample_trilinear_index_space(shared), 4.0)
        assert np.allclose(a.sample_trilinear_index_space(only_a), 5.0)
        assert np.allclose(a.sample_trilinear_index_space(only_b), 7.0)

        # comp_mul
        a, b = make_pair(3.0, 5.0, 4.0, 7.0)
        a.comp_mul(b)
        assert np.allclose(a.sample_trilinear_index_space(shared), 12.0)

        # comp_div
        a, b = make_pair(12.0, 5.0, 4.0, 1.0)
        a.comp_div(b)
        assert np.allclose(a.sample_trilinear_index_space(shared), 3.0)

    def test_dense(self, cube):
        mesh = cube.clone()
        for ext in GRID_TYPES:
            for comp in [
                lagrange.volume.Compression.Uncompressed,
                lagrange.volume.Compression.Zip,
                lagrange.volume.Compression.Blosc,
            ]:
                sparse_grid = Grid.from_mesh(mesh)
                sparse_buffer = sparse_grid.to_buffer(grid_type=ext, compression=comp)
                assert type(sparse_buffer) is bytes
                dense_grid = sparse_grid.densify().redistance()
                dense_buffer = dense_grid.to_buffer(grid_type=ext, compression=comp)
                coarse_grid = dense_grid.resample(voxel_size=-2)
                assert type(dense_buffer) is bytes
                mesh1 = sparse_grid.to_mesh()
                mesh2 = dense_grid.to_mesh()
                mesh3 = coarse_grid.to_mesh()
                assert mesh1.num_vertices > 0
                assert mesh1.num_facets > 0
                assert mesh3.num_vertices > 0
                assert mesh3.num_facets > 0
                assert mesh3.num_vertices < mesh1.num_vertices
                assert mesh1.num_vertices == mesh2.num_vertices
                assert mesh1.num_facets == mesh2.num_facets
