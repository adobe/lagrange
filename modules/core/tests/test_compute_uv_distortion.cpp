/*
 * Copyright 2023 Adobe. All rights reserved.
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
#include <lagrange/IndexedAttribute.h>
#include <lagrange/compute_uv_distortion.h>
#include <lagrange/testing/common.h>

#include <catch2/matchers/catch_matchers_floating_point.hpp>

TEST_CASE("compute_uv_distortion", "[core][uv]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;
    constexpr Scalar tol = static_cast<Scalar>(1e-6);

    SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({0, 1, 0});
    mesh.add_vertex({1, 1, 0});
    mesh.add_triangle(0, 1, 2);
    mesh.add_triangle(2, 1, 3);

    UVDistortionOptions opt;
    opt.uv_attribute_name = "uv";

    auto run_and_check = [&](DistortionMetric metric, Scalar s0, Scalar s1) {
        opt.metric = metric;
        auto id = compute_uv_distortion(mesh, opt);
        auto& attr = mesh.get_attribute<Scalar>(id);
        REQUIRE(attr.get_usage() == AttributeUsage::Scalar);
        REQUIRE(attr.get_element_type() == AttributeElement::Facet);

        switch (metric) {
        case DistortionMetric::AreaRatio:
            REQUIRE_THAT(attr.get(0), Catch::Matchers::WithinAbs(s0 * s1, tol));
            REQUIRE_THAT(attr.get(1), Catch::Matchers::WithinAbs(s0 * s1, tol));
            break;
        case DistortionMetric::Dirichlet:
            REQUIRE_THAT(attr.get(0), Catch::Matchers::WithinAbs(s0 * s0 + s1 * s1, tol));
            REQUIRE_THAT(attr.get(1), Catch::Matchers::WithinAbs(s0 * s0 + s1 * s1, tol));
            break;
        case DistortionMetric::InverseDirichlet:
            REQUIRE_THAT(
                attr.get(0),
                Catch::Matchers::WithinAbs(1 / (s0 * s0) + 1 / (s1 * s1), tol));
            REQUIRE_THAT(
                attr.get(1),
                Catch::Matchers::WithinAbs(1 / (s0 * s0) + 1 / (s1 * s1), tol));
            break;
        case DistortionMetric::SymmetricDirichlet:
            REQUIRE_THAT(
                attr.get(0),
                Catch::Matchers::WithinAbs(s0 * s0 + s1 * s1 + 1 / (s0 * s0) + 1 / (s1 * s1), tol));
            REQUIRE_THAT(
                attr.get(1),
                Catch::Matchers::WithinAbs(s0 * s0 + s1 * s1 + 1 / (s0 * s0) + 1 / (s1 * s1), tol));
            break;
        case DistortionMetric::MIPS:
            REQUIRE_THAT(attr.get(0), Catch::Matchers::WithinAbs(s0 / s1 + s1 / s0, tol));
            REQUIRE_THAT(attr.get(1), Catch::Matchers::WithinAbs(s0 / s1 + s1 / s0, tol));
            break;
        }
    };

    SECTION("identity")
    {
        std::vector<Scalar> uv_values{0, 0, 1, 0, 0, 1, 1, 1};
        std::vector<Index> uv_indices{0, 1, 2, 2, 1, 3};
        mesh.template create_attribute<Scalar>(
            opt.uv_attribute_name,
            AttributeElement::Indexed,
            AttributeUsage::UV,
            2,
            uv_values,
            uv_indices);

        SECTION("area ratio")
        {
            run_and_check(DistortionMetric::AreaRatio, 1, 1);
        }

        SECTION("Dirichlet")
        {
            run_and_check(DistortionMetric::Dirichlet, 1, 1);
        }

        SECTION("Symmetric Dirichlet")
        {
            run_and_check(DistortionMetric::SymmetricDirichlet, 1, 1);
        }

        SECTION("MIPS")
        {
            run_and_check(DistortionMetric::MIPS, 1, 1);
        }
    }

    SECTION("2x in X")
    {
        std::vector<Scalar> uv_values{0, 0, 2, 0, 0, 1, 2, 1};
        std::vector<Index> uv_indices{0, 1, 2, 2, 1, 3};
        mesh.template create_attribute<Scalar>(
            opt.uv_attribute_name,
            AttributeElement::Indexed,
            AttributeUsage::UV,
            2,
            uv_values,
            uv_indices);

        SECTION("area ratio")
        {
            run_and_check(DistortionMetric::AreaRatio, 2, 1);
        }

        SECTION("Dirichlet")
        {
            run_and_check(DistortionMetric::Dirichlet, 2, 1);
        }

        SECTION("Symmetric Dirichlet")
        {
            run_and_check(DistortionMetric::SymmetricDirichlet, 2, 1);
        }

        SECTION("MIPS")
        {
            run_and_check(DistortionMetric::MIPS, 2, 1);
        }
    }

    SECTION("2x in X, -1 in Y")
    {
        std::vector<Scalar> uv_values{0, 0, 2, 0, 0, -1, 2, -1};
        std::vector<Index> uv_indices{0, 1, 2, 2, 1, 3};
        mesh.template create_attribute<Scalar>(
            opt.uv_attribute_name,
            AttributeElement::Indexed,
            AttributeUsage::UV,
            2,
            uv_values,
            uv_indices);

        SECTION("area ratio")
        {
            run_and_check(DistortionMetric::AreaRatio, 2, -1);
        }

        SECTION("Dirichlet")
        {
            run_and_check(DistortionMetric::Dirichlet, 2, -1);
        }

        SECTION("Symmetric Dirichlet")
        {
            run_and_check(DistortionMetric::SymmetricDirichlet, 2, -1);
        }

        SECTION("MIPS")
        {
            run_and_check(DistortionMetric::MIPS, 2, -1);
        }
    }
}
