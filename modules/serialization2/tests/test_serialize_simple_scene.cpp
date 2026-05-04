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
#include <lagrange/primitive/generate_sphere.h>
#include <lagrange/serialization/serialize_simple_scene.h>
#include <lagrange/testing/check_simple_scenes_equal.h>
#include <lagrange/testing/common.h>

#include <lagrange/fs/filesystem.h>

// Internal headers for corruption tests
#include "../src/CistaSimpleScene.h"
#include "../src/compress.h"

#include <catch2/catch_test_macros.hpp>

#include <cstring>

namespace {

template <typename Scalar, typename Index>
lagrange::SurfaceMesh<Scalar, Index> make_test_sphere()
{
    lagrange::primitive::SphereOptions opts;
    opts.num_longitude_sections = 8;
    opts.num_latitude_sections = 8;
    return lagrange::primitive::generate_sphere<Scalar, Index>(opts);
}

} // namespace

TEST_CASE("serialization2: empty simple scene", "[serialization2][simple_scene]")
{
    using Scalar = double;
    using Index = uint32_t;
    lagrange::scene::SimpleScene<Scalar, Index, 3> scene;

    SECTION("uncompressed")
    {
        lagrange::serialization::SerializeOptions opts;
        opts.compress = false;
        auto buf = lagrange::serialization::serialize_simple_scene(scene, opts);
        auto result = lagrange::serialization::deserialize_simple_scene<
            lagrange::scene::SimpleScene<Scalar, Index, 3>>(buf);
        lagrange::testing::check_simple_scenes_equal(scene, result);
    }

    SECTION("compressed")
    {
        auto buf = lagrange::serialization::serialize_simple_scene(scene);
        auto result = lagrange::serialization::deserialize_simple_scene<
            lagrange::scene::SimpleScene<Scalar, Index, 3>>(buf);
        lagrange::testing::check_simple_scenes_equal(scene, result);
    }
}

TEST_CASE(
    "serialization2: simple scene with meshes and instances",
    "[serialization2][simple_scene]")
{
    using Scalar = double;
    using Index = uint32_t;
    lagrange::scene::SimpleScene<Scalar, Index, 3> scene;

    auto mesh = make_test_sphere<Scalar, Index>();
    Index mesh_idx = scene.add_mesh(std::move(mesh));

    // Add instances with different transforms
    using Transform = Eigen::Transform<Scalar, 3, Eigen::Affine>;
    {
        lagrange::scene::MeshInstance<Scalar, Index, 3> inst;
        inst.mesh_index = mesh_idx;
        inst.transform = Transform::Identity();
        scene.add_instance(std::move(inst));
    }
    {
        lagrange::scene::MeshInstance<Scalar, Index, 3> inst;
        inst.mesh_index = mesh_idx;
        inst.transform = Transform::Identity();
        inst.transform.translate(Eigen::Matrix<Scalar, 3, 1>(1.0, 2.0, 3.0));
        scene.add_instance(std::move(inst));
    }
    {
        lagrange::scene::MeshInstance<Scalar, Index, 3> inst;
        inst.mesh_index = mesh_idx;
        inst.transform = Transform::Identity();
        inst.transform.rotate(
            Eigen::AngleAxis<Scalar>(Scalar(0.5), Eigen::Matrix<Scalar, 3, 1>::UnitZ()));
        scene.add_instance(std::move(inst));
    }

    auto buf = lagrange::serialization::serialize_simple_scene(scene);
    auto result = lagrange::serialization::deserialize_simple_scene<
        lagrange::scene::SimpleScene<Scalar, Index, 3>>(buf);
    lagrange::testing::check_simple_scenes_equal(scene, result);
}

TEST_CASE("serialization2: simple scene multiple meshes", "[serialization2][simple_scene]")
{
    using Scalar = float;
    using Index = uint32_t;
    lagrange::scene::SimpleScene<Scalar, Index, 3> scene;

    for (int k = 0; k < 3; ++k) {
        auto mesh = make_test_sphere<Scalar, Index>();
        Index mesh_idx = scene.add_mesh(std::move(mesh));

        lagrange::scene::MeshInstance<Scalar, Index, 3> inst;
        inst.mesh_index = mesh_idx;
        inst.transform = Eigen::Transform<Scalar, 3, Eigen::Affine>::Identity();
        inst.transform.translate(Eigen::Matrix<Scalar, 3, 1>(static_cast<Scalar>(k), 0.f, 0.f));
        scene.add_instance(std::move(inst));
    }

    auto buf = lagrange::serialization::serialize_simple_scene(scene);
    auto result = lagrange::serialization::deserialize_simple_scene<
        lagrange::scene::SimpleScene<Scalar, Index, 3>>(buf);
    lagrange::testing::check_simple_scenes_equal(scene, result);
}

TEST_CASE("serialization2: simple scene 2D", "[serialization2][simple_scene]")
{
    using Scalar = float;
    using Index = uint32_t;
    constexpr size_t Dim = 2;
    lagrange::scene::SimpleScene<Scalar, Index, Dim> scene;

    lagrange::SurfaceMesh<Scalar, Index> mesh(2);
    mesh.add_vertices(3, {0.f, 0.f, 1.f, 0.f, 0.f, 1.f});
    mesh.add_polygon({0, 1, 2});
    scene.add_mesh(std::move(mesh));

    lagrange::scene::MeshInstance<Scalar, Index, Dim> inst;
    inst.mesh_index = 0;
    inst.transform = Eigen::Transform<Scalar, 2, Eigen::Affine>::Identity();
    scene.add_instance(std::move(inst));

    auto buf = lagrange::serialization::serialize_simple_scene(scene);
    auto result = lagrange::serialization::deserialize_simple_scene<
        lagrange::scene::SimpleScene<Scalar, Index, Dim>>(buf);
    lagrange::testing::check_simple_scenes_equal(scene, result);
}

TEST_CASE("serialization2: simple scene all type instantiations", "[serialization2][simple_scene]")
{
    SECTION("float, uint32_t, 3")
    {
        lagrange::scene::SimpleScene<float, uint32_t, 3> scene;
        auto buf = lagrange::serialization::serialize_simple_scene(scene);
        auto result = lagrange::serialization::deserialize_simple_scene<
            lagrange::scene::SimpleScene<float, uint32_t, 3>>(buf);
        lagrange::testing::check_simple_scenes_equal(scene, result);
    }

    SECTION("double, uint32_t, 3")
    {
        lagrange::scene::SimpleScene<double, uint32_t, 3> scene;
        auto buf = lagrange::serialization::serialize_simple_scene(scene);
        auto result = lagrange::serialization::deserialize_simple_scene<
            lagrange::scene::SimpleScene<double, uint32_t, 3>>(buf);
        lagrange::testing::check_simple_scenes_equal(scene, result);
    }

    SECTION("float, uint64_t, 3")
    {
        lagrange::scene::SimpleScene<float, uint64_t, 3> scene;
        auto buf = lagrange::serialization::serialize_simple_scene(scene);
        auto result = lagrange::serialization::deserialize_simple_scene<
            lagrange::scene::SimpleScene<float, uint64_t, 3>>(buf);
        lagrange::testing::check_simple_scenes_equal(scene, result);
    }

    SECTION("double, uint64_t, 3")
    {
        lagrange::scene::SimpleScene<double, uint64_t, 3> scene;
        auto buf = lagrange::serialization::serialize_simple_scene(scene);
        auto result = lagrange::serialization::deserialize_simple_scene<
            lagrange::scene::SimpleScene<double, uint64_t, 3>>(buf);
        lagrange::testing::check_simple_scenes_equal(scene, result);
    }
}

TEST_CASE("serialization2: simple scene type mismatch", "[serialization2][simple_scene]")
{
    using Scalar = float;
    using Index = uint32_t;
    lagrange::scene::SimpleScene<Scalar, Index, 3> scene;
    auto mesh = make_test_sphere<Scalar, Index>();
    scene.add_mesh(std::move(mesh));

    auto buf = lagrange::serialization::serialize_simple_scene(scene);

    LA_REQUIRE_THROWS((lagrange::serialization::deserialize_simple_scene<
                       lagrange::scene::SimpleScene<double, uint32_t, 3>>(buf)));
    LA_REQUIRE_THROWS((lagrange::serialization::deserialize_simple_scene<
                       lagrange::scene::SimpleScene<float, uint64_t, 3>>(buf)));
}

namespace {

/// Helper to serialize a CistaSimpleScene directly (bypassing the public API) so we can construct
/// intentionally malformed buffers for negative testing.
std::vector<uint8_t> serialize_raw_cista_simple_scene(
    const lagrange::serialization::internal::CistaSimpleScene& cscene)
{
    using namespace lagrange::serialization::internal;
    cista::buf<std::vector<uint8_t>> buf;
    cista::serialize<k_cista_mode>(buf, cscene);
    return std::move(buf.buf_);
}

} // namespace

TEST_CASE("serialization2: simple scene corrupted buffer", "[serialization2][simple_scene]")
{
    using namespace lagrange::serialization::internal;
    using SceneType = lagrange::scene::SimpleScene<float, uint32_t, 3>;

    // Build a valid CistaSimpleScene with 1 mesh placeholder and 1 instance
    CistaSimpleScene cscene;
    cscene.version = 1;
    cscene.scalar_type_size = sizeof(float);
    cscene.index_type_size = sizeof(uint32_t);
    cscene.dimension = 3;

    // Add a minimal mesh (empty, but valid enough for the struct)
    cscene.meshes.emplace_back();

    // One mesh with one instance
    cscene.instances_per_mesh.push_back(1);

    CistaInstance cinst;
    cinst.mesh_index = 0;
    constexpr size_t byte_size = 4 * 4 * sizeof(float); // (3+1)^2 * sizeof(float)
    cinst.transform_bytes.resize(byte_size);
    std::memset(cinst.transform_bytes.data(), 0, byte_size);
    cscene.instances.emplace_back(std::move(cinst));

    SECTION("instances_per_mesh size mismatch")
    {
        // Add an extra entry to instances_per_mesh so it doesn't match meshes.size()
        cscene.instances_per_mesh.push_back(0);
        auto buf = serialize_raw_cista_simple_scene(cscene);
        LA_REQUIRE_THROWS(lagrange::serialization::deserialize_simple_scene<SceneType>(buf));
    }

    SECTION("instance offset out of bounds")
    {
        // Claim more instances than actually exist
        cscene.instances_per_mesh[0] = 99;
        auto buf = serialize_raw_cista_simple_scene(cscene);
        LA_REQUIRE_THROWS(lagrange::serialization::deserialize_simple_scene<SceneType>(buf));
    }

    SECTION("total instance count mismatch (trailing instances)")
    {
        // Add extra instances that are not accounted for by instances_per_mesh
        CistaInstance extra;
        extra.mesh_index = 0;
        extra.transform_bytes.resize(byte_size);
        std::memset(extra.transform_bytes.data(), 0, byte_size);
        cscene.instances.emplace_back(std::move(extra));
        auto buf = serialize_raw_cista_simple_scene(cscene);
        LA_REQUIRE_THROWS(lagrange::serialization::deserialize_simple_scene<SceneType>(buf));
    }

    SECTION("transform data size mismatch")
    {
        // Truncate the transform bytes
        cscene.instances[0].transform_bytes.resize(byte_size / 2);
        auto buf = serialize_raw_cista_simple_scene(cscene);
        LA_REQUIRE_THROWS(lagrange::serialization::deserialize_simple_scene<SceneType>(buf));
    }

    SECTION("cast path: instance offset out of bounds")
    {
        // Serialize as double/uint32_t, deserialize as float/uint32_t with casting
        cscene.scalar_type_size = sizeof(double);
        constexpr size_t double_byte_size = 4 * 4 * sizeof(double);
        cscene.instances[0].transform_bytes.resize(double_byte_size);
        std::memset(cscene.instances[0].transform_bytes.data(), 0, double_byte_size);
        cscene.instances_per_mesh[0] = 99; // More instances than exist
        auto buf = serialize_raw_cista_simple_scene(cscene);
        lagrange::serialization::DeserializeOptions opts;
        opts.allow_type_cast = true;
        LA_REQUIRE_THROWS(lagrange::serialization::deserialize_simple_scene<SceneType>(buf, opts));
    }

    SECTION("cast path: total instance count mismatch")
    {
        // Serialize as double/uint32_t with trailing instances
        cscene.scalar_type_size = sizeof(double);
        constexpr size_t double_byte_size = 4 * 4 * sizeof(double);
        cscene.instances[0].transform_bytes.resize(double_byte_size);
        std::memset(cscene.instances[0].transform_bytes.data(), 0, double_byte_size);
        CistaInstance extra;
        extra.mesh_index = 0;
        extra.transform_bytes.resize(double_byte_size);
        std::memset(extra.transform_bytes.data(), 0, double_byte_size);
        cscene.instances.emplace_back(std::move(extra));
        auto buf = serialize_raw_cista_simple_scene(cscene);
        lagrange::serialization::DeserializeOptions opts;
        opts.allow_type_cast = true;
        LA_REQUIRE_THROWS(lagrange::serialization::deserialize_simple_scene<SceneType>(buf, opts));
    }
}

TEST_CASE("serialization2: simple scene file round-trip", "[serialization2][simple_scene]")
{
    using Scalar = double;
    using Index = uint32_t;
    lagrange::scene::SimpleScene<Scalar, Index, 3> scene;

    auto mesh = make_test_sphere<Scalar, Index>();
    scene.add_mesh(std::move(mesh));

    lagrange::scene::MeshInstance<Scalar, Index, 3> inst;
    inst.mesh_index = 0;
    inst.transform = Eigen::Transform<Scalar, 3, Eigen::Affine>::Identity();
    scene.add_instance(std::move(inst));

    auto tmp_dir = lagrange::fs::temp_directory_path();

    SECTION("compressed")
    {
        auto path = tmp_dir / "test_simple_scene_compressed.lscene";
        lagrange::serialization::save_simple_scene(path, scene);
        auto result = lagrange::serialization::load_simple_scene<
            lagrange::scene::SimpleScene<Scalar, Index, 3>>(path);
        lagrange::testing::check_simple_scenes_equal(scene, result);
        lagrange::fs::remove(path);
    }

    SECTION("uncompressed")
    {
        auto path = tmp_dir / "test_simple_scene_uncompressed.lscene";
        lagrange::serialization::SerializeOptions opts;
        opts.compress = false;
        lagrange::serialization::save_simple_scene(path, scene, opts);
        auto result = lagrange::serialization::load_simple_scene<
            lagrange::scene::SimpleScene<Scalar, Index, 3>>(path);
        lagrange::testing::check_simple_scenes_equal(scene, result);
        lagrange::fs::remove(path);
    }
}
