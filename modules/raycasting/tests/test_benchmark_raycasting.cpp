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

#include <lagrange/Mesh.h>
#include <lagrange/common.h>
#include <lagrange/io/load_mesh.h>
#include <lagrange/mesh_convert.h>
#include <lagrange/raycasting/RayCaster.h>
#include <lagrange/raycasting/create_ray_caster.h>
#include <lagrange/views.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <igl/random_dir.h>
#include <tbb/parallel_for.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <lagrange/testing/common.h>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <cmath>

namespace {

using Scalar = double;
using Index = uint32_t;

///
/// Ambient occlusion via the legacy EmbreeRayCaster (single-ray).
///
template <typename RayCasterPtr, typename LegacyMesh>
size_t legacy_ao_single(RayCasterPtr& caster, LegacyMesh& mesh, const Eigen::MatrixXd& directions)
{
    using RC = typename RayCasterPtr::element_type;
    using RCScalar = typename RC::Scalar;
    using Point = typename RC::Point;
    using Direction = typename RC::Direction;

    std::atomic_size_t hit_counter(0);
    const int nv = static_cast<int>(mesh.get_num_vertices());
    tbb::parallel_for(0, nv, [&](int v) {
        Point origin = mesh.get_vertices().row(v).transpose().template cast<RCScalar>();
        for (int d = 0; d < directions.rows(); ++d) {
            Direction dir = directions.row(d).transpose().template cast<RCScalar>();
            size_t mesh_index, instance_index, facet_index;
            RCScalar ray_depth;
            Point bc, normal;
            bool hit = caster->cast(
                origin,
                dir,
                mesh_index,
                instance_index,
                facet_index,
                ray_depth,
                bc,
                normal);
            if (hit) ++hit_counter;
        }
    });
    return hit_counter;
}

///
/// Ambient occlusion via the legacy EmbreeRayCaster (4-packed).
///
template <typename RayCasterPtr, typename LegacyMesh>
size_t legacy_ao_pack4(RayCasterPtr& caster, LegacyMesh& mesh, const Eigen::MatrixXd& directions)
{
    using RC = typename RayCasterPtr::element_type;
    using RCScalar = typename RC::Scalar;
    using Point4 = typename RC::Point4;
    using Direction4 = typename RC::Direction4;
    using Index4 = typename RC::Index4;
    using Scalar4 = typename RC::Scalar4;
    using Mask4 = typename RC::Mask4;

    auto popcount4 = [](uint32_t x) -> size_t {
        size_t c = 0;
        for (int i = 0; i < 4; ++i)
            if (x & (1u << i)) ++c;
        return c;
    };

    std::atomic_size_t hit_counter(0);
    const int nv = static_cast<int>(mesh.get_num_vertices());
    const int nd = static_cast<int>(directions.rows());

    tbb::parallel_for(0, nv, [&](int v) {
        Point4 origins;
        origins.row(0) = mesh.get_vertices().row(v).template cast<RCScalar>();
        for (int i = 1; i < 4; ++i) origins.row(i) = origins.row(0);
        Mask4 mask = Mask4::Constant(-1);

        for (int d = 0; d < nd; d += 4) {
            const int batch = std::min(nd - d, 4);
            Direction4 dirs;
            for (int i = 0; i < batch; ++i)
                dirs.row(i) = directions.row(d + i).template cast<RCScalar>();

            auto active = mask;
            for (int i = batch; i < 4; ++i) active(i) = 0;

            Index4 mesh_indices, instance_indices, facet_indices;
            Scalar4 ray_depths;
            Point4 bc, normal;
            uint32_t hits = caster->cast4(
                static_cast<uint32_t>(batch),
                origins,
                dirs,
                active,
                mesh_indices,
                instance_indices,
                facet_indices,
                ray_depths,
                bc,
                normal);
            if (hits) hit_counter += popcount4(hits);
        }
    });
    return hit_counter;
}

///
/// Ambient occlusion via the new RayCaster (single-ray).
///
size_t new_ao_single(
    const lagrange::raycasting::RayCaster& caster,
    const lagrange::SurfaceMesh<Scalar, Index>& mesh,
    const Eigen::MatrixXd& directions)
{
    std::atomic_size_t hit_counter(0);
    const auto V = lagrange::vertex_view(mesh);
    const int nv = static_cast<int>(mesh.get_num_vertices());
    tbb::parallel_for(0, nv, [&](int v) {
        Eigen::Vector3f origin = V.row(v).transpose().template cast<float>();
        for (int d = 0; d < directions.rows(); ++d) {
            Eigen::Vector3f dir = directions.row(d).transpose().template cast<float>();
            auto hit = caster.cast(origin, dir);
            if (hit) ++hit_counter;
        }
    });
    return hit_counter;
}

///
/// Ambient occlusion via the new RayCaster (4-packed).
///
template <size_t N>
size_t new_ao_pack(
    const lagrange::raycasting::RayCaster& caster,
    const lagrange::SurfaceMesh<Scalar, Index>& mesh,
    const Eigen::MatrixXd& directions)
{
    using PointNf = Eigen::Matrix<float, N, 3, Eigen::RowMajor>;
    using DirectionNf = Eigen::Matrix<float, N, 3, Eigen::RowMajor>;

    auto popcountN = [](uint32_t x) -> size_t {
        size_t c = 0;
        for (size_t i = 0; i < N; ++i)
            if (x & (1u << i)) ++c;
        return c;
    };

    std::atomic_size_t hit_counter(0);
    const auto V = lagrange::vertex_view(mesh);
    const size_t nv = static_cast<size_t>(mesh.get_num_vertices());
    const size_t nd = static_cast<size_t>(directions.rows());

    tbb::parallel_for(size_t(0), nv, [&](size_t v) {
        Eigen::Vector3f origin = V.row(v).transpose().template cast<float>();

        for (size_t d = 0; d < nd; d += N) {
            const size_t batch = std::min<int>(nd - d, N);
            PointNf origins;
            DirectionNf dirs;
            for (size_t i = 0; i < N; ++i) origins.row(i) = origin.transpose();
            for (size_t i = 0; i < batch; ++i)
                dirs.row(i) = directions.row(d + i).template cast<float>();
            for (size_t i = batch; i < N; ++i) dirs.row(i).setZero();

            if constexpr (N == 4) {
                auto result = caster.cast4(origins, dirs, static_cast<size_t>(batch));
                if (result.valid_mask) hit_counter += popcountN(result.valid_mask);
            } else if constexpr (N == 16) {
                auto result = caster.cast16(origins, dirs, static_cast<size_t>(batch));
                if (result.valid_mask) hit_counter += popcountN(result.valid_mask);
            }
        }
    });
    return hit_counter;
}

} // namespace

TEST_CASE("Raycasting Benchmark", "[raycasting][!benchmark]")
{
    const int num_samples = 32;
    Eigen::MatrixXd directions = igl::random_dir_stratified(num_samples);

    // Load the dragon mesh for both APIs.
    auto legacy_mesh = lagrange::to_shared_ptr(
        lagrange::testing::load_mesh<lagrange::TriangleMesh3D>("open/core/dragon.obj"));
    auto surface_mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/dragon.obj");

    INFO(
        "Dragon mesh: " << surface_mesh.get_num_vertices() << " vertices, "
                        << surface_mesh.get_num_facets() << " facets");

    using namespace lagrange::raycasting;

    // -------------------------------------------------------------------------
    // Build the legacy EmbreeRayCaster (robust, high quality)
    // -------------------------------------------------------------------------
    auto legacy_caster = create_ray_caster<float>(EMBREE_ROBUST, BUILD_QUALITY_HIGH);
    legacy_caster->add_mesh(legacy_mesh, Eigen::Matrix4f::Identity());
    // Trigger scene build with a dummy cast.
    legacy_caster->cast(Eigen::Vector3f(0, 0, 0), Eigen::Vector3f(0, 0, 1));

    // -------------------------------------------------------------------------
    // Build the new RayCaster (robust, high quality)
    // -------------------------------------------------------------------------
    RayCaster new_caster(SceneFlags::Robust, BuildQuality::High);
    {
        auto mesh_copy = surface_mesh; // keep original for vertex access
        new_caster.add_mesh(std::move(mesh_copy));
        new_caster.commit_updates();
    }

    // -------------------------------------------------------------------------
    // Single-ray benchmarks
    // -------------------------------------------------------------------------
    BENCHMARK("Legacy EmbreeRayCaster (single ray)")
    {
        return legacy_ao_single(legacy_caster, *legacy_mesh, directions);
    };

    BENCHMARK("New RayCaster (single ray)")
    {
        return new_ao_single(new_caster, surface_mesh, directions);
    };

    // -------------------------------------------------------------------------
    // 4-packed ray benchmarks
    // -------------------------------------------------------------------------
    BENCHMARK("Legacy EmbreeRayCaster (4-packed)")
    {
        return legacy_ao_pack4(legacy_caster, *legacy_mesh, directions);
    };

    BENCHMARK("New RayCaster (4-packed)")
    {
        return new_ao_pack<4>(new_caster, surface_mesh, directions);
    };

    // -------------------------------------------------------------------------
    // 16-packed ray benchmarks
    // -------------------------------------------------------------------------
    BENCHMARK("New RayCaster (16-packed)")
    {
        return new_ao_pack<16>(new_caster, surface_mesh, directions);
    };
}
