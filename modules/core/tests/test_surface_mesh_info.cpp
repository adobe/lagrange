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
#include <lagrange/SurfaceMesh.h>
#include <lagrange/internal/surface_mesh_info_convert.h>
#include <lagrange/testing/check_meshes_equal.h>

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace {

template <typename Scalar, typename Index>
void check_unaligned_attribute_round_trip()
{
    lagrange::SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertices(3, {0, 0, 0, 1, 0, 0, 0, 1, 0});
    mesh.add_triangle(0, 1, 2);

    // Include 64-bit non-indexed values, scalar-valued indexed data, and mesh indices.
    const std::vector<uint64_t> labels = {11, 22, 33};
    mesh.template create_attribute<uint64_t>(
        "labels",
        lagrange::AttributeElement::Vertex,
        lagrange::AttributeUsage::Scalar,
        1,
        lagrange::span<const uint64_t>(labels.data(), labels.size()));

    const std::vector<Scalar> uv_values = {0, 0, 1, 0, 0, 1};
    const std::vector<Index> uv_indices = {2, 0, 1};
    mesh.template create_attribute<Scalar>(
        "uv",
        lagrange::AttributeElement::Indexed,
        lagrange::AttributeUsage::UV,
        2,
        lagrange::span<const Scalar>(uv_values.data(), uv_values.size()),
        lagrange::span<const Index>(uv_indices.data(), uv_indices.size()));

    bool unalign_data = false;
    bool unalign_values = false;
    bool unalign_indices = false;
    SECTION("non-indexed data")
    {
        unalign_data = true;
    }
    SECTION("indexed values")
    {
        unalign_values = true;
    }
    SECTION("indexed indices")
    {
        unalign_indices = true;
    }
    SECTION("all attribute buffers")
    {
        unalign_data = unalign_values = unalign_indices = true;
    }

    auto info = lagrange::internal::from_surface_mesh(mesh);
    std::vector<std::vector<std::max_align_t>> storage;
    storage.reserve(info.attributes.size() * 3);
    auto unaligned_copy = [&](lagrange::span<const uint8_t> bytes) {
        if (bytes.empty()) return bytes;
        auto& buffer = storage.emplace_back(
            (bytes.size() + sizeof(std::max_align_t)) / sizeof(std::max_align_t));
        auto* data = reinterpret_cast<uint8_t*>(buffer.data()) + 1;
        REQUIRE(reinterpret_cast<uintptr_t>(data) % alignof(std::max_align_t) == 1);
        std::memcpy(data, bytes.data(), bytes.size());
        return lagrange::span<const uint8_t>(data, bytes.size());
    };

    // Byte spans need not be aligned for their value types. Run UBSan with
    // halt_on_error=1: x86 can otherwise accept the old unaligned typed reads.
    for (auto& attr : info.attributes) {
        if (unalign_data) attr.data_bytes = unaligned_copy(attr.data_bytes);
        if (unalign_values) attr.values_bytes = unaligned_copy(attr.values_bytes);
        if (unalign_indices) attr.indices_bytes = unaligned_copy(attr.indices_bytes);
    }

    auto result = lagrange::internal::to_surface_mesh<Scalar, Index>(info);
    storage.clear(); // Restoration must own its attribute data, not retain the byte spans.
    lagrange::testing::check_meshes_equal(mesh, result);
}

} // namespace

TEST_CASE("SurfaceMeshInfo: unaligned byte round-trip", "[core][surface_mesh_info]")
{
    SECTION("float, uint32_t")
    {
        check_unaligned_attribute_round_trip<float, uint32_t>();
    }
    SECTION("double, uint32_t")
    {
        check_unaligned_attribute_round_trip<double, uint32_t>();
    }
    SECTION("float, uint64_t")
    {
        check_unaligned_attribute_round_trip<float, uint64_t>();
    }
    SECTION("double, uint64_t")
    {
        check_unaligned_attribute_round_trip<double, uint64_t>();
    }
}
