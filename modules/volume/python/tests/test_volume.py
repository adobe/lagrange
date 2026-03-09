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
from lagrange.lagrange.volume import Grid  # why??
import numpy as np
import tempfile
import pathlib

from .assets import cube  # noqa: F401


class TestMeshToVolume:
    def test_bbox(self, cube):
        mesh = cube.clone()
        for dtype in [np.float32, np.float64]:
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
                grid.bbox_world, grid.index_to_world(grid.bbox_index.astype(np.float64))
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
            tmp_dir_path = pathlib.Path(tmp_dir)
            for ext in ["vdb", "nvdb"]:
                tmp_file_path = tmp_dir_path / f"out.{ext}"
                for comp in [
                    lagrange.volume.Compression.Uncompressed,
                    lagrange.volume.Compression.Zip,
                    lagrange.volume.Compression.Blosc,
                ]:
                    grid = Grid.from_mesh(mesh)
                    grid.save(tmp_file_path, compression=comp)
                    buffer = grid.to_buffer(grid_type=ext, compression=comp)
                    assert type(buffer) is bytes
                    mesh1 = Grid.load(tmp_file_path).to_mesh()
                    mesh2 = Grid.load(buffer).to_mesh()
                    assert mesh1.num_vertices > 0
                    assert mesh1.num_facets > 0
                    assert mesh1.num_vertices == mesh2.num_vertices
                    assert mesh1.num_facets == mesh2.num_facets

    def test_offset(self, cube):
        mesh = cube.clone()
        for signing in [
            lagrange.volume.Sign.FloodFill,
            lagrange.volume.Sign.WindingNumber,
            lagrange.volume.Sign.Unsigned,
        ]:
            for dilate in [True, False]:
                if dilate:
                    offset = -1
                else:
                    offset = 1
            grid = Grid.from_mesh(mesh, signing_method=signing)
            active_before = grid.num_active_voxels
            grid.offset_in_place(offset, relative=True)
            active_after = grid.num_active_voxels
            if signing == lagrange.volume.Sign.Unsigned:
                assert active_before == active_after
            else:
                if dilate:
                    assert active_after > active_before
                else:
                    assert active_after < active_before

    def test_dense(self, cube):
        mesh = cube.clone()
        for ext in ["vdb", "nvdb"]:
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
