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
import pytest

import numpy as np


class TestImage:
    def test_split_grid_auto(self):
        # Grid shape is (H, W, C) = (256, 384, 3): 2 rows x 3 cols of 128x128 cells.
        grid = np.zeros((256, 384, 3), dtype=np.float32)
        for row in range(2):
            for col in range(3):
                grid[
                    row * 128 : (row + 1) * 128,
                    col * 128 : (col + 1) * 128,
                    :,
                ] = float(row * 3 + col + 1)

        cells = lagrange.image.split_grid(grid, 6)
        assert len(cells) == 6
        for idx, cell in enumerate(cells):
            assert cell.shape == (128, 128, 3)
            assert np.all(cell == float(idx + 1))

    def test_split_grid_explicit_layout(self):
        # Force a 1x6 layout: 6 cells of (128, 64, 3).
        grid = np.zeros((128, 384, 3), dtype=np.float32)
        for col in range(6):
            grid[:, col * 64 : (col + 1) * 64, :] = float(col + 1)

        cells = lagrange.image.split_grid(grid, 6, rows=1, cols=6)
        assert len(cells) == 6
        for idx, cell in enumerate(cells):
            assert cell.shape == (128, 64, 3)
            assert np.all(cell == float(idx + 1))

    def test_split_grid_partial_layout(self):
        # Specify only rows: cols is derived as num_cells / rows.
        grid = np.zeros((256, 256, 3), dtype=np.float32)
        for row in range(2):
            for col in range(2):
                grid[
                    row * 128 : (row + 1) * 128,
                    col * 128 : (col + 1) * 128,
                    :,
                ] = float(row * 2 + col + 1)

        cells = lagrange.image.split_grid(grid, 4, rows=2)
        assert len(cells) == 4
        for idx, cell in enumerate(cells):
            assert cell.shape == (128, 128, 3)
            assert np.all(cell == float(idx + 1))

    def test_split_grid_view_writes_through(self):
        # Mutating a returned cell view should mutate the source grid.
        grid = np.zeros((128, 256, 3), dtype=np.float32)
        cells = lagrange.image.split_grid(grid, 2)
        cells[1][:] = 7.0
        assert np.all(grid[:, 128:, :] == 7.0)
        assert np.all(grid[:, :128, :] == 0.0)

    def test_split_grid_zero_cells_raises(self):
        grid = np.zeros((128, 128, 3), dtype=np.float32)
        with pytest.raises(RuntimeError):
            lagrange.image.split_grid(grid, 0)

    def test_split_grid_indivisible_raises(self):
        # 100x100 grid cannot be evenly split into 3 cells.
        grid = np.zeros((100, 100, 3), dtype=np.float32)
        with pytest.raises(RuntimeError):
            lagrange.image.split_grid(grid, 3)

    def test_split_grid_explicit_mismatch_raises(self):
        # rows=2, cols=2 gives 4 cells but num_cells=3.
        grid = np.zeros((128, 128, 3), dtype=np.float32)
        with pytest.raises(RuntimeError):
            lagrange.image.split_grid(grid, 3, rows=2, cols=2)
