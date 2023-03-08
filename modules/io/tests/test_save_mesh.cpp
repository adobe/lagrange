/*
 * Copyright 2019 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/io/save_mesh.h>
#include <lagrange/testing/common.h>
#include <lagrange/testing/create_test_mesh.h>
#include <lagrange/testing/equivalence_check.h>

#include <lagrange/Attribute.h>
#include <lagrange/compute_area.h>
#include <lagrange/compute_normal.h>
#include <lagrange/internal/find_attribute_utils.h>
#include <lagrange/map_attribute.h>
#include <lagrange/unify_index_buffer.h>

using namespace lagrange;

void ensure_attributes_exist(const SurfaceMesh32d& mesh, bool texcoord, bool normal)
{
    bool found_texcoord = false;
    bool found_normal = false;
    mesh.seq_foreach_attribute_id([&](AttributeId id) -> void {
        auto& attr_base = mesh.get_attribute_base(id);
        if (attr_base.get_usage() == AttributeUsage::UV) found_texcoord = true;
        if (attr_base.get_usage() == AttributeUsage::Normal) found_normal = true;
    });
    REQUIRE(found_texcoord == texcoord);
    REQUIRE(found_normal == normal);
}

TEST_CASE("save_mesh_attributes_all", "[io]")
{
    auto cube_indexed = testing::create_test_cube<double, uint32_t>();
    using Scalar = decltype(cube_indexed)::Scalar;
    using Index = decltype(cube_indexed)::Index;
    lagrange::compute_normal<Scalar, Index>(cube_indexed, [](Index) -> bool { return false; });
    auto cube = unify_index_buffer(cube_indexed);

    // make sure we have uvs and normals
    ensure_attributes_exist(cube, true, true);

    lagrange::fs::path filename("save_mesh_attributes_all");

    io::SaveOptions opt;

    SECTION("gltf")
    {
        filename.replace_extension(".gltf");
        opt.encoding = io::FileEncoding::Ascii;
        REQUIRE_NOTHROW(io::save_mesh(filename, cube, opt));
        auto loaded = io::load_mesh<SurfaceMesh32d>(filename);
        ensure_attributes_exist(loaded, true, true);
        testing::ensure_approx_equivalent_usage<AttributeUsage::UV>(cube, loaded);
        testing::ensure_approx_equivalent_usage<AttributeUsage::Normal>(cube, loaded);
    }

    SECTION("glb")
    {
        filename.replace_extension(".glb");
        opt.encoding = io::FileEncoding::Binary;
        REQUIRE_NOTHROW(io::save_mesh(filename, cube, opt));
        auto loaded = io::load_mesh<SurfaceMesh32d>(filename);
        ensure_attributes_exist(loaded, true, true);
        testing::ensure_approx_equivalent_usage<AttributeUsage::UV>(cube, loaded);
        testing::ensure_approx_equivalent_usage<AttributeUsage::Normal>(cube, loaded);
    }

    SECTION("msh ascii")
    {
        filename.replace_extension(".msh");
        opt.encoding = io::FileEncoding::Ascii;
        REQUIRE_NOTHROW(io::save_mesh(filename, cube, opt));
        auto loaded = io::load_mesh<SurfaceMesh32d>(filename);
        ensure_attributes_exist(loaded, true, true);
        testing::ensure_approx_equivalent_usage<AttributeUsage::UV>(cube, loaded);
        testing::ensure_approx_equivalent_usage<AttributeUsage::Normal>(cube, loaded);
    }

    SECTION("msh binary")
    {
        filename.replace_extension(".msh");
        opt.encoding = io::FileEncoding::Binary;
        REQUIRE_NOTHROW(io::save_mesh(filename, cube, opt));
        auto loaded = io::load_mesh<SurfaceMesh32d>(filename);
        ensure_attributes_exist(loaded, true, true);
        testing::ensure_approx_equivalent_usage<AttributeUsage::UV>(cube, loaded);
        testing::ensure_approx_equivalent_usage<AttributeUsage::Normal>(cube, loaded);
    }

    SECTION("obj")
    {
        filename.replace_extension(".obj");
        opt.encoding = io::FileEncoding::Ascii;
        REQUIRE_NOTHROW(io::save_mesh(filename, cube, opt));
        auto loaded = io::load_mesh<SurfaceMesh32d>(filename);
        ensure_attributes_exist(loaded, true, true);
        testing::ensure_approx_equivalent_usage<AttributeUsage::UV>(cube, loaded);
        testing::ensure_approx_equivalent_usage<AttributeUsage::Normal>(cube, loaded);
    }

    SECTION("ply ascii")
    {
        filename.replace_extension(".ply");
        opt.encoding = io::FileEncoding::Ascii;
        REQUIRE_NOTHROW(io::save_mesh(filename, cube, opt));
        auto loaded = io::load_mesh<SurfaceMesh32d>(filename);
        ensure_attributes_exist(loaded, true, true);
        testing::ensure_approx_equivalent_usage<AttributeUsage::UV>(cube, loaded);
        testing::ensure_approx_equivalent_usage<AttributeUsage::Normal>(cube, loaded);
    }

    SECTION("ply binary")
    {
        filename.replace_extension(".ply");
        opt.encoding = io::FileEncoding::Binary;
        REQUIRE_NOTHROW(io::save_mesh(filename, cube, opt));
        auto loaded = io::load_mesh<SurfaceMesh32d>(filename);
        ensure_attributes_exist(loaded, true, true);
        testing::ensure_approx_equivalent_usage<AttributeUsage::UV>(cube, loaded);
        testing::ensure_approx_equivalent_usage<AttributeUsage::Normal>(cube, loaded);
    }
}

TEST_CASE("save_mesh_attributes_selected", "[io]")
{
    auto cube_indexed = testing::create_test_cube<double, uint32_t>();
    using Scalar = decltype(cube_indexed)::Scalar;
    using Index = decltype(cube_indexed)::Index;
    lagrange::compute_normal<Scalar, Index>(cube_indexed, [](Index) -> bool { return false; });
    auto cube = unify_index_buffer(cube_indexed);

    AttributeId normal_id = invalid_attribute_id();
    cube.seq_foreach_attribute_id([&](AttributeId id) -> void {
        auto& attr_base = cube.get_attribute_base(id);
        if (attr_base.get_usage() == AttributeUsage::Normal) normal_id = id;
    });
    REQUIRE(normal_id != invalid_attribute_id());

    io::SaveOptions opt;
    opt.output_attributes = io::SaveOptions::OutputAttributes::SelectedOnly;
    opt.selected_attributes = {normal_id};
    lagrange::fs::path filename("save_mesh_attributes_selected");

    SECTION("gltf")
    {
        filename.replace_extension(".gltf");
        opt.encoding = io::FileEncoding::Ascii;
        REQUIRE_NOTHROW(io::save_mesh(filename, cube, opt));
        auto loaded = io::load_mesh<SurfaceMesh32d>(filename);
        ensure_attributes_exist(loaded, false, true);
        testing::ensure_approx_equivalent_usage<AttributeUsage::Normal>(cube, loaded);
    }

    SECTION("glb")
    {
        filename.replace_extension(".glb");
        opt.encoding = io::FileEncoding::Binary;
        REQUIRE_NOTHROW(io::save_mesh(filename, cube, opt));
        auto loaded = io::load_mesh<SurfaceMesh32d>(filename);
        ensure_attributes_exist(loaded, false, true);
        testing::ensure_approx_equivalent_usage<AttributeUsage::Normal>(cube, loaded);
    }

    SECTION("msh ascii")
    {
        filename.replace_extension(".msh");
        opt.encoding = io::FileEncoding::Ascii;
        REQUIRE_NOTHROW(io::save_mesh(filename, cube, opt));
        auto loaded = io::load_mesh<SurfaceMesh32d>(filename);
        ensure_attributes_exist(loaded, false, true);
        testing::ensure_approx_equivalent_usage<AttributeUsage::Normal>(cube, loaded);
    }

    SECTION("msh binary")
    {
        filename.replace_extension(".msh");
        opt.encoding = io::FileEncoding::Binary;
        REQUIRE_NOTHROW(io::save_mesh(filename, cube, opt));
        auto loaded = io::load_mesh<SurfaceMesh32d>(filename);
        ensure_attributes_exist(loaded, false, true);
        testing::ensure_approx_equivalent_usage<AttributeUsage::Normal>(cube, loaded);
    }

    SECTION("obj")
    {
        filename.replace_extension(".obj");
        opt.encoding = io::FileEncoding::Ascii;
        REQUIRE_NOTHROW(io::save_mesh(filename, cube, opt));
        auto loaded = io::load_mesh<SurfaceMesh32d>(filename);
        ensure_attributes_exist(loaded, false, true);
        testing::ensure_approx_equivalent_usage<AttributeUsage::Normal>(cube, loaded);
    }

    SECTION("ply ascii")
    {
        filename.replace_extension(".ply");
        opt.encoding = io::FileEncoding::Ascii;
        REQUIRE_NOTHROW(io::save_mesh(filename, cube, opt));
        auto loaded = io::load_mesh<SurfaceMesh32d>(filename);
        ensure_attributes_exist(loaded, false, true); // same as above
        testing::ensure_approx_equivalent_usage<AttributeUsage::Normal>(cube, loaded);
    }

    SECTION("ply binary")
    {
        filename.replace_extension(".ply");
        opt.encoding = io::FileEncoding::Binary;
        REQUIRE_NOTHROW(io::save_mesh(filename, cube, opt));
        auto loaded = io::load_mesh<SurfaceMesh32d>(filename);
        ensure_attributes_exist(loaded, false, true); // same as above
        testing::ensure_approx_equivalent_usage<AttributeUsage::Normal>(cube, loaded);
    }
}

TEST_CASE("save_mesh_indexed_attributes", "[io]")
{
    auto cube = testing::create_test_cube<double, uint32_t>();
    using Scalar = decltype(cube)::Scalar;
    lagrange::compute_facet_area(cube);
    auto normal_id = lagrange::compute_normal(cube, static_cast<Scalar>(M_PI / 4));

    io::SaveOptions opt;
    opt.output_attributes = io::SaveOptions::OutputAttributes::SelectedOnly;
    opt.attribute_conversion_policy = io::SaveOptions::AttributeConversionPolicy::ConvertAsNeeded;
    opt.selected_attributes = {normal_id};
    lagrange::fs::path filename("save_mesh_indexed_attributes");

    SECTION("gltf")
    {
        filename.replace_extension(".gltf");
        opt.encoding = io::FileEncoding::Ascii;
        REQUIRE_NOTHROW(io::save_mesh(filename, cube, opt));
        auto loaded = io::load_mesh<SurfaceMesh32d>(filename);
        ensure_attributes_exist(loaded, false, true);
        testing::ensure_approx_equivalent_usage<AttributeUsage::Normal>(cube, loaded);
    }

    SECTION("glb")
    {
        filename.replace_extension(".glb");
        opt.encoding = io::FileEncoding::Binary;
        REQUIRE_NOTHROW(io::save_mesh(filename, cube, opt));
        auto loaded = io::load_mesh<SurfaceMesh32d>(filename);
        ensure_attributes_exist(loaded, false, true);
        testing::ensure_approx_equivalent_usage<AttributeUsage::Normal>(cube, loaded);
    }

    SECTION("msh ascii")
    {
        filename.replace_extension(".msh");
        opt.encoding = io::FileEncoding::Ascii;
        REQUIRE_NOTHROW(io::save_mesh(filename, cube, opt));
        auto loaded = io::load_mesh<SurfaceMesh32d>(filename);
        ensure_attributes_exist(loaded, false, true);
        testing::ensure_approx_equivalent_usage<AttributeUsage::Normal>(cube, loaded);
    }

    SECTION("msh binary")
    {
        filename.replace_extension(".msh");
        opt.encoding = io::FileEncoding::Binary;
        REQUIRE_NOTHROW(io::save_mesh(filename, cube, opt));
        auto loaded = io::load_mesh<SurfaceMesh32d>(filename);
        ensure_attributes_exist(loaded, false, true);
        testing::ensure_approx_equivalent_usage<AttributeUsage::Normal>(cube, loaded);
    }

    SECTION("obj")
    {
        filename.replace_extension(".obj");
        opt.encoding = io::FileEncoding::Ascii;
        REQUIRE_NOTHROW(io::save_mesh(filename, cube, opt));
        auto loaded = io::load_mesh<SurfaceMesh32d>(filename);
        ensure_attributes_exist(loaded, false, true);
        testing::ensure_approx_equivalent_usage<AttributeUsage::Normal>(cube, loaded);
    }

    SECTION("ply ascii")
    {
        filename.replace_extension(".ply");
        opt.encoding = io::FileEncoding::Ascii;
        REQUIRE_NOTHROW(io::save_mesh(filename, cube, opt));
        auto loaded = io::load_mesh<SurfaceMesh32d>(filename);
        ensure_attributes_exist(loaded, false, true);
        testing::ensure_approx_equivalent_usage<AttributeUsage::Normal>(cube, loaded);
    }

    SECTION("ply binary")
    {
        filename.replace_extension(".ply");
        opt.encoding = io::FileEncoding::Binary;
        REQUIRE_NOTHROW(io::save_mesh(filename, cube, opt));
        auto loaded = io::load_mesh<SurfaceMesh32d>(filename);
        ensure_attributes_exist(loaded, false, true);
        testing::ensure_approx_equivalent_usage<AttributeUsage::Normal>(cube, loaded);
    }
}
