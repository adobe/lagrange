/*
 * Copyright 2026 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#pragma once

#include <lagrange/image/View3D.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/fmt/format.h>

#include <array>
#include <cmath>
#include <limits>
#include <vector>

namespace lagrange::image::experimental {

/// @addtogroup module-image
/// @{

///
/// Options controlling how a grid image is split by split_grid().
///
/// A value of zero on either ``rows`` or ``cols`` means "auto-detect". The resolution rules are:
/// - ``rows = 0, cols = 0``: pick the factorization producing cells closest to square.
/// - ``rows = R, cols = 0``: derive ``cols = num_cells / R``.
/// - ``rows = 0, cols = C``: derive ``rows = num_cells / C``.
/// - ``rows = R, cols = C``: validate ``R * C == num_cells``.
///
/// In all cases, the grid extents must be divisible by the resulting layout.
///
struct SplitGridOptions
{
    /// Number of cells the grid contains.
    size_t num_cells = 0;

    /// Number of cell rows in the grid (0 = auto).
    size_t rows = 0;

    /// Number of cell columns in the grid (0 = auto).
    size_t cols = 0;
};

///
/// Split a grid image into row-major cell views.
///
/// The returned views share memory with the input grid image (no copy). The layout is controlled
/// by ``options``; see SplitGridOptions for resolution rules.
///
/// @param[in] grid     Grid image of shape ``(width, height, channels)`` per View3D axis
///                     ordering (note: the Python binding accepts HxWxC numpy arrays and
///                     transposes the axes internally).
/// @param[in] options  Options including ``num_cells`` and an optional explicit layout.
///
/// @tparam T  Pixel element type (e.g. ``float``, ``const float``, ``uint8_t``).
///
/// @return List of ``options.num_cells`` views into the grid, ordered row-major.
///
template <typename T>
std::vector<View3D<T>> split_grid(View3D<T> grid, const SplitGridOptions& options)
{
    const size_t grid_width = grid.extent(0);
    const size_t grid_height = grid.extent(1);
    const size_t num_channels = grid.extent(2);
    const size_t num_cells = options.num_cells;
    la_runtime_assert(num_cells > 0, "num_cells must be greater than 0");

    auto resolve = [&]() -> std::pair<size_t, size_t> {
        if (options.rows != 0 && options.cols != 0) {
            return {options.rows, options.cols};
        }
        if (options.rows != 0) {
            la_runtime_assert(
                num_cells % options.rows == 0,
                lagrange::format(
                    "Number of cells ({}) is not divisible by rows ({})",
                    num_cells,
                    options.rows));
            return {options.rows, num_cells / options.rows};
        }
        if (options.cols != 0) {
            la_runtime_assert(
                num_cells % options.cols == 0,
                lagrange::format(
                    "Number of cells ({}) is not divisible by cols ({})",
                    num_cells,
                    options.cols));
            return {num_cells / options.cols, options.cols};
        }
        size_t best_rows = 0;
        size_t best_cols = 0;
        double best_aspect_diff = std::numeric_limits<double>::max();
        for (size_t cols = 1; cols <= num_cells; ++cols) {
            if (num_cells % cols != 0) continue;
            size_t rows = num_cells / cols;
            if (grid_width % cols != 0 || grid_height % rows != 0) continue;
            size_t cell_w = grid_width / cols;
            size_t cell_h = grid_height / rows;
            double aspect_diff = std::abs(static_cast<double>(cell_w) / cell_h - 1.0);
            if (aspect_diff < best_aspect_diff) {
                best_aspect_diff = aspect_diff;
                best_rows = rows;
                best_cols = cols;
            }
        }
        la_runtime_assert(
            best_cols > 0,
            lagrange::format(
                "Cannot evenly divide grid image ({}x{}) into {} cells",
                grid_width,
                grid_height,
                num_cells));
        return {best_rows, best_cols};
    };

    auto [rows, cols] = resolve();
    la_runtime_assert(
        rows * cols == num_cells,
        lagrange::format(
            "Layout {}x{} does not match number of cells ({})",
            rows,
            cols,
            num_cells));
    la_runtime_assert(
        grid_width % cols == 0 && grid_height % rows == 0,
        lagrange::format(
            "Grid image ({}x{}) is not divisible by layout ({}x{})",
            grid_width,
            grid_height,
            rows,
            cols));

    const size_t cell_width = grid_width / cols;
    const size_t cell_height = grid_height / rows;

    std::vector<View3D<T>> views;
    views.reserve(num_cells);
    const dextents<size_t, 3> cell_shape{cell_width, cell_height, num_channels};
    const std::array<size_t, 3> cell_strides{grid.stride(0), grid.stride(1), grid.stride(2)};
    const layout_stride::mapping<dextents<size_t, 3>> cell_mapping{cell_shape, cell_strides};
    for (size_t row = 0; row < rows; ++row) {
        for (size_t col = 0; col < cols; ++col) {
            T* cell_ptr = &grid(col * cell_width, row * cell_height, 0);
            views.emplace_back(cell_ptr, cell_mapping);
        }
    }
    return views;
}

/// @}

} // namespace lagrange::image::experimental
