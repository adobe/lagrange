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

#include <lagrange/SurfaceMesh.h>
#include <lagrange/testing/common.h>
#include <lagrange/utils/Error.h>

#include "../src/AtlasEngine.h"

#include <atomic>
#include <stdexcept>

using Scalar = float;
using Index = uint32_t;

using SurfaceMesh = lagrange::SurfaceMesh<Scalar, Index>;
using AtlasEngine = lagrange::xatlas::AtlasEngine<Scalar, Index>;
using ChartOptions = lagrange::xatlas::ChartOptions;
using PackOptions = lagrange::xatlas::PackOptions;
using RepackOptions = lagrange::xatlas::RepackOptions;
using MultiAtlasPolicy = lagrange::xatlas::MultiAtlasPolicy;
using Error = lagrange::Error;

TEST_CASE("add_mesh -> generate -> extract", "[xatlas][engine]")
{
    const ChartOptions chart_opts;
    const PackOptions pack_opts;
    const MultiAtlasPolicy policy = MultiAtlasPolicy::NormalizePerTile;

    SurfaceMesh mesh;
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({0, 1, 0});
    mesh.add_triangle(0, 1, 2);

    AtlasEngine engine;
    engine.add_mesh(mesh);
    engine.generate(chart_opts, pack_opts);

    REQUIRE(engine.mesh_count() == 1);
    REQUIRE(engine.atlas_count() >= 1);
    REQUIRE(engine.chart_count() >= 1);

    auto out = engine.extract_mesh(0, policy, "uv", "uv_atlas", "uv_chart");
    REQUIRE(out.has_attribute("uv"));
    REQUIRE(out.is_attribute_indexed("uv"));
    REQUIRE(out.has_attribute("uv_atlas"));
    REQUIRE(out.has_attribute("uv_chart"));
}

TEST_CASE("re-pack with new options", "[xatlas][engine]")
{
    const MultiAtlasPolicy policy = MultiAtlasPolicy::NormalizePerTile;

    SurfaceMesh mesh;
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({0, 1, 0});
    mesh.add_vertex({1, 1, 0});
    mesh.add_triangle(0, 1, 2);
    mesh.add_triangle(1, 3, 2);

    AtlasEngine engine;
    engine.add_mesh(mesh);
    engine.compute_charts();

    PackOptions opts1;
    opts1.resolution = 256;
    engine.pack_charts(opts1);
    auto out1 = engine.extract_mesh(0, policy, "uv1", "", "");

    PackOptions opts2;
    opts2.resolution = 512;
    engine.pack_charts(opts2);
    auto out2 = engine.extract_mesh(0, policy, "uv2", "", "");

    REQUIRE(out1.has_attribute("uv1"));
    REQUIRE(out2.has_attribute("uv2"));
    REQUIRE(out1.get_num_vertices() == out2.get_num_vertices());
    REQUIRE(out1.get_num_facets() == out2.get_num_facets());
}

TEST_CASE("clear() allows reuse", "[xatlas][engine]")
{
    const ChartOptions chart_opts;
    const PackOptions pack_opts;

    SurfaceMesh mesh;
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({0, 1, 0});
    mesh.add_triangle(0, 1, 2);

    AtlasEngine engine;
    engine.add_mesh(mesh);
    engine.generate(chart_opts, pack_opts);
    REQUIRE(engine.mesh_count() == 1);

    engine.clear();
    REQUIRE(engine.mesh_count() == 0);
    REQUIRE(engine.atlas_count() == 0);

    engine.add_mesh(mesh);
    engine.generate(chart_opts, pack_opts);
    REQUIRE(engine.mesh_count() == 1);
}

TEST_CASE("cannot mix add modes", "[xatlas][engine]")
{
    SurfaceMesh mesh;
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({0, 1, 0});
    mesh.add_triangle(0, 1, 2);

    std::vector<Scalar> uv_values = {0, 0, 1, 0, 0, 1};
    std::vector<Index> uv_indices = {0, 1, 2};
    mesh.template create_attribute<Scalar>(
        "uv",
        lagrange::AttributeElement::Indexed,
        lagrange::AttributeUsage::UV,
        2,
        {uv_values.data(), uv_values.size()},
        {uv_indices.data(), uv_indices.size()});

    AtlasEngine engine;
    engine.add_mesh(mesh);

    RepackOptions opts;
    opts.input_uv_attribute_name = "uv";
    REQUIRE_THROWS_AS(engine.add_uv_mesh(mesh, opts), Error);
}

TEST_CASE("pack_charts before compute_charts throws on unwrap path", "[xatlas][engine]")
{
    const PackOptions pack_opts;

    SurfaceMesh mesh;
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({0, 1, 0});
    mesh.add_triangle(0, 1, 2);

    AtlasEngine engine;
    engine.add_mesh(mesh);
    REQUIRE_THROWS_AS(engine.pack_charts(pack_opts), Error);
}

TEST_CASE("extract before pack throws", "[xatlas][engine]")
{
    const MultiAtlasPolicy policy = MultiAtlasPolicy::NormalizePerTile;

    SurfaceMesh mesh;
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({0, 1, 0});
    mesh.add_triangle(0, 1, 2);

    AtlasEngine engine;
    engine.add_mesh(mesh);
    REQUIRE_THROWS_AS(engine.extract_mesh(0, policy, "uv", "", ""), Error);
}

TEST_CASE("notification exceptions are rethrown and rolled back", "[xatlas][engine][progress]")
{
    SurfaceMesh mesh;
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({0, 1, 0});
    mesh.add_triangle(0, 1, 2);

    AtlasEngine engine;
    engine.set_notification_func(
        [](const std::string&, float) { throw std::runtime_error("progress failed"); });

    REQUIRE_THROWS_AS(engine.add_mesh(mesh), std::runtime_error);
    REQUIRE(engine.mesh_count() == 0);
}

TEST_CASE("cancelled add_mesh is rolled back", "[xatlas][engine][progress]")
{
    SurfaceMesh mesh;
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({0, 1, 0});
    mesh.add_triangle(0, 1, 2);

    std::atomic_bool cancel{true};

    AtlasEngine engine;
    engine.set_cancel(&cancel);

    REQUIRE_THROWS_AS(engine.add_mesh(mesh), Error);
    REQUIRE(engine.mesh_count() == 0);
}
