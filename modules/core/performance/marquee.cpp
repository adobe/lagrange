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
#include <CLI/CLI.hpp>
#include <iostream>

#include <lagrange/Logger.h>
#include <lagrange/io/load_mesh.h>
#include <lagrange/io/save_mesh.h>
#include <lagrange/select_facets_in_frustum.h>
#include <lagrange/utils/timing.h>

#ifdef UI_ENABLED
#include <lagrange/ui/Frustum.h>
#include <lagrange/ui/ProxyMesh.h>
#endif

int main(int argc, char** argv)
{
    CLI::App app{argv[0]};
    lagrange::fs::path input_file;
    int N = 10;
    bool greedy = false;
    app.add_flag("--greedy", greedy, "Stop when a facet is selected.");
    app.add_option("-N", N, "Number of marquee queries.")->default_val(10);
    app.add_option("input", input_file, "Input mesh file.")->required()->check(CLI::ExistingFile);
    CLI11_PARSE(app, argc, argv);

    lagrange::logger().info("input filename: {}", input_file);
    lagrange::logger().info("greedy: {}", greedy);
    lagrange::logger().info("N: {}", N);

    using MeshType = lagrange::TriangleMesh3D;
    using VertexType = typename MeshType::VertexType;
    using Scalar = typename MeshType::Scalar;

    auto mesh = lagrange::io::load_mesh<MeshType>(input_file);
    lagrange::logger().info("# vertices: {}", mesh->get_num_vertices());
    lagrange::logger().info("# facets: {}", mesh->get_num_facets());

    const auto& vertices = mesh->get_vertices();
    const VertexType bbox_min = vertices.colwise().minCoeff();
    const VertexType bbox_max = vertices.colwise().maxCoeff();
    const VertexType bbox_center = 0.5 * (bbox_min + bbox_max);
    const Scalar diagonal_len = (bbox_max - bbox_min).norm();
    const Scalar step_x = (bbox_max[0] - bbox_min[0]) / (N - 1);
    const Scalar step_y = (bbox_max[1] - bbox_min[1]) / (N - 1);

    const VertexType camera_pos{bbox_center[0], bbox_center[1], bbox_center[2] + diagonal_len};
    VertexType p0, p1, p2, p3, n01, n12, n23, n30;
    Eigen::Matrix<Scalar, Eigen::Dynamic, 1> times(N);
    times.setZero();

#ifdef UI_ENABLED
    using FVertex = lagrange::ui::FrustumVertices;

    auto construct_plane = [](const VertexType& p, const VertexType& n) {
        return lagrange::ui::Frustum::Plane(n.cast<float>(), p.cast<float>());
    };
    auto set_frustum_vertex = [](lagrange::ui::Frustum& f, FVertex which, const VertexType& p) {
        auto pf = p.cast<float>();
        f.vertices[which] = {pf[0], pf[1], pf[2]};
    };

    auto construct_frustum = [&]() -> lagrange::ui::Frustum {
        // p3     p2
        //  +----+
        //  |    |
        //  +----+
        // p0     p1
        lagrange::ui::Frustum frustum;

        set_frustum_vertex(frustum, FVertex::FRUSTUM_NEAR_LEFT_BOTTOM, p0);
        set_frustum_vertex(
            frustum,
            FVertex::FRUSTUM_FAR_LEFT_BOTTOM,
            p0 + (p0 - camera_pos).normalized() * diagonal_len);

        set_frustum_vertex(frustum, FVertex::FRUSTUM_NEAR_RIGHT_BOTTOM, p1);
        set_frustum_vertex(
            frustum,
            FVertex::FRUSTUM_FAR_RIGHT_BOTTOM,
            p1 + (p1 - camera_pos).normalized() * diagonal_len);

        set_frustum_vertex(frustum, FVertex::FRUSTUM_NEAR_LEFT_TOP, p3);
        set_frustum_vertex(
            frustum,
            FVertex::FRUSTUM_FAR_LEFT_TOP,
            p3 + (p3 - camera_pos).normalized() * diagonal_len);

        set_frustum_vertex(frustum, FVertex::FRUSTUM_NEAR_RIGHT_TOP, p2);
        set_frustum_vertex(
            frustum,
            FVertex::FRUSTUM_FAR_RIGHT_TOP,
            p2 + (p2 - camera_pos).normalized() * diagonal_len);


        frustum.planes[0] = construct_plane(camera_pos, {0, 0, -1}); // Near plane
        frustum.planes[1] =
            construct_plane(camera_pos - VertexType(0, 0, 100 * diagonal_len), {0, 0, 1}); // Far
        frustum.planes[2] = construct_plane(p3, n30); // Left
        frustum.planes[3] = construct_plane(p1, n12); // Right
        frustum.planes[4] = construct_plane(p2, n23); // Top
        frustum.planes[5] = construct_plane(p0, n01); // Bottom
        return frustum;
    };
    auto start_time_proxy = lagrange::get_timestamp();
    auto proxy_mesh = std::make_unique<lagrange::ui::ProxyMesh>(*mesh);
    auto end_time_proxy = lagrange::get_timestamp();
    lagrange::logger().info(
        "Proxy mesh construction: {}s",
        lagrange::timestamp_diff_in_seconds(start_time_proxy, end_time_proxy));
    Eigen::Matrix<Scalar, Eigen::Dynamic, 1> times_2(N);
    times_2.setZero();
#endif

    for (int i = 0; i < N; i++) {
        const Scalar x = step_x * i + bbox_min[0];
        const Scalar y = step_y * i + bbox_min[1];
        p0 = {x - 0.5 * step_x, y - 0.5 * step_y, bbox_max[2]};
        p1 = {x + 0.5 * step_x, y - 0.5 * step_y, bbox_max[2]};
        p2 = {x + 0.5 * step_x, y + 0.5 * step_y, bbox_max[2]};
        p3 = {x - 0.5 * step_x, y + 0.5 * step_y, bbox_max[2]};
        n01 = -(p0 - camera_pos).cross(p1 - camera_pos);
        n12 = -(p1 - camera_pos).cross(p2 - camera_pos);
        n23 = -(p2 - camera_pos).cross(p3 - camera_pos);
        n30 = -(p3 - camera_pos).cross(p0 - camera_pos);

        auto start_time = lagrange::get_timestamp();
        bool r =
            lagrange::select_facets_in_frustum(*mesh, n01, p0, n12, p1, n23, p2, n30, p3, greedy);
        auto end_time = lagrange::get_timestamp();
        times[i] = lagrange::timestamp_diff_in_seconds(start_time, end_time);
        lagrange::logger().info("select_facets run {}: {}s  selected: {}", i, times[i], r);

#ifdef UI_ENABLED
        auto frustum = construct_frustum();
        start_time = lagrange::get_timestamp();
        bool r2 = proxy_mesh->intersects(frustum);
        end_time = lagrange::get_timestamp();
        times_2[i] = lagrange::timestamp_diff_in_seconds(start_time, end_time);
        lagrange::logger().info("UI select run {}: {}s  selected: {}", i, times_2[i], r2);

        if (r2 != r) {
            lagrange::logger().error("Ops, select_facet and UI select disagrees!");
            using VertexArray = typename MeshType::VertexArray;
            using FacetArray = typename MeshType::FacetArray;

            VertexArray frustum_vertices(5, 3);
            frustum_vertices.row(0) = camera_pos;
            frustum_vertices.row(1) = camera_pos + (p0 - camera_pos) * 10;
            frustum_vertices.row(2) = camera_pos + (p1 - camera_pos) * 10;
            frustum_vertices.row(3) = camera_pos + (p2 - camera_pos) * 10;
            frustum_vertices.row(4) = camera_pos + (p3 - camera_pos) * 10;
            FacetArray frustum_facets(4, 3);
            frustum_facets << 0, 1, 2, 0, 2, 3, 0, 3, 4, 0, 4, 1;
            auto frustum2 = lagrange::wrap_with_mesh(frustum_vertices, frustum_facets);
            auto filename = fmt::format("frustum_{}.obj", i);
            lagrange::io::save_mesh(filename, *frustum2);
            lagrange::logger().error("Saving frustum as {} for further debugging", filename);
        }
#endif
    }

    lagrange::logger().info("select_facet average time over {} runs: {}", N, times.mean());
#ifdef UI_ENABLED
    lagrange::logger().info("UI select average time over {} runs: {}", N, times_2.mean());
#endif
    return 0;
}
