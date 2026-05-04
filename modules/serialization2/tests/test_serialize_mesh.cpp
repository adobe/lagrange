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
#include <lagrange/IndexedAttribute.h>
#include <lagrange/SurfaceMesh.h>
#include <lagrange/primitive/generate_sphere.h>
#include <lagrange/serialization/serialize_mesh.h>
#include <lagrange/testing/check_meshes_equal.h>
#include <lagrange/testing/common.h>

#include <lagrange/fs/filesystem.h>

#include <catch2/catch_test_macros.hpp>

namespace {

template <typename Scalar, typename Index>
lagrange::SurfaceMesh<Scalar, Index> make_test_sphere()
{
    lagrange::primitive::SphereOptions opts;
    opts.num_longitude_sections = 8;
    opts.num_latitude_sections = 8;
    return lagrange::primitive::generate_sphere<Scalar, Index>(opts);
}

template <typename Scalar, typename Index>
lagrange::SurfaceMesh<Scalar, Index> make_large_test_sphere()
{
    lagrange::primitive::SphereOptions opts;
    opts.num_longitude_sections = 32;
    opts.num_latitude_sections = 32;
    return lagrange::primitive::generate_sphere<Scalar, Index>(opts);
}

} // namespace

TEST_CASE("serialization2: empty mesh", "[serialization2]")
{
    using Scalar = double;
    using Index = uint32_t;
    lagrange::SurfaceMesh<Scalar, Index> mesh;

    SECTION("uncompressed")
    {
        lagrange::serialization::SerializeOptions opts;
        opts.compress = false;
        auto buf = lagrange::serialization::serialize_mesh(mesh, opts);
        auto result =
            lagrange::serialization::deserialize_mesh<lagrange::SurfaceMesh<Scalar, Index>>(buf);
        lagrange::testing::check_meshes_equal(mesh, result);
    }

    SECTION("compressed")
    {
        auto buf = lagrange::serialization::serialize_mesh(mesh);
        auto result =
            lagrange::serialization::deserialize_mesh<lagrange::SurfaceMesh<Scalar, Index>>(buf);
        lagrange::testing::check_meshes_equal(mesh, result);
    }
}

TEST_CASE("serialization2: triangle mesh round-trip", "[serialization2]")
{
    using Scalar = double;
    using Index = uint32_t;

    auto mesh = make_test_sphere<Scalar, Index>();

    SECTION("uncompressed")
    {
        lagrange::serialization::SerializeOptions opts;
        opts.compress = false;
        auto buf = lagrange::serialization::serialize_mesh(mesh, opts);
        auto result =
            lagrange::serialization::deserialize_mesh<lagrange::SurfaceMesh<Scalar, Index>>(buf);
        lagrange::testing::check_meshes_equal(mesh, result);
    }

    SECTION("compressed")
    {
        auto buf = lagrange::serialization::serialize_mesh(mesh);
        auto result =
            lagrange::serialization::deserialize_mesh<lagrange::SurfaceMesh<Scalar, Index>>(buf);
        lagrange::testing::check_meshes_equal(mesh, result);
    }
}

TEST_CASE("serialization2: user attributes", "[serialization2]")
{
    using Scalar = float;
    using Index = uint32_t;

    auto mesh = make_test_sphere<Scalar, Index>();

    // Add per-vertex color (4 channels)
    {
        std::vector<float> colors(mesh.get_num_vertices() * 4);
        for (size_t i = 0; i < colors.size(); ++i) {
            colors[i] = static_cast<float>(i) / static_cast<float>(colors.size());
        }
        mesh.template create_attribute<float>(
            "color",
            lagrange::AttributeElement::Vertex,
            lagrange::AttributeUsage::Color,
            4,
            lagrange::span<const float>(colors.data(), colors.size()));
    }

    // Add per-facet label (1 channel, integer)
    {
        std::vector<int32_t> labels(mesh.get_num_facets());
        for (size_t i = 0; i < labels.size(); ++i) {
            labels[i] = static_cast<int32_t>(i % 5);
        }
        mesh.template create_attribute<int32_t>(
            "label",
            lagrange::AttributeElement::Facet,
            lagrange::AttributeUsage::Scalar,
            1,
            lagrange::span<const int32_t>(labels.data(), labels.size()));
    }

    // Add per-corner weight (1 channel)
    {
        std::vector<Scalar> weights(mesh.get_num_corners());
        for (size_t i = 0; i < weights.size(); ++i) {
            weights[i] = static_cast<Scalar>(i) * Scalar(0.01);
        }
        mesh.template create_attribute<Scalar>(
            "weight",
            lagrange::AttributeElement::Corner,
            lagrange::AttributeUsage::Scalar,
            1,
            lagrange::span<const Scalar>(weights.data(), weights.size()));
    }

    auto buf = lagrange::serialization::serialize_mesh(mesh);
    auto result =
        lagrange::serialization::deserialize_mesh<lagrange::SurfaceMesh<Scalar, Index>>(buf);
    lagrange::testing::check_meshes_equal(mesh, result);
}

TEST_CASE("serialization2: indexed attributes", "[serialization2]")
{
    using Scalar = double;
    using Index = uint32_t;

    auto mesh = make_test_sphere<Scalar, Index>();

    // Add an indexed UV attribute
    {
        size_t num_uv_values = 10;
        std::vector<Scalar> uv_values(num_uv_values * 2);
        for (size_t i = 0; i < uv_values.size(); ++i) {
            uv_values[i] = static_cast<Scalar>(i) / static_cast<Scalar>(uv_values.size());
        }
        std::vector<Index> uv_indices(mesh.get_num_corners());
        for (size_t i = 0; i < uv_indices.size(); ++i) {
            uv_indices[i] = static_cast<Index>(i % num_uv_values);
        }
        mesh.template create_attribute<Scalar>(
            "uv",
            lagrange::AttributeElement::Indexed,
            lagrange::AttributeUsage::UV,
            2,
            lagrange::span<const Scalar>(uv_values.data(), uv_values.size()),
            lagrange::span<const Index>(uv_indices.data(), uv_indices.size()));
    }

    auto buf = lagrange::serialization::serialize_mesh(mesh);
    auto result =
        lagrange::serialization::deserialize_mesh<lagrange::SurfaceMesh<Scalar, Index>>(buf);
    lagrange::testing::check_meshes_equal(mesh, result);
}

TEST_CASE("serialization2: hybrid mesh", "[serialization2]")
{
    using Scalar = double;
    using Index = uint32_t;

    lagrange::SurfaceMesh<Scalar, Index> mesh(3);

    // Create a mesh with mixed polygon sizes
    mesh.add_vertices(
        6,
        {0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0, 1.0, 0.0, 2.0, 0.0, 0.0, 2.0, 1.0, 0.0});

    // Add a triangle and a quad
    mesh.add_polygon({0, 1, 2});
    mesh.add_polygon({0, 2, 3});
    mesh.add_polygon({1, 4, 5, 2});

    REQUIRE(mesh.is_hybrid());

    auto buf = lagrange::serialization::serialize_mesh(mesh);
    auto result =
        lagrange::serialization::deserialize_mesh<lagrange::SurfaceMesh<Scalar, Index>>(buf);
    lagrange::testing::check_meshes_equal(mesh, result);
}

TEST_CASE("serialization2: edge topology", "[serialization2]")
{
    using Scalar = float;
    using Index = uint32_t;

    auto mesh = make_test_sphere<Scalar, Index>();
    mesh.initialize_edges();
    REQUIRE(mesh.get_num_edges() > 0);

    auto buf = lagrange::serialization::serialize_mesh(mesh);
    auto result =
        lagrange::serialization::deserialize_mesh<lagrange::SurfaceMesh<Scalar, Index>>(buf);

    REQUIRE(result.get_num_edges() == mesh.get_num_edges());
    lagrange::testing::check_meshes_equal(mesh, result);
}

TEST_CASE("serialization2: compression reduces size", "[serialization2]")
{
    using Scalar = double;
    using Index = uint32_t;

    auto mesh = make_large_test_sphere<Scalar, Index>();

    lagrange::serialization::SerializeOptions raw_opts;
    raw_opts.compress = false;
    auto buf_raw = lagrange::serialization::serialize_mesh(mesh, raw_opts);

    auto buf_compressed = lagrange::serialization::serialize_mesh(mesh);

    REQUIRE(buf_compressed.size() < buf_raw.size());

    auto result = lagrange::serialization::deserialize_mesh<lagrange::SurfaceMesh<Scalar, Index>>(
        buf_compressed);
    lagrange::testing::check_meshes_equal(mesh, result);
}

TEST_CASE("serialization2: compression levels", "[serialization2]")
{
    using Scalar = double;
    using Index = uint32_t;

    auto mesh = make_test_sphere<Scalar, Index>();

    for (int level : {1, 10, 22}) {
        lagrange::serialization::SerializeOptions opts;
        opts.compression_level = level;
        auto buf = lagrange::serialization::serialize_mesh(mesh, opts);
        auto result =
            lagrange::serialization::deserialize_mesh<lagrange::SurfaceMesh<Scalar, Index>>(buf);
        lagrange::testing::check_meshes_equal(mesh, result);
    }
}

TEST_CASE("serialization2: all mesh type instantiations", "[serialization2]")
{
    SECTION("float, uint32_t")
    {
        auto mesh = make_test_sphere<float, uint32_t>();
        auto buf = lagrange::serialization::serialize_mesh(mesh);
        auto result =
            lagrange::serialization::deserialize_mesh<lagrange::SurfaceMesh<float, uint32_t>>(buf);
        lagrange::testing::check_meshes_equal(mesh, result);
    }

    SECTION("double, uint32_t")
    {
        auto mesh = make_test_sphere<double, uint32_t>();
        auto buf = lagrange::serialization::serialize_mesh(mesh);
        auto result =
            lagrange::serialization::deserialize_mesh<lagrange::SurfaceMesh<double, uint32_t>>(buf);
        lagrange::testing::check_meshes_equal(mesh, result);
    }

    SECTION("float, uint64_t")
    {
        auto mesh = make_test_sphere<float, uint64_t>();
        auto buf = lagrange::serialization::serialize_mesh(mesh);
        auto result =
            lagrange::serialization::deserialize_mesh<lagrange::SurfaceMesh<float, uint64_t>>(buf);
        lagrange::testing::check_meshes_equal(mesh, result);
    }

    SECTION("double, uint64_t")
    {
        auto mesh = make_test_sphere<double, uint64_t>();
        auto buf = lagrange::serialization::serialize_mesh(mesh);
        auto result =
            lagrange::serialization::deserialize_mesh<lagrange::SurfaceMesh<double, uint64_t>>(buf);
        lagrange::testing::check_meshes_equal(mesh, result);
    }
}

TEST_CASE("serialization2: type mismatch detection", "[serialization2]")
{
    using Scalar = float;
    using Index = uint32_t;

    auto mesh = make_test_sphere<Scalar, Index>();
    auto buf = lagrange::serialization::serialize_mesh(mesh);

    LA_REQUIRE_THROWS(
        lagrange::serialization::deserialize_mesh<lagrange::SurfaceMesh<double, uint32_t>>(buf));
    LA_REQUIRE_THROWS(
        lagrange::serialization::deserialize_mesh<lagrange::SurfaceMesh<float, uint64_t>>(buf));
}

TEST_CASE("serialization2: integrity check detects corruption", "[serialization2]")
{
    using Scalar = float;
    using Index = uint32_t;

    auto mesh = make_test_sphere<Scalar, Index>();
    auto buf = lagrange::serialization::serialize_mesh(mesh);

    // Corrupt a byte in the middle of the buffer
    REQUIRE(buf.size() > 100);
    buf[buf.size() / 2] ^= 0xFF;

    LA_REQUIRE_THROWS(
        lagrange::serialization::deserialize_mesh<lagrange::SurfaceMesh<Scalar, Index>>(buf));
}

TEST_CASE("serialization2: format version is accessible", "[serialization2]")
{
    REQUIRE(lagrange::serialization::mesh_format_version() >= 1);
}

TEST_CASE("serialization2: user attribute ids are preserved", "[serialization2]")
{
    using Scalar = float;
    using Index = uint32_t;

    auto mesh = make_test_sphere<Scalar, Index>();

    // Record original user attribute ids
    std::vector<std::pair<std::string, lagrange::AttributeId>> original_ids;
    mesh.seq_foreach_attribute_id([&](std::string_view name, lagrange::AttributeId id) {
        if (!lagrange::SurfaceMesh<Scalar, Index>::attr_name_is_reserved(name)) {
            original_ids.emplace_back(std::string(name), id);
        }
    });
    REQUIRE(!original_ids.empty());

    auto buf = lagrange::serialization::serialize_mesh(mesh);
    auto result =
        lagrange::serialization::deserialize_mesh<lagrange::SurfaceMesh<Scalar, Index>>(buf);

    for (const auto& [name, expected_id] : original_ids) {
        INFO("Checking attribute id for: " << name);
        REQUIRE(result.has_attribute(name));
        REQUIRE(result.get_attribute_id(name) == expected_id);
    }
}

TEST_CASE("serialization2: file round-trip", "[serialization2]")
{
    using Scalar = double;
    using Index = uint32_t;

    auto mesh = make_test_sphere<Scalar, Index>();

    auto tmp_dir = lagrange::fs::temp_directory_path();

    SECTION("compressed")
    {
        auto path = tmp_dir / "test_mesh_compressed.lmesh";
        lagrange::serialization::save_mesh(path, mesh);
        auto result =
            lagrange::serialization::load_mesh<lagrange::SurfaceMesh<Scalar, Index>>(path);
        lagrange::testing::check_meshes_equal(mesh, result);
        lagrange::fs::remove(path);
    }

    SECTION("uncompressed")
    {
        auto path = tmp_dir / "test_mesh_uncompressed.lmesh";
        lagrange::serialization::SerializeOptions opts;
        opts.compress = false;
        lagrange::serialization::save_mesh(path, mesh, opts);
        auto result =
            lagrange::serialization::load_mesh<lagrange::SurfaceMesh<Scalar, Index>>(path);
        lagrange::testing::check_meshes_equal(mesh, result);
        lagrange::fs::remove(path);
    }
}
