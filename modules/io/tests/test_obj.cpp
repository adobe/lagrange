/*
 * Copyright 2024 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/io/load_mesh_obj.h>
#include <lagrange/io/save_mesh_obj.h>
#include <lagrange/testing/check_mesh.h>
#include <lagrange/testing/common.h>
#include <lagrange/testing/create_test_mesh.h>
#include <lagrange/testing/equivalence_check.h>

#include <catch2/catch_approx.hpp>

TEST_CASE("Grenade_H", "[mesh][io]" LA_CORP_FLAG)
{
    using namespace lagrange;
    auto mesh = io::load_mesh_obj<SurfaceMesh32d>(testing::get_data_path("corp/io/Grenade_H.obj"));
}

TEST_CASE("io/obj", "[io][obj]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    auto mesh = testing::create_test_sphere<Scalar, Index>();
    std::stringstream data;
    io::SaveOptions save_options;
    save_options.output_attributes = io::SaveOptions::OutputAttributes::All;
    save_options.attribute_conversion_policy =
        io::SaveOptions::AttributeConversionPolicy::ConvertAsNeeded;

    save_options.encoding = io::FileEncoding::Ascii;
    REQUIRE_NOTHROW(io::save_mesh_obj(data, mesh, save_options));
    auto mesh2 = io::load_mesh_obj<SurfaceMesh<Scalar, Index>>(data);
    testing::check_mesh(mesh2);
    testing::ensure_approx_equivalent_mesh(mesh, mesh2);
}

TEST_CASE("io/obj empty", "[io][obj]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    SurfaceMesh<Scalar, Index> mesh;
    std::stringstream data;
    io::SaveOptions save_options;
    save_options.output_attributes = io::SaveOptions::OutputAttributes::All;
    save_options.attribute_conversion_policy =
        io::SaveOptions::AttributeConversionPolicy::ConvertAsNeeded;

    save_options.encoding = io::FileEncoding::Ascii;
    REQUIRE_NOTHROW(io::save_mesh_obj(data, mesh, save_options));
    auto mesh2 = io::load_mesh_obj<SurfaceMesh<Scalar, Index>>(data);
    testing::check_mesh(mesh2);
    testing::ensure_approx_equivalent_mesh(mesh, mesh2);
}
