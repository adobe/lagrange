/*
 * Copyright 2020 Adobe. All rights reserved.
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
#include <lagrange/raycasting/create_ray_caster.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
// #include <igl/embree/EmbreeIntersector.h>
#include <igl/random_dir.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <lagrange/testing/common.h>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <Eigen/Core>

#include <cmath>

namespace {

template <typename RayCasterPtr, typename Mesh>
size_t raycast_ambient_occlusion(RayCasterPtr& caster, Mesh& mesh, Eigen::MatrixXd& directions)
{
    using RayCaster = typename RayCasterPtr::element_type;
    using Scalar = typename RayCaster::Scalar;
    using Point = typename RayCaster::Point;
    using Direction = typename RayCaster::Direction;
    std::atomic_size_t hit_counter(0);
    int num_vertices = mesh.get_num_vertices();
    tbb::parallel_for(0, num_vertices, [&](int v) {
        Point origin = mesh.get_vertices().row(v).transpose().template cast<Scalar>();
        for (int d = 0; d < directions.rows(); ++d) {
            Direction direction = directions.row(d).transpose().template cast<Scalar>();
            size_t mesh_index;
            size_t instance_index;
            size_t facet_index;
            Scalar ray_depth;
            Point bc;
            Point normal;
            bool hit = caster->cast(
                origin,
                direction,
                mesh_index,
                instance_index,
                facet_index,
                ray_depth,
                bc,
                normal);
            if (hit) {
                ++hit_counter;
            }
        }
    });
    return hit_counter;
}


template <typename RayCasterPtr, typename Mesh>
size_t
raycast_ambient_occlusion_pack4(RayCasterPtr& caster, Mesh& mesh, Eigen::MatrixXd& directions_)
{
    using RayCaster = typename RayCasterPtr::element_type;
    using Scalar = typename RayCaster::Scalar;
    using Scalar4 = typename RayCaster::Scalar4;
    using Index4 = typename RayCaster::Index4;
    using Point4 = typename RayCaster::Point4;
    using Direction4 = typename RayCaster::Direction4;
    using Mask4 = typename RayCaster::Mask4;
    std::atomic_size_t hit_counter(0);
    int num_vertices = mesh.get_num_vertices();
    int num_directions = static_cast<int>(directions_.rows());

    auto bitcount4 = [](uint8_t i) {
        i = i - ((i >> 1) & '\5');
        return (i & '\3') + ((i >> 2) & '\3');
    };

    tbb::parallel_for(0, num_vertices, [&](int v) {
        Point4 origins;
        origins.row(0) = mesh.get_vertices().row(v).template cast<Scalar>();
        for (int i = 1; i < 4; ++i) origins.row(i) = origins.row(0);
        Mask4 mask = Mask4(-1, -1, -1, -1);

        for (int d = 0; d < num_directions; d += 4) {
            int batchsize = std::min(num_directions - d, 4);
            Direction4 directions;
            Index4 mesh_indices;
            Index4 instance_indices;
            Index4 facet_indices;
            Scalar4 ray_depths;
            Point4 bc;
            Point4 normal;

            for (int i = 0; i < batchsize; ++i) {
                directions.row(i) = directions.row(d + i).template cast<Scalar>();
            }

            for (int i = batchsize; i < 4; ++i) {
                mask(i) = 0;
            }

            uint8_t hit4 = caster->cast4(
                batchsize,
                origins,
                directions,
                mask,
                mesh_indices,
                instance_indices,
                facet_indices,
                ray_depths,
                bc,
                normal);

            if (hit4) {
                hit_counter += static_cast<size_t>(bitcount4(hit4));
            }
        }
    });
    return hit_counter;
}

// TODO: Re-enable after moving to Embree 4
#if 0
template <typename Mesh>
size_t igl_ambient_occlusion(
    const igl::embree::EmbreeIntersector& engine,
    Mesh& mesh,
    Eigen::MatrixXd& directions)
{
    using Scalar = float;
    std::atomic_size_t hit_counter(0);
    int num_vertices = mesh.get_num_vertices();
    tbb::parallel_for(0, num_vertices, [&](int v) {
        Eigen::Vector3f origin = mesh.get_vertices().row(v).transpose().template cast<Scalar>();
        for (int d = 0; d < directions.rows(); ++d) {
            Eigen::Vector3f direction = directions.row(d).transpose().template cast<Scalar>();
            igl::Hit hit;
            bool ret = engine.intersectRay(origin, direction, hit);
            if (ret) {
                ++hit_counter;
            }
        }
    });
    return hit_counter;
}
#endif

} // namespace

TEST_CASE("Raycasting Speed", "[raycasting][!benchmark]")
{
    int num_samples = 50;
    Eigen::MatrixXd directions = igl::random_dir_stratified(num_samples);

    // use a mesh that spends affordable time
    auto mesh = lagrange::to_shared_ptr(
        lagrange::testing::load_mesh<lagrange::TriangleMesh3D>("open/core/cube_soup.obj"));

    using namespace lagrange::raycasting;

    std::vector<std::pair<std::string, RayCasterType>> all_types = {
        {"default", EMBREE_DEFAULT},
        {"dynamic", EMBREE_DYNAMIC},
        {"robust", EMBREE_ROBUST},
        {"compact", EMBREE_COMPACT},
        {"dynamic_compact", static_cast<RayCasterType>(EMBREE_DYNAMIC | EMBREE_COMPACT)},
        {"robust_compact", static_cast<RayCasterType>(EMBREE_ROBUST | EMBREE_COMPACT)},
    };

    std::vector<std::pair<std::string, RayCasterQuality>> all_qualities = {
        {"low", BUILD_QUALITY_LOW},
        {"medium", BUILD_QUALITY_MEDIUM},
        {"high", BUILD_QUALITY_HIGH},
    };

    // Libigl raycaster
    #if 0
    {
        igl::embree::EmbreeIntersector engine;
        engine.init(mesh->get_vertices().cast<float>(), mesh->get_facets().cast<int>());
        BENCHMARK("libigl") { return igl_ambient_occlusion(engine, *mesh, directions); };
    }
    #endif

    // Lagrange 4-packed raycaster
    for (auto type : all_types) {
        for (auto quality : all_qualities) {
            std::string name = fmt::format("{} + {}", type.first, quality.first);
            auto engine =
                lagrange::raycasting::create_ray_caster<float>(type.second, quality.second);
            engine->add_mesh(mesh, Eigen::Matrix4f::Identity());
            engine->cast({0, 0, 0}, {0, 0, 1}); // triggers scene update
            BENCHMARK(name + " (4-packed)")
            {
                return raycast_ambient_occlusion_pack4(engine, *mesh, directions);
            };
        }
    }

    // Lagrange single-ray raycaster
    for (auto type : all_types) {
        for (auto quality : all_qualities) {
            std::string name = fmt::format("{} + {}", type.first, quality.first);
            auto engine =
                lagrange::raycasting::create_ray_caster<float>(type.second, quality.second);
            engine->add_mesh(mesh, Eigen::Matrix4f::Identity());
            engine->cast({0, 0, 0}, {0, 0, 1}); // triggers scene update
            BENCHMARK(name + " (single ray)")
            {
                return raycast_ambient_occlusion(engine, *mesh, directions);
            };
        }
    }
}
