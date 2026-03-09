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

#include <lagrange/Attribute.h>
#include <lagrange/SurfaceMesh.h>
#include <lagrange/bvh/BVHNanoflann.h>
#include <lagrange/io/load_mesh.h>
#include <lagrange/mesh_convert.h>
#include <lagrange/raycasting/RayCaster.h>
#include <lagrange/raycasting/project_closest_vertex.h>
#include <lagrange/views.h>

#include <lagrange/testing/common.h>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <tbb/parallel_for.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <random>

namespace {

using Scalar = double;
using Index = uint32_t;
using MeshType = lagrange::SurfaceMesh<Scalar, Index>;

/// Create a source triangle mesh with a vertex attribute "color". Uses the hemisphere test asset
/// which has a moderate number of vertices and triangles.
MeshType create_source_mesh()
{
    auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/dragon.obj");

    auto V = lagrange::vertex_view(mesh);
    const Index nv = mesh.get_num_vertices();
    std::vector<Scalar> values(static_cast<size_t>(nv) * 3);
    for (Index v = 0; v < nv; ++v) {
        // Use normalized position as a synthetic color attribute.
        Eigen::RowVector3<Scalar> p = V.row(v).normalized();
        values[3 * v + 0] = p.x();
        values[3 * v + 1] = p.y();
        values[3 * v + 2] = p.z();
    }
    mesh.template create_attribute<Scalar>(
        "color",
        lagrange::AttributeElement::Vertex,
        3,
        lagrange::AttributeUsage::Color,
        {values.data(), values.size()});

    return mesh;
}

/// Create a target mesh by perturbing vertex positions of the source.
MeshType create_target_mesh(const MeshType& source, double perturbation = 0.01)
{
    MeshType target = source;
    auto V = lagrange::vertex_ref(target);

    std::mt19937 gen(42);
    std::uniform_real_distribution<double> dist(-perturbation, perturbation);
    for (Index i = 0; i < target.get_num_vertices(); ++i) {
        for (int c = 0; c < 3; ++c) {
            V(i, c) += static_cast<Scalar>(dist(gen));
        }
    }

    // Remove the "color" attribute from the target so it can be projected.
    if (target.has_attribute("color")) {
        target.delete_attribute("color");
    }

    return target;
}

/// Nanoflann-based closest-vertex attribute transfer (the legacy approach).
///
/// Builds a kd-tree over the source mesh vertices, then for each target vertex finds the closest
/// source vertex and copies the attribute value.
void nanoflann_closest_vertex(
    const MeshType& source,
    MeshType& target,
    lagrange::AttributeId src_attr_id)
{
    using VertexMatrix = Eigen::Matrix<Scalar, Eigen::Dynamic, 3, Eigen::RowMajor>;
    using BVH = lagrange::bvh::BVHNanoflann<VertexMatrix>;

    // Extract source vertices into an Eigen matrix for Nanoflann.
    auto src_V = lagrange::vertex_view(source);
    VertexMatrix src_vertices = src_V;

    auto engine = std::make_unique<BVH>();
    engine->build(src_vertices);

    const auto& src_attr = source.template get_attribute<Scalar>(src_attr_id);
    size_t num_channels = src_attr.get_num_channels();
    auto src_span = src_attr.get_all();

    // Create target attribute.
    lagrange::AttributeId dst_attr_id = lagrange::invalid<lagrange::AttributeId>();
    if (target.has_attribute("color")) {
        dst_attr_id = target.get_attribute_id("color");
    } else {
        dst_attr_id = target.template create_attribute<Scalar>(
            "color",
            lagrange::AttributeElement::Vertex,
            num_channels,
            lagrange::AttributeUsage::Color);
    }
    auto& dst_attr = target.template ref_attribute<Scalar>(dst_attr_id);
    auto dst_span = dst_attr.ref_all();

    auto tgt_V = lagrange::vertex_view(target);

    tbb::parallel_for(Index(0), target.get_num_vertices(), [&](Index i) {
        Eigen::RowVector<Scalar, 3> p = tgt_V.row(i);
        auto res = engine->query_closest_point(p);

        size_t src_offset = static_cast<size_t>(res.closest_vertex_idx) * num_channels;
        size_t dst_offset = static_cast<size_t>(i) * num_channels;
        for (size_t c = 0; c < num_channels; ++c) {
            dst_span[dst_offset + c] = src_span[src_offset + c];
        }
    });
}

} // namespace

TEST_CASE("Closest Vertex Benchmark", "[raycasting][!benchmark]")
{
    auto source = create_source_mesh();
    auto src_attr_id = source.get_attribute_id("color");
    const Index nv = source.get_num_vertices();

    INFO("Source mesh: " << nv << " vertices, " << source.get_num_facets() << " facets");

    // Pre-build the Embree ray caster (construction time is excluded from the benchmark).
    lagrange::raycasting::RayCaster caster(
        lagrange::raycasting::SceneFlags::Robust,
        lagrange::raycasting::BuildQuality::High);
    {
        MeshType source_copy = source;
        caster.add_mesh(std::move(source_copy));
        caster.commit_updates();
    }

    BENCHMARK("Embree closest_vertex")
    {
        auto target = create_target_mesh(source);
        lagrange::raycasting::ProjectCommonOptions opts;
        opts.attribute_ids = {src_attr_id};
        opts.project_vertices = false;
        lagrange::raycasting::project_closest_vertex(source, target, opts, &caster);
        return target.get_num_vertices();
    };

    BENCHMARK("Nanoflann closest_vertex")
    {
        auto target = create_target_mesh(source);
        nanoflann_closest_vertex(source, target, src_attr_id);
        return target.get_num_vertices();
    };
}
