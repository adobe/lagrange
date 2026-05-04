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
#include <lagrange/testing/common.h>

#include <lagrange/internal/set_invalid_indexed_values.h>

TEST_CASE("set_invalid_indexed_values", "[core][attribute][indexed]")
{
    using namespace lagrange;
    using Scalar = float;
    using Index = uint32_t;

    SECTION("no invalid indices")
    {
        SurfaceMesh<Scalar, Index> mesh(2);
        mesh.add_vertex({0, 0});
        mesh.add_vertex({1, 0});
        mesh.add_vertex({0, 1});
        mesh.add_triangle(0, 1, 2);

        std::array<Scalar, 6> uv_values{0, 0, 1, 0, 0, 1};
        std::array<Index, 3> uv_indices{0, 1, 2};
        auto id = mesh.create_attribute<Scalar>(
            "uv",
            AttributeElement::Indexed,
            AttributeUsage::UV,
            2,
            uv_values,
            uv_indices);

        auto& attr = mesh.ref_indexed_attribute<Scalar>(id);
        internal::set_invalid_indexed_values(attr);

        // Nothing should change.
        REQUIRE(attr.values().get_num_elements() == 3);
        REQUIRE(attr.indices().get(0) == 0);
        REQUIRE(attr.indices().get(1) == 1);
        REQUIRE(attr.indices().get(2) == 2);
    }

    SECTION("some invalid indices")
    {
        SurfaceMesh<Scalar, Index> mesh(2);
        mesh.add_vertex({0, 0});
        mesh.add_vertex({1, 0});
        mesh.add_vertex({0, 1});
        mesh.add_vertex({1, 1});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(2, 1, 3);

        std::array<Scalar, 4> uv_values{0, 0, 1, 0};
        std::array<Index, 6>
            uv_indices{0, 1, invalid<Index>(), invalid<Index>(), 1, invalid<Index>()};
        auto id = mesh.create_attribute<Scalar>(
            "uv",
            AttributeElement::Indexed,
            AttributeUsage::UV,
            2,
            uv_values,
            uv_indices);

        auto& attr = mesh.ref_indexed_attribute<Scalar>(id);
        internal::set_invalid_indexed_values(attr);

        // 3 invalid indices should create 3 new values (indices 2, 3, 4).
        REQUIRE(attr.values().get_num_elements() == 5);

        // Original valid indices unchanged.
        CHECK(attr.indices().get(0) == 0);
        CHECK(attr.indices().get(1) == 1);
        CHECK(attr.indices().get(4) == 1);

        // Each invalid index gets a unique new value index.
        Index i2 = attr.indices().get(2);
        Index i3 = attr.indices().get(3);
        Index i5 = attr.indices().get(5);
        CHECK(i2 != invalid<Index>());
        CHECK(i3 != invalid<Index>());
        CHECK(i5 != invalid<Index>());
        CHECK(i2 >= 2);
        CHECK(i3 >= 2);
        CHECK(i5 >= 2);
        // All distinct.
        CHECK(i2 != i3);
        CHECK(i2 != i5);
        CHECK(i3 != i5);

        // New values should be set to invalid<Scalar>().
        for (Index idx : {i2, i3, i5}) {
            CHECK(attr.values().get(idx, 0) == invalid<Scalar>());
            CHECK(attr.values().get(idx, 1) == invalid<Scalar>());
        }
    }

    SECTION("all invalid indices")
    {
        SurfaceMesh<Scalar, Index> mesh(2);
        mesh.add_vertex({0, 0});
        mesh.add_vertex({1, 0});
        mesh.add_vertex({0, 1});
        mesh.add_triangle(0, 1, 2);

        std::array<Scalar, 2> uv_values{0, 0};
        std::array<Index, 3> uv_indices{invalid<Index>(), invalid<Index>(), invalid<Index>()};
        auto id = mesh.create_attribute<Scalar>(
            "uv",
            AttributeElement::Indexed,
            AttributeUsage::UV,
            2,
            uv_values,
            uv_indices);

        auto& attr = mesh.ref_indexed_attribute<Scalar>(id);
        internal::set_invalid_indexed_values(attr);

        // Original 1 value + 3 new.
        REQUIRE(attr.values().get_num_elements() == 4);
        // Each gets a unique index.
        CHECK(attr.indices().get(0) == 1);
        CHECK(attr.indices().get(1) == 2);
        CHECK(attr.indices().get(2) == 3);
    }
}
