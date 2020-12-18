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
#include <cmath>
#include <iostream>

#include <lagrange/ExactPredicates.h>
#include <lagrange/attributes/eval_as_attribute.h>
#include <lagrange/common.h>
#include <lagrange/create_mesh.h>
#include <lagrange/utils/timing.h>

template <typename MeshType>
void test_using_eigen(MeshType& mesh)
{
    using AttributeArray = typename MeshType::AttributeArray;
    constexpr size_t num_itrs = 100;
    lagrange::timestamp_type start, finish;
    lagrange::get_timestamp(&start);

    const auto& vertices = mesh.get_vertices();
    mesh.add_vertex_attribute("is_selected");

    for (size_t i = 0; i < num_itrs; i++) {
        AttributeArray attr(vertices.rows(), 1);
        Eigen::Vector3d n(0.1 * i, -0.01 * i, 1 - 0.01 * i);
        attr = vertices * n;
        mesh.import_vertex_attribute("is_selected", attr);
    }

    lagrange::get_timestamp(&finish);
    double duration = lagrange::timestamp_diff_in_seconds(start, finish);

    std::cout << "select_vertices using eigen (Baseline):" << std::endl;
    std::cout << "Total running time: " << duration << " secs" << std::endl;
    std::cout << "Average: " << duration / num_itrs * 1e3 << " ms per call" << std::endl;
}

template <typename MeshType>
void test_using_lambda(MeshType& mesh)
{
    using Scalar = typename MeshType::Scalar;
    constexpr size_t num_itrs = 100;
    lagrange::timestamp_type start, finish;
    lagrange::get_timestamp(&start);
    for (size_t i = 0; i < num_itrs; i++) {
        auto in_out = [i](Scalar x, Scalar y, Scalar z) {
            return 0.1 * i * x - 0.01 * i * y - 0.01 * i * z;
        };
        lagrange::eval_as_vertex_attribute(mesh, "is_selected", in_out);
    }
    lagrange::get_timestamp(&finish);
    double duration = lagrange::timestamp_diff_in_seconds(start, finish);

    std::cout << "select_vertices using lambda:" << std::endl;
    std::cout << "Total running time: " << duration << " secs" << std::endl;
    std::cout << "Average: " << duration / num_itrs * 1e3 << " ms per call" << std::endl;
}

template <typename MeshType>
void test_using_exact_predicates(MeshType& mesh)
{
    using Scalar = typename MeshType::Scalar;
    constexpr size_t num_itrs = 100;

    auto predicates = lagrange::ExactPredicates::create("shewchuk");
    lagrange::timestamp_type start, finish;
    lagrange::get_timestamp(&start);
    for (size_t i = 0; i < num_itrs; i++) {
        auto in_out = [i, &predicates](Scalar x, Scalar y, Scalar z) {
            Scalar p0[3]{0.0, 0.0, 0.0};
            Scalar p1[3]{1.0, 0.0, -0.1 * i};
            Scalar p2[3]{1.0, 1.0, -0.1 * i + 0.01 * i};
            Scalar p[3]{x, y, z};
            return predicates->orient3D(p0, p1, p2, p);
        };
        lagrange::eval_as_vertex_attribute(mesh, "is_selected", in_out);
    }
    lagrange::get_timestamp(&finish);
    double duration = lagrange::timestamp_diff_in_seconds(start, finish);

    std::cout << "select_vertices using exact predicates:" << std::endl;
    std::cout << "Total running time: " << duration << " secs" << std::endl;
    std::cout << "Average: " << duration / num_itrs * 1e3 << " ms per call" << std::endl;
}

int main()
{
    constexpr size_t N = lagrange::safe_cast<size_t>(1e6);
    lagrange::Vertices3D vertices = lagrange::Vertices3D::Random(N, 3);
    lagrange::Triangles facets;
    auto mesh = lagrange::wrap_with_mesh(vertices, facets);

    test_using_eigen(*mesh);
    test_using_lambda(*mesh);
    test_using_exact_predicates(*mesh);

    return 0;
}
