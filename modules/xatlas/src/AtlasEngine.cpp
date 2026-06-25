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
#include "AtlasEngine.h"

#include "internal/atlas_to_mesh.h"
#include "internal/mesh_to_xatlas.h"
#include "internal/progress_bridge.h"

#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/utils/Error.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/fmt/format.h>

#include <xatlas/xatlas.h>

namespace lagrange::xatlas {

template <typename Scalar, typename Index>
struct AtlasEngine<Scalar, Index>::Impl
{
    enum class State {
        Empty, ///< No meshes added yet.
        UnwrapAdded, ///< add_mesh() called.
        UvAdded, ///< add_uv_mesh() called.
        ChartsComputed, ///< compute_charts() called (unwrap path).
        Packed, ///< pack_charts() called.
    };

    Impl() { atlas = ::xatlas::Create(); }

    ~Impl()
    {
        if (atlas != nullptr) {
            ::xatlas::Destroy(atlas);
            atlas = nullptr;
        }
    }

    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;

    ::xatlas::Atlas* atlas = nullptr;
    State state = State::Empty;

    // Owning storage for the input adapters. The active vector depends on state.
    std::vector<internal::MeshAdapter> unwrap_adapters;
    std::vector<internal::UvMeshAdapter> uv_adapters;

    // Original meshes (for extraction). Same order as adapters were added.
    std::vector<SurfaceMesh<Scalar, Index>> input_meshes;

    // Optional progress/cancel.
    std::function<void(const std::string&, float)> notification_func;
    const std::atomic_bool* cancel = nullptr;

    void reset_atlas()
    {
        if (atlas != nullptr) {
            ::xatlas::Destroy(atlas);
        }
        atlas = ::xatlas::Create();
        state = State::Empty;
        input_meshes.clear();
        unwrap_adapters.clear();
        uv_adapters.clear();
    }
};

namespace {

template <typename Impl>
void install_progress(Impl& impl, internal::ProgressBridge& bridge)
{
    bridge.install(impl.atlas);
}

void check_add_mesh_error(::xatlas::AddMeshError err)
{
    if (err != ::xatlas::AddMeshError::Success) {
        throw Error(
            lagrange::format("lagrange::xatlas: AddMesh failed: {}", ::xatlas::StringForEnum(err)));
    }
}

} // namespace

template <typename Scalar, typename Index>
AtlasEngine<Scalar, Index>::AtlasEngine()
    : m_impl(std::make_unique<Impl>())
{}

template <typename Scalar, typename Index>
AtlasEngine<Scalar, Index>::~AtlasEngine() = default;

template <typename Scalar, typename Index>
AtlasEngine<Scalar, Index>::AtlasEngine(AtlasEngine&&) noexcept = default;

template <typename Scalar, typename Index>
AtlasEngine<Scalar, Index>& AtlasEngine<Scalar, Index>::operator=(AtlasEngine&&) noexcept = default;

template <typename Scalar, typename Index>
void AtlasEngine<Scalar, Index>::add_mesh(const Mesh& mesh, const UnwrapOptions& options)
{
    auto& impl = *m_impl;
    if (impl.state == Impl::State::UvAdded) {
        throw Error(
            "lagrange::xatlas::AtlasEngine: cannot mix add_mesh() and add_uv_mesh(); call clear() "
            "first");
    }
    if (impl.state == Impl::State::ChartsComputed || impl.state == Impl::State::Packed) {
        throw Error(
            "lagrange::xatlas::AtlasEngine: cannot add_mesh() after compute_charts() or "
            "pack_charts(); call clear() first");
    }

    // Strong exception safety: append to engine state, then roll back on any failure / cancel
    // so callers don't have to call clear() to recover.
    impl.unwrap_adapters.emplace_back(internal::build_mesh_adapter(mesh, options));
    bool committed = false;
    bool input_appended = false;
    auto rollback_guard = [&]() noexcept {
        la_debug_assert(
            impl.unwrap_adapters.size() == impl.input_meshes.size() + (input_appended ? 0 : 1));
        if (!committed) {
            impl.unwrap_adapters.pop_back();
            if (input_appended) {
                impl.input_meshes.pop_back();
            }
        }
    };
    try {
        impl.input_meshes.emplace_back(mesh);
        input_appended = true;

        internal::ProgressBridge bridge(impl.notification_func, impl.cancel);
        install_progress(impl, bridge);

        auto decl = impl.unwrap_adapters.back().as_decl();
        auto err = ::xatlas::AddMesh(impl.atlas, decl, /*meshCountHint=*/0);
        if (impl.notification_func || impl.cancel != nullptr) {
            ::xatlas::AddMeshJoin(impl.atlas);
        }
        bridge.rethrow_if_pending();
        if (bridge.was_cancelled()) {
            throw Error("xatlas operation cancelled");
        }
        check_add_mesh_error(err);
        committed = true;
        impl.state = Impl::State::UnwrapAdded;
    } catch (...) {
        rollback_guard();
        throw;
    }
}

template <typename Scalar, typename Index>
void AtlasEngine<Scalar, Index>::add_uv_mesh(const Mesh& mesh, const RepackOptions& options)
{
    auto& impl = *m_impl;
    if (impl.state == Impl::State::UnwrapAdded || impl.state == Impl::State::ChartsComputed ||
        impl.state == Impl::State::Packed) {
        throw Error(
            "lagrange::xatlas::AtlasEngine: cannot mix add_uv_mesh() with the unwrap path; call "
            "clear() first");
    }

    impl.uv_adapters.emplace_back(
        internal::build_uv_mesh_adapter(
            mesh,
            options.input_uv_attribute_name,
            options.input_chart_attribute_name));
    bool committed = false;
    bool input_appended = false;
    auto rollback_guard = [&]() noexcept {
        la_debug_assert(
            impl.uv_adapters.size() == impl.input_meshes.size() + (input_appended ? 0 : 1));
        if (!committed) {
            impl.uv_adapters.pop_back();
            if (input_appended) {
                impl.input_meshes.pop_back();
            }
        }
    };
    try {
        impl.input_meshes.emplace_back(mesh);
        input_appended = true;

        internal::ProgressBridge bridge(impl.notification_func, impl.cancel);
        install_progress(impl, bridge);

        auto decl = impl.uv_adapters.back().as_decl();
        auto err = ::xatlas::AddUvMesh(impl.atlas, decl);
        bridge.rethrow_if_pending();
        if (bridge.was_cancelled()) {
            throw Error("xatlas operation cancelled");
        }
        check_add_mesh_error(err);
        committed = true;
        impl.state = Impl::State::UvAdded;
    } catch (...) {
        rollback_guard();
        throw;
    }
}

template <typename Scalar, typename Index>
void AtlasEngine<Scalar, Index>::clear()
{
    m_impl->reset_atlas();
}

template <typename Scalar, typename Index>
void AtlasEngine<Scalar, Index>::compute_charts(const ChartOptions& opts)
{
    auto& impl = *m_impl;
    if (impl.state != Impl::State::UnwrapAdded && impl.state != Impl::State::ChartsComputed &&
        impl.state != Impl::State::Packed) {
        throw Error(
            "lagrange::xatlas::AtlasEngine: compute_charts() requires at least one add_mesh() "
            "call; the repack path does not use compute_charts()");
    }

    ::xatlas::ChartOptions x;
    x.maxChartArea = opts.max_chart_area;
    x.maxBoundaryLength = opts.max_boundary_length;
    x.normalDeviationWeight = opts.normal_deviation_weight;
    x.roundnessWeight = opts.roundness_weight;
    x.straightnessWeight = opts.straightness_weight;
    x.normalSeamWeight = opts.normal_seam_weight;
    x.textureSeamWeight = opts.texture_seam_weight;
    x.maxCost = opts.max_cost;
    x.maxIterations = opts.max_iterations;
    x.useInputMeshUvs = opts.use_input_mesh_uvs;
    x.fixWinding = opts.fix_winding;

    internal::ProgressBridge bridge(impl.notification_func, impl.cancel);
    install_progress(impl, bridge);

    ::xatlas::ComputeCharts(impl.atlas, x);

    bridge.rethrow_if_pending();
    if (bridge.was_cancelled()) {
        throw Error("xatlas operation cancelled");
    }
    impl.state = Impl::State::ChartsComputed;
}

template <typename Scalar, typename Index>
void AtlasEngine<Scalar, Index>::pack_charts(const PackOptions& opts)
{
    auto& impl = *m_impl;
    if (impl.state == Impl::State::Empty) {
        throw Error("lagrange::xatlas::AtlasEngine: pack_charts() requires meshes to be added");
    }

    if (impl.state == Impl::State::UnwrapAdded) {
        throw Error(
            "lagrange::xatlas::AtlasEngine: pack_charts() requires compute_charts() to be called "
            "first on the unwrap path");
    }

    // xatlas requires ComputeCharts before PackCharts. On the repack/UV-mesh path, callers are
    // allowed to go directly from add_uv_mesh() to pack_charts(), so compute charts implicitly
    // here once for that flow only.
    if (impl.state == Impl::State::UvAdded) {
        internal::ProgressBridge cc_bridge(impl.notification_func, impl.cancel);
        install_progress(impl, cc_bridge);
        ::xatlas::ChartOptions cc_opts;
        ::xatlas::ComputeCharts(impl.atlas, cc_opts);
        cc_bridge.rethrow_if_pending();
        if (cc_bridge.was_cancelled()) {
            throw Error("xatlas operation cancelled");
        }
    }

    ::xatlas::PackOptions x;
    x.maxChartSize = opts.max_chart_size;
    x.padding = opts.padding;
    x.texelsPerUnit = opts.texels_per_unit;
    x.resolution = opts.resolution;
    x.bilinear = opts.bilinear;
    x.blockAlign = opts.block_align;
    x.bruteForce = opts.brute_force;
    x.rotateChartsToAxis = opts.rotate_charts_to_axis;
    x.rotateCharts = opts.rotate_charts;

    internal::ProgressBridge bridge(impl.notification_func, impl.cancel);
    install_progress(impl, bridge);

    ::xatlas::PackCharts(impl.atlas, x);

    bridge.rethrow_if_pending();
    if (bridge.was_cancelled()) {
        throw Error("xatlas operation cancelled");
    }
    impl.state = Impl::State::Packed;
}

template <typename Scalar, typename Index>
void AtlasEngine<Scalar, Index>::generate(
    const ChartOptions& chart_opts,
    const PackOptions& pack_opts)
{
    compute_charts(chart_opts);
    pack_charts(pack_opts);
}

template <typename Scalar, typename Index>
void AtlasEngine<Scalar, Index>::generate(const UnwrapOptions& opts)
{
    generate(opts.chart, opts.packing);
}

template <typename Scalar, typename Index>
auto AtlasEngine<Scalar, Index>::extract_mesh(
    size_t mesh_index,
    MultiAtlasPolicy multi_atlas_policy,
    std::string_view output_uv_attribute_name,
    std::string_view output_atlas_attribute_name,
    std::string_view output_chart_attribute_name) const -> Mesh
{
    auto& impl = *m_impl;
    if (impl.state != Impl::State::Packed) {
        throw Error(
            "lagrange::xatlas::AtlasEngine: extract_mesh() requires pack_charts() to have been "
            "called");
    }
    if (mesh_index >= impl.input_meshes.size()) {
        throw Error(
            lagrange::format(
                "lagrange::xatlas::AtlasEngine: mesh_index {} out of range (added {} meshes)",
                mesh_index,
                impl.input_meshes.size()));
    }

    Mesh out = impl.input_meshes[mesh_index];
    internal::apply_atlas_uvs_to_mesh(
        out,
        *impl.atlas,
        static_cast<uint32_t>(mesh_index),
        multi_atlas_policy,
        output_uv_attribute_name,
        output_atlas_attribute_name,
        output_chart_attribute_name);
    return out;
}

template <typename Scalar, typename Index>
size_t AtlasEngine<Scalar, Index>::mesh_count() const
{
    return m_impl->input_meshes.size();
}

template <typename Scalar, typename Index>
uint32_t AtlasEngine<Scalar, Index>::atlas_width() const
{
    return m_impl->atlas != nullptr ? m_impl->atlas->width : 0u;
}

template <typename Scalar, typename Index>
uint32_t AtlasEngine<Scalar, Index>::atlas_height() const
{
    return m_impl->atlas != nullptr ? m_impl->atlas->height : 0u;
}

template <typename Scalar, typename Index>
uint32_t AtlasEngine<Scalar, Index>::atlas_count() const
{
    return m_impl->atlas != nullptr ? m_impl->atlas->atlasCount : 0u;
}

template <typename Scalar, typename Index>
uint32_t AtlasEngine<Scalar, Index>::chart_count() const
{
    return m_impl->atlas != nullptr ? m_impl->atlas->chartCount : 0u;
}

template <typename Scalar, typename Index>
float AtlasEngine<Scalar, Index>::texels_per_unit() const
{
    return m_impl->atlas != nullptr ? m_impl->atlas->texelsPerUnit : 0.f;
}

template <typename Scalar, typename Index>
float AtlasEngine<Scalar, Index>::utilization(uint32_t atlas_index) const
{
    auto* atlas = m_impl->atlas;
    if (atlas == nullptr || atlas->atlasCount == 0 || atlas->utilization == nullptr) return 0.f;
    if (atlas_index >= atlas->atlasCount) {
        throw Error(
            lagrange::format(
                "lagrange::xatlas::AtlasEngine: atlas_index {} out of range (atlas_count = {})",
                atlas_index,
                atlas->atlasCount));
    }
    return atlas->utilization[atlas_index];
}

template <typename Scalar, typename Index>
void AtlasEngine<Scalar, Index>::set_notification_func(NotificationFunc func)
{
    m_impl->notification_func = std::move(func);
}

template <typename Scalar, typename Index>
void AtlasEngine<Scalar, Index>::set_cancel(const std::atomic_bool* cancel)
{
    m_impl->cancel = cancel;
}

#define LA_X_atlas_engine(_, S, I) template class AtlasEngine<S, I>;
LA_SURFACE_MESH_X(atlas_engine, 0)
#undef LA_X_atlas_engine

} // namespace lagrange::xatlas
