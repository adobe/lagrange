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
#include <lagrange/testing/common.h>
#include <lagrange/testing/create_test_mesh.h>
#include <lagrange/testing/equivalence_check.h>

#include <lagrange/Attribute.h>
#include <lagrange/compute_area.h>
#include <lagrange/compute_normal.h>
#include <lagrange/internal/find_attribute_utils.h>
#include <lagrange/io/load_mesh_gltf.h>
#include <lagrange/io/load_mesh_msh.h>
#include <lagrange/io/load_mesh_obj.h>
#include <lagrange/io/load_mesh_ply.h>
#include <lagrange/io/save_mesh.h>
#include <lagrange/io/save_mesh_gltf.h>
#include <lagrange/io/save_mesh_msh.h>
#include <lagrange/io/save_mesh_obj.h>
#include <lagrange/io/save_mesh_ply.h>
#include <lagrange/map_attribute.h>
#include <lagrange/unify_index_buffer.h>

#include <sstream>

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

    io::SaveOptions opt;

    SECTION("gltf")
    {
        opt.encoding = io::FileEncoding::Ascii;
        std::stringstream buffer;
        REQUIRE_NOTHROW(io::save_mesh_gltf(buffer, cube, opt));
        auto loaded = io::load_mesh_gltf<SurfaceMesh32d>(buffer);
        ensure_attributes_exist(loaded, true, true);
        testing::ensure_approx_equivalent_usage<AttributeUsage::UV>(cube, loaded);
        testing::ensure_approx_equivalent_usage<AttributeUsage::Normal>(cube, loaded);
    }

    SECTION("glb")
    {
        opt.encoding = io::FileEncoding::Binary;
        std::stringstream buffer;
        REQUIRE_NOTHROW(io::save_mesh_gltf(buffer, cube, opt));
        auto loaded = io::load_mesh_gltf<SurfaceMesh32d>(buffer);
        ensure_attributes_exist(loaded, true, true);
        testing::ensure_approx_equivalent_usage<AttributeUsage::UV>(cube, loaded);
        testing::ensure_approx_equivalent_usage<AttributeUsage::Normal>(cube, loaded);
    }

    SECTION("msh ascii")
    {
        opt.encoding = io::FileEncoding::Ascii;
        std::stringstream buffer;
        REQUIRE_NOTHROW(io::save_mesh_msh(buffer, cube, opt));
        auto loaded = io::load_mesh_msh<SurfaceMesh32d>(buffer);
        ensure_attributes_exist(loaded, true, true);
        testing::ensure_approx_equivalent_usage<AttributeUsage::UV>(cube, loaded);
        testing::ensure_approx_equivalent_usage<AttributeUsage::Normal>(cube, loaded);
    }

    SECTION("msh binary")
    {
        opt.encoding = io::FileEncoding::Binary;
        std::stringstream buffer;
        REQUIRE_NOTHROW(io::save_mesh_msh(buffer, cube, opt));
        auto loaded = io::load_mesh_msh<SurfaceMesh32d>(buffer);
        ensure_attributes_exist(loaded, true, true);
        testing::ensure_approx_equivalent_usage<AttributeUsage::UV>(cube, loaded);
        testing::ensure_approx_equivalent_usage<AttributeUsage::Normal>(cube, loaded);
    }

    SECTION("obj")
    {
        opt.encoding = io::FileEncoding::Ascii;
        std::stringstream buffer;
        REQUIRE_NOTHROW(io::save_mesh_obj(buffer, cube, opt));
        auto loaded = io::load_mesh_obj<SurfaceMesh32d>(buffer);
        ensure_attributes_exist(loaded, true, true);
        testing::ensure_approx_equivalent_usage<AttributeUsage::UV>(cube, loaded);
        testing::ensure_approx_equivalent_usage<AttributeUsage::Normal>(cube, loaded);
    }

    SECTION("ply ascii")
    {
        opt.encoding = io::FileEncoding::Ascii;
        std::stringstream buffer;
        REQUIRE_NOTHROW(io::save_mesh_ply(buffer, cube, opt));
        auto loaded = io::load_mesh_ply<SurfaceMesh32d>(buffer);
        ensure_attributes_exist(loaded, true, true);
        testing::ensure_approx_equivalent_usage<AttributeUsage::UV>(cube, loaded);
        testing::ensure_approx_equivalent_usage<AttributeUsage::Normal>(cube, loaded);
    }

    SECTION("ply binary")
    {
        opt.encoding = io::FileEncoding::Binary;
        std::stringstream buffer;
        REQUIRE_NOTHROW(io::save_mesh_ply(buffer, cube, opt));
        auto loaded = io::load_mesh_ply<SurfaceMesh32d>(buffer);
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

    SECTION("gltf")
    {
        opt.encoding = io::FileEncoding::Ascii;
        std::stringstream buffer;
        REQUIRE_NOTHROW(io::save_mesh_gltf(buffer, cube, opt));
        auto loaded = io::load_mesh_gltf<SurfaceMesh32d>(buffer);
        ensure_attributes_exist(loaded, false, true);
        testing::ensure_approx_equivalent_usage<AttributeUsage::Normal>(cube, loaded);
    }

    SECTION("glb")
    {
        opt.encoding = io::FileEncoding::Binary;
        std::stringstream buffer;
        REQUIRE_NOTHROW(io::save_mesh_gltf(buffer, cube, opt));
        auto loaded = io::load_mesh_gltf<SurfaceMesh32d>(buffer);
        ensure_attributes_exist(loaded, false, true);
        testing::ensure_approx_equivalent_usage<AttributeUsage::Normal>(cube, loaded);
    }

    SECTION("msh ascii")
    {
        opt.encoding = io::FileEncoding::Ascii;
        std::stringstream buffer;
        REQUIRE_NOTHROW(io::save_mesh_msh(buffer, cube, opt));
        auto loaded = io::load_mesh_msh<SurfaceMesh32d>(buffer);
        ensure_attributes_exist(loaded, false, true);
        testing::ensure_approx_equivalent_usage<AttributeUsage::Normal>(cube, loaded);
    }

    SECTION("msh binary")
    {
        opt.encoding = io::FileEncoding::Binary;
        std::stringstream buffer;
        REQUIRE_NOTHROW(io::save_mesh_msh(buffer, cube, opt));
        auto loaded = io::load_mesh_msh<SurfaceMesh32d>(buffer);
        ensure_attributes_exist(loaded, false, true);
        testing::ensure_approx_equivalent_usage<AttributeUsage::Normal>(cube, loaded);
    }

    SECTION("obj")
    {
        opt.encoding = io::FileEncoding::Ascii;
        std::stringstream buffer;
        REQUIRE_NOTHROW(io::save_mesh_obj(buffer, cube, opt));
        auto loaded = io::load_mesh_obj<SurfaceMesh32d>(buffer);
        ensure_attributes_exist(loaded, false, true);
        testing::ensure_approx_equivalent_usage<AttributeUsage::Normal>(cube, loaded);
    }

    SECTION("ply ascii")
    {
        opt.encoding = io::FileEncoding::Ascii;
        std::stringstream buffer;
        REQUIRE_NOTHROW(io::save_mesh_ply(buffer, cube, opt));
        auto loaded = io::load_mesh_ply<SurfaceMesh32d>(buffer);
        ensure_attributes_exist(loaded, false, true); // same as above
        testing::ensure_approx_equivalent_usage<AttributeUsage::Normal>(cube, loaded);
    }

    SECTION("ply binary")
    {
        opt.encoding = io::FileEncoding::Binary;
        std::stringstream buffer;
        REQUIRE_NOTHROW(io::save_mesh_ply(buffer, cube, opt));
        auto loaded = io::load_mesh_ply<SurfaceMesh32d>(buffer);
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

    SECTION("gltf")
    {
        opt.encoding = io::FileEncoding::Ascii;
        std::stringstream buffer;
        REQUIRE_NOTHROW(io::save_mesh_gltf(buffer, cube, opt));
        auto loaded = io::load_mesh_gltf<SurfaceMesh32d>(buffer);
        ensure_attributes_exist(loaded, false, true);
        testing::ensure_approx_equivalent_usage<AttributeUsage::Normal>(cube, loaded);
    }

    SECTION("glb")
    {
        opt.encoding = io::FileEncoding::Binary;
        std::stringstream buffer;
        REQUIRE_NOTHROW(io::save_mesh_gltf(buffer, cube, opt));
        auto loaded = io::load_mesh_gltf<SurfaceMesh32d>(buffer);
        ensure_attributes_exist(loaded, false, true);
        testing::ensure_approx_equivalent_usage<AttributeUsage::Normal>(cube, loaded);
    }

    SECTION("msh ascii")
    {
        opt.encoding = io::FileEncoding::Ascii;
        std::stringstream buffer;
        REQUIRE_NOTHROW(io::save_mesh_msh(buffer, cube, opt));
        auto loaded = io::load_mesh_msh<SurfaceMesh32d>(buffer);
        ensure_attributes_exist(loaded, false, true);
        testing::ensure_approx_equivalent_usage<AttributeUsage::Normal>(cube, loaded);
    }

    SECTION("msh binary")
    {
        opt.encoding = io::FileEncoding::Binary;
        std::stringstream buffer;
        REQUIRE_NOTHROW(io::save_mesh_msh(buffer, cube, opt));
        auto loaded = io::load_mesh_msh<SurfaceMesh32d>(buffer);
        ensure_attributes_exist(loaded, false, true);
        testing::ensure_approx_equivalent_usage<AttributeUsage::Normal>(cube, loaded);
    }

    SECTION("obj")
    {
        opt.encoding = io::FileEncoding::Ascii;
        std::stringstream buffer;
        REQUIRE_NOTHROW(io::save_mesh_obj(buffer, cube, opt));
        auto loaded = io::load_mesh_obj<SurfaceMesh32d>(buffer);
        ensure_attributes_exist(loaded, false, true);
        testing::ensure_approx_equivalent_usage<AttributeUsage::Normal>(cube, loaded);
    }

    SECTION("ply ascii")
    {
        opt.encoding = io::FileEncoding::Ascii;
        std::stringstream buffer;
        REQUIRE_NOTHROW(io::save_mesh_ply(buffer, cube, opt));
        auto loaded = io::load_mesh_ply<SurfaceMesh32d>(buffer);
        ensure_attributes_exist(loaded, false, true);
        testing::ensure_approx_equivalent_usage<AttributeUsage::Normal>(cube, loaded);
    }

    SECTION("ply binary")
    {
        opt.encoding = io::FileEncoding::Binary;
        std::stringstream buffer;
        REQUIRE_NOTHROW(io::save_mesh_ply(buffer, cube, opt));
        auto loaded = io::load_mesh_ply<SurfaceMesh32d>(buffer);
        ensure_attributes_exist(loaded, false, true);
        testing::ensure_approx_equivalent_usage<AttributeUsage::Normal>(cube, loaded);
    }
}
