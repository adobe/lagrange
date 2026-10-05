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
#include <lagrange/SurfaceMesh.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/attribute_names.h>
#include <lagrange/io/internal/detect_file_format.h>
#include <lagrange/io/load_mesh_pcd.h>
#include <lagrange/io/save_mesh_pcd.h>
#include <lagrange/testing/common.h>
#include <lagrange/views.h>

#include <catch2/catch_approx.hpp>

#include <limits>
#include <sstream>
#include <vector>

namespace {

template <typename Scalar, typename Index>
lagrange::SurfaceMesh<Scalar, Index> make_point_cloud()
{
    using namespace lagrange;
    SurfaceMesh<Scalar, Index> mesh;
    const Index n = 6;
    mesh.add_vertices(n, [&](Index v, span<Scalar> p) {
        p[0] = static_cast<Scalar>(v) * static_cast<Scalar>(1.5);
        p[1] = static_cast<Scalar>(v) * static_cast<Scalar>(-2.25);
        p[2] = static_cast<Scalar>(v) + static_cast<Scalar>(0.5);
    });

    std::vector<float> normals(static_cast<size_t>(n) * 3);
    for (Index v = 0; v < n; ++v) {
        normals[v * 3 + 0] = 1.0f;
        normals[v * 3 + 1] = 0.0f;
        normals[v * 3 + 2] = static_cast<float>(v) * 0.1f;
    }
    mesh.template create_attribute<float>(
        AttributeName::normal,
        AttributeElement::Vertex,
        AttributeUsage::Normal,
        3,
        {normals.data(), normals.size()});

    std::vector<uint8_t> colors(static_cast<size_t>(n) * 4);
    for (Index v = 0; v < n; ++v) {
        colors[v * 4 + 0] = static_cast<uint8_t>(10 * v);
        colors[v * 4 + 1] = static_cast<uint8_t>(20 * v);
        colors[v * 4 + 2] = static_cast<uint8_t>(30 * v);
        colors[v * 4 + 3] = 255;
    }
    mesh.template create_attribute<uint8_t>(
        AttributeName::color,
        AttributeElement::Vertex,
        AttributeUsage::Color,
        4,
        {colors.data(), colors.size()});

    std::vector<float> intensity(static_cast<size_t>(n));
    for (Index v = 0; v < n; ++v) intensity[v] = static_cast<float>(v) * 0.5f;
    mesh.template create_attribute<float>(
        "intensity",
        AttributeElement::Vertex,
        AttributeUsage::Scalar,
        1,
        {intensity.data(), intensity.size()});

    return mesh;
}

template <typename Scalar, typename Index>
void check_roundtrip(lagrange::io::FileEncoding encoding)
{
    using namespace lagrange;
    auto mesh = make_point_cloud<Scalar, Index>();

    io::SaveOptions save_options;
    save_options.encoding = encoding;

    std::stringstream data;
    REQUIRE_NOTHROW(io::save_mesh_pcd(data, mesh, save_options));

    auto mesh2 = io::load_mesh_pcd<SurfaceMesh<Scalar, Index>>(data);

    REQUIRE(mesh2.get_num_vertices() == mesh.get_num_vertices());
    REQUIRE(mesh2.get_num_facets() == 0);

    // Positions.
    auto pos = vertex_view(mesh);
    auto pos2 = vertex_view(mesh2);
    for (Index v = 0; v < mesh.get_num_vertices(); ++v) {
        for (int k = 0; k < 3; ++k) {
            REQUIRE(pos2(v, k) == Catch::Approx(pos(v, k)));
        }
    }

    // Normals.
    REQUIRE(mesh2.has_attribute(AttributeName::normal));
    const auto& nrm = mesh.template get_attribute<float>(AttributeName::normal);
    const auto& nrm2 = mesh2.template get_attribute<float>(AttributeName::normal);
    REQUIRE(nrm2.get_num_channels() == 3);
    for (Index v = 0; v < mesh.get_num_vertices(); ++v) {
        for (Index c = 0; c < 3; ++c) {
            REQUIRE(nrm2.get(v, c) == Catch::Approx(nrm.get(v, c)));
        }
    }

    // Colors (packed rgba -> uint8 4 channels, exact).
    REQUIRE(mesh2.has_attribute(AttributeName::color));
    const auto& col = mesh.template get_attribute<uint8_t>(AttributeName::color);
    const auto& col2 = mesh2.template get_attribute<uint8_t>(AttributeName::color);
    REQUIRE(col2.get_num_channels() == 4);
    for (Index v = 0; v < mesh.get_num_vertices(); ++v) {
        for (Index c = 0; c < 4; ++c) {
            REQUIRE(col2.get(v, c) == col.get(v, c));
        }
    }

    // Scalar attribute.
    REQUIRE(mesh2.has_attribute("intensity"));
    const auto& intensity = mesh.template get_attribute<float>("intensity");
    const auto& intensity2 = mesh2.template get_attribute<float>("intensity");
    REQUIRE(intensity2.get_num_channels() == 1);
    for (Index v = 0; v < mesh.get_num_vertices(); ++v) {
        REQUIRE(intensity2.get(v, 0) == Catch::Approx(intensity.get(v, 0)));
    }
}

} // namespace

TEST_CASE("io/pcd detection after comments", "[io][pcd]")
{
    std::istringstream stream(
        "# generated by tool\r\n \t\r\n\t# metadata\nVERSION 0.7\nFIELDS x y z\n");
    REQUIRE(lagrange::io::internal::detect_file_format(stream) == lagrange::io::FileFormat::Pcd);
    REQUIRE(stream.peek() == '#');

    std::istringstream obj("# mesh\nv 0 0 0\n");
    REQUIRE(lagrange::io::internal::detect_file_format(obj) == lagrange::io::FileFormat::Obj);
}

TEST_CASE("io/pcd roundtrip", "[io][pcd]")
{
    // Binary encoding is written as LZF-compressed `binary_compressed`.
    SECTION("Ascii double")
    {
        check_roundtrip<double, uint32_t>(lagrange::io::FileEncoding::Ascii);
    }
    SECTION("Binary double")
    {
        check_roundtrip<double, uint32_t>(lagrange::io::FileEncoding::Binary);
    }
    SECTION("Ascii float")
    {
        check_roundtrip<float, uint32_t>(lagrange::io::FileEncoding::Ascii);
    }
    SECTION("Binary float")
    {
        check_roundtrip<float, uint32_t>(lagrange::io::FileEncoding::Binary);
    }
}

TEST_CASE("io/pcd ascii fixture", "[io][pcd]")
{
    using namespace lagrange;
    // Minimal PCD ascii payload with x y z and an intensity field.
    const std::string payload = "# .PCD v0.7 - Point Cloud Data file format\n"
                                "VERSION 0.7\n"
                                "FIELDS x y z intensity\n"
                                "SIZE 4 4 4 4\n"
                                "TYPE F F F F\n"
                                "COUNT 1 1 1 1\n"
                                "WIDTH 3\n"
                                "HEIGHT 1\n"
                                "VIEWPOINT 0 0 0 1 0 0 0\n"
                                "POINTS 3\n"
                                "DATA ascii\n"
                                "0 0 0 1\n"
                                "1 2 3 2\n"
                                "-1 -2 -3 3\n";

    std::stringstream ss(payload);
    auto mesh = io::load_mesh_pcd<SurfaceMesh32f>(ss);
    REQUIRE(mesh.get_num_vertices() == 3);
    REQUIRE(mesh.get_num_facets() == 0);
    REQUIRE(mesh.has_attribute("intensity"));

    auto pos = vertex_view(mesh);
    REQUIRE(pos(1, 0) == Catch::Approx(1.0f));
    REQUIRE(pos(1, 1) == Catch::Approx(2.0f));
    REQUIRE(pos(2, 2) == Catch::Approx(-3.0f));

    const auto& intensity = mesh.get_attribute<float>("intensity");
    REQUIRE(intensity.get(2, 0) == Catch::Approx(3.0f));
}

TEST_CASE("io/pcd uncompressed binary fixture", "[io][pcd]")
{
    using namespace lagrange;
    // We always write `binary_compressed`, so build an uncompressed `binary` payload by hand to
    // exercise the uncompressed-binary read path (e.g. files produced by other tools).
    std::string payload = "# .PCD v0.7 - Point Cloud Data file format\n"
                          "VERSION 0.7\n"
                          "FIELDS x y z\n"
                          "SIZE 4 4 4\n"
                          "TYPE F F F\n"
                          "COUNT 1 1 1\n"
                          "WIDTH 2\n"
                          "HEIGHT 1\n"
                          "VIEWPOINT 0 0 0 1 0 0 0\n"
                          "POINTS 2\n"
                          "DATA binary\n";
    const float data[6] = {0.f, 1.f, 2.f, 10.f, 11.f, 12.f};
    payload.append(reinterpret_cast<const char*>(data), sizeof(data));

    std::stringstream ss(payload);
    auto mesh = io::load_mesh_pcd<SurfaceMesh32f>(ss);
    REQUIRE(mesh.get_num_vertices() == 2);
    REQUIRE(mesh.get_num_facets() == 0);

    auto pos = vertex_view(mesh);
    REQUIRE(pos(0, 0) == Catch::Approx(0.f));
    REQUIRE(pos(0, 2) == Catch::Approx(2.f));
    REQUIRE(pos(1, 0) == Catch::Approx(10.f));
    REQUIRE(pos(1, 2) == Catch::Approx(12.f));
}

TEST_CASE("io/pcd uint64 attribute round-trip", "[io][pcd]")
{
    using namespace lagrange;
    SurfaceMesh<double, uint32_t> mesh;
    mesh.add_vertices(2, [](uint32_t v, span<double> p) {
        p[0] = v;
        p[1] = v;
        p[2] = v;
    });
    // Value above INT64_MAX must survive the ascii round-trip.
    const uint64_t big = static_cast<uint64_t>(std::numeric_limits<int64_t>::max()) + 100u;
    std::vector<uint64_t> ids = {big, big - 1u};
    mesh.create_attribute<uint64_t>(
        "id",
        AttributeElement::Vertex,
        AttributeUsage::Scalar,
        1,
        {ids.data(), ids.size()});

    io::SaveOptions opts;
    opts.encoding = io::FileEncoding::Ascii;
    std::stringstream data;
    io::save_mesh_pcd(data, mesh, opts);

    auto mesh2 = io::load_mesh_pcd<SurfaceMesh<double, uint32_t>>(data);
    REQUIRE(mesh2.has_attribute("id"));
    const auto& id2 = mesh2.get_attribute<uint64_t>("id");
    REQUIRE(id2.get(0, 0) == big);
    REQUIRE(id2.get(1, 0) == big - 1u);
}

TEST_CASE("io/pcd reserved attribute name is skipped", "[io][pcd]")
{
    using namespace lagrange;
    SurfaceMesh<double, uint32_t> mesh;
    mesh.add_vertices(2, [](uint32_t v, span<double> p) {
        p[0] = v;
        p[1] = v + 1;
        p[2] = v + 2;
    });
    // An attribute literally named "x" collides with the position field and must not corrupt
    // output.
    std::vector<double> bogus = {7.0, 8.0};
    mesh.create_attribute<double>(
        "x",
        AttributeElement::Vertex,
        AttributeUsage::Scalar,
        1,
        {bogus.data(), bogus.size()});

    io::SaveOptions opts;
    opts.encoding = io::FileEncoding::Ascii;
    opts.quiet = true;
    std::stringstream data;
    io::save_mesh_pcd(data, mesh, opts);

    auto mesh2 = io::load_mesh_pcd<SurfaceMesh<double, uint32_t>>(data);
    REQUIRE(mesh2.get_num_vertices() == 2);
    auto pos = vertex_view(mesh2);
    REQUIRE(pos(1, 0) == Catch::Approx(1.0)); // position, not the bogus "x" value
    REQUIRE(pos(1, 1) == Catch::Approx(2.0));
}

TEST_CASE("io/pcd rejects malformed header", "[io][pcd]")
{
    using namespace lagrange;
    const std::string payload = "VERSION 0.7\n"
                                "FIELDS x y z\n"
                                "SIZE 4 4 -4\n" // negative size must be rejected
                                "TYPE F F F\n"
                                "COUNT 1 1 1\n"
                                "WIDTH 1\n"
                                "HEIGHT 1\n"
                                "POINTS 1\n"
                                "DATA ascii\n"
                                "0 0 0\n";
    std::stringstream ss(payload);
    LA_REQUIRE_THROWS(io::load_mesh_pcd<SurfaceMesh32f>(ss));
}

TEST_CASE("io/pcd binary_compressed liblzf interop", "[io][pcd]")
{
    using namespace lagrange;
    // This payload's compressed body was produced by the *reference* liblzf 3.6 `lzf_compress`
    // (the codec PCL uses for `binary_compressed` PCD). Loading it through load_mesh_pcd (which
    // delegates decompression to the pcdio dependency) verifies real liblzf/PCL interoperability
    // of the whole lagrange -> pcdio read path against externally produced bytes.
    //
    // The 16-point cloud has fields x y z intensity (all float32). Column-major (SoA) source:
    //   x = 0,1,...,15   y = 0   z = 2   intensity = 100
    // The repeated y/z/intensity columns force liblzf to emit back-references, so the payload
    // exercises the decoder's copy path in addition to literal runs.
    static const unsigned char kBlob[] = {
        0x01, 0x00, 0x00, 0x40, 0x00, 0x01, 0x80, 0x3f, 0x20, 0x05, 0x00, 0x40, 0x20, 0x02,
        0x20, 0x03, 0x00, 0x80, 0x20, 0x03, 0x00, 0xa0, 0x20, 0x03, 0x00, 0xc0, 0x20, 0x03,
        0x00, 0xe0, 0x20, 0x03, 0x04, 0x00, 0x41, 0x00, 0x00, 0x10, 0x20, 0x03, 0x00, 0x20,
        0x20, 0x03, 0x00, 0x30, 0x20, 0x03, 0x00, 0x40, 0x20, 0x03, 0x00, 0x50, 0x20, 0x03,
        0x00, 0x60, 0x20, 0x03, 0x00, 0x70, 0x20, 0x03, 0xe0, 0x38, 0x00, 0x40, 0x63, 0xe0,
        0x32, 0x03, 0x01, 0xc8, 0x42, 0xe0, 0x31, 0x03, 0x01, 0xc8, 0x42,
    };
    const uint32_t compressed_size = static_cast<uint32_t>(sizeof(kBlob)); // 81
    const uint32_t uncompressed_size = 16u * 4u * sizeof(float); // 256

    std::string payload = "# .PCD v0.7 - Point Cloud Data file format\n"
                          "VERSION 0.7\n"
                          "FIELDS x y z intensity\n"
                          "SIZE 4 4 4 4\n"
                          "TYPE F F F F\n"
                          "COUNT 1 1 1 1\n"
                          "WIDTH 16\n"
                          "HEIGHT 1\n"
                          "VIEWPOINT 0 0 0 1 0 0 0\n"
                          "POINTS 16\n"
                          "DATA binary_compressed\n";
    payload.append(reinterpret_cast<const char*>(&compressed_size), sizeof(compressed_size));
    payload.append(reinterpret_cast<const char*>(&uncompressed_size), sizeof(uncompressed_size));
    payload.append(reinterpret_cast<const char*>(kBlob), sizeof(kBlob));

    std::stringstream ss(payload);
    auto mesh = io::load_mesh_pcd<SurfaceMesh32f>(ss);
    REQUIRE(mesh.get_num_vertices() == 16);
    REQUIRE(mesh.get_num_facets() == 0);
    REQUIRE(mesh.has_attribute("intensity"));

    auto pos = vertex_view(mesh);
    const auto& intensity = mesh.get_attribute<float>("intensity");
    for (int v = 0; v < 16; ++v) {
        REQUIRE(pos(v, 0) == Catch::Approx(static_cast<float>(v)));
        REQUIRE(pos(v, 1) == Catch::Approx(0.0f));
        REQUIRE(pos(v, 2) == Catch::Approx(2.0f));
        REQUIRE(intensity.get(v, 0) == Catch::Approx(100.0f));
    }
}
