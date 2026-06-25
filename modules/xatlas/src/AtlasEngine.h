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

#include <lagrange/SurfaceMesh.h>
#include <lagrange/xatlas/Options.h>
#include <lagrange/xatlas/api.h>

#include <memory>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

namespace lagrange::xatlas {

///
/// Reusable xatlas engine. Wraps an `xatlas::Atlas*` so callers can run the chart-then-pack
/// pipeline once and re-pack with different `PackOptions` without rebuilding charts. This is the
/// efficient path for repeated calls; one-shot users should prefer the free functions.
///
/// State machine:
/// - After construction the atlas is empty.
/// - Call `add_mesh()` (one or more times) for the unwrap path, OR call `add_uv_mesh()` (one or
///   more times) for the repack-only path. Mixing the two is rejected.
/// - For the unwrap path: `compute_charts()` then `pack_charts()` (or `generate()` to do both).
///   `compute_charts()` may be called again to re-segment with new options; this invalidates any
///   previously packed output and requires another `pack_charts()` before extraction.
///   `pack_charts()` may be called repeatedly with different options.
/// - For the repack path: `pack_charts()` directly.
/// - `extract_mesh(i, ...)` reads back the i-th input mesh with the new UVs applied.
/// - `clear()` resets the engine to its initial empty state.
///
template <typename Scalar, typename Index>
class AtlasEngine
{
public:
    using Mesh = SurfaceMesh<Scalar, Index>;
    using NotificationFunc = std::function<void(const std::string&, float)>;

    AtlasEngine();
    ~AtlasEngine();
    AtlasEngine(const AtlasEngine&) = delete;
    AtlasEngine& operator=(const AtlasEngine&) = delete;
    AtlasEngine(AtlasEngine&&) noexcept;
    AtlasEngine& operator=(AtlasEngine&&) noexcept;

    /// Add a mesh for unwrapping. Throws if any UV-only mesh has already been added.
    void add_mesh(const Mesh& mesh, const UnwrapOptions& options = {});

    /// Add a mesh with existing UVs for repacking only. Throws if any unwrap mesh has been added.
    void add_uv_mesh(const Mesh& mesh, const RepackOptions& options = {});

    /// Reset the engine to its initial empty state.
    void clear();

    /// Compute charts. Valid only after at least one `add_mesh()` call.
    void compute_charts(const ChartOptions& opts = {});

    /// Pack charts. Valid after `compute_charts()` (unwrap path) or `add_uv_mesh()` (repack path).
    void pack_charts(const PackOptions& opts);

    /// Convenience: `compute_charts()` + `pack_charts()`.
    void generate(const ChartOptions& chart_opts, const PackOptions& pack_opts);

    /// Convenience: `compute_charts(opts.chart)` + `pack_charts(opts.packing)`.
    void generate(const UnwrapOptions& opts);

    /// Read back the i-th input mesh with the new UVs applied as an indexed attribute.
    Mesh extract_mesh(
        size_t mesh_index,
        MultiAtlasPolicy multi_atlas_policy,
        std::string_view output_uv_attribute_name,
        std::string_view output_atlas_attribute_name,
        std::string_view output_chart_attribute_name) const;

    /// Number of meshes added to the atlas.
    size_t mesh_count() const;

    /// Atlas tile width in pixels (0 before pack).
    uint32_t atlas_width() const;

    /// Atlas tile height in pixels (0 before pack).
    uint32_t atlas_height() const;

    /// Number of atlas tiles xatlas produced (0 before pack).
    uint32_t atlas_count() const;

    /// Total number of charts (0 before chart computation).
    uint32_t chart_count() const;

    /// Effective texels-per-unit value used by the most recent pack.
    float texels_per_unit() const;

    /// Utilization of the given atlas tile (in `[0, 1]`).
    float utilization(uint32_t atlas_index) const;

    /// Set the optional progress callback. xatlas may invoke it from any thread.
    void set_notification_func(NotificationFunc func);

    /// Set the optional cancellation flag.
    void set_cancel(const std::atomic_bool* cancel);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace lagrange::xatlas
