/*
 * Copyright 2025 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/IndexedAttribute.h>
#include <lagrange/common.h>
#include <lagrange/primitive/generate_disc.h>
#include <lagrange/views.h>

#include "primitive_test_utils.h"

#include <catch2/matchers/catch_matchers_floating_point.hpp>

TEST_CASE("generate_disc", "[primitive][surface]")
{
    using namespace lagrange;
    using Scalar = float;
    using Index = uint32_t;

    primitive::DiscOptions setting;

    SECTION("Simple disc")
    {
        auto mesh = primitive::generate_disc<Scalar, Index>(setting);
        primitive_test_utils::validate_primitive(mesh, 1);
        primitive_test_utils::check_degeneracy(mesh);
        primitive_test_utils::check_UV(mesh);
    }

    SECTION("Partial disc")
    {
        setting.start_angle = static_cast<Scalar>(lagrange::internal::pi / 4);
        setting.end_angle = static_cast<Scalar>(3 * lagrange::internal::pi / 4);
        setting.radial_sections = 10;
        setting.num_rings = 5;

        auto mesh = primitive::generate_disc<Scalar, Index>(setting);
        primitive_test_utils::validate_primitive(mesh, 1);
        primitive_test_utils::check_degeneracy(mesh);
        primitive_test_utils::check_UV(mesh);
    }

    SECTION("Zero radius")
    {
        setting.radius = 0.0f;

        auto mesh = primitive::generate_disc<Scalar, Index>(setting);
        primitive_test_utils::validate_primitive(mesh, 1);
    }

    SECTION("Different normal and center")
    {
        setting.normal = {0, 1, 0};
        setting.center = {0, 1, 0};

        auto mesh = primitive::generate_disc<Scalar, Index>(setting);
        primitive_test_utils::validate_primitive(mesh, 1);
        primitive_test_utils::check_degeneracy(mesh);
        primitive_test_utils::check_UV(mesh);

        // Check center
        auto vertices = vertex_view(mesh);
        auto bbox_min = vertices.colwise().minCoeff();
        auto bbox_max = vertices.colwise().maxCoeff();
        Eigen::Vector3<Scalar> center = (bbox_min + bbox_max) / 2.0f;
        REQUIRE_THAT(center[0], Catch::Matchers::WithinAbs(0.0f, 1e-6f));
        REQUIRE_THAT(center[1], Catch::Matchers::WithinAbs(1.0f, 1e-6f));
        REQUIRE_THAT(center[2], Catch::Matchers::WithinAbs(0.0f, 1e-6f));

        // Check normal direction
        REQUIRE(mesh.has_attribute(setting.normal_attribute_name));
        REQUIRE(mesh.is_attribute_indexed(setting.normal_attribute_name));
        auto& normal_attr = mesh.get_indexed_attribute<Scalar>(setting.normal_attribute_name);
        auto& normal_values = normal_attr.values();
        auto normals = matrix_view(normal_values);
        for (Index i = 0; i < normals.rows(); i++) {
            REQUIRE_THAT(normals(i, 0), Catch::Matchers::WithinAbs(0.0f, 1e-6f));
            REQUIRE_THAT(normals(i, 1), Catch::Matchers::WithinAbs(1.0f, 1e-6f));
            REQUIRE_THAT(normals(i, 2), Catch::Matchers::WithinAbs(0.0f, 1e-6f));
        }
    }
}
