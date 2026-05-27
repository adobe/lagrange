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
#include <lagrange/testing/common.h>
#include <lagrange/testing/create_test_mesh.h>

#include <lagrange/Attribute.h>
#include <lagrange/attribute_names.h>
#include <lagrange/compute_normal.h>
#include <lagrange/compute_seam_edges.h>
#include <lagrange/internal/constants.h>

using Scalar = double;
using Index = uint32_t;

namespace {

size_t count_seam_edges(const lagrange::SurfaceMesh<Scalar, Index>& mesh, lagrange::AttributeId id)
{
    const auto& seam_edges = mesh.get_attribute<uint8_t>(id).get_all();
    return std::count(seam_edges.begin(), seam_edges.end(), uint8_t(1));
}

} // namespace

TEST_CASE("compute_seam_edges", "[core][seam]")
{
    SECTION("Cube")
    {
        lagrange::testing::CreateOptions options;
        options.with_indexed_uv = true;
        options.with_indexed_normal = true;
        auto mesh = lagrange::testing::create_test_cube<Scalar, Index>(options);

        auto uv_seam_id = lagrange::compute_seam_edges(
            mesh,
            mesh.get_attribute_id(lagrange::AttributeName::texcoord));
        REQUIRE(count_seam_edges(mesh, uv_seam_id) == 7);

        lagrange::NormalOptions normal_options;
        auto normal_seam_id = lagrange::compute_seam_edges(
            mesh,
            mesh.get_attribute_id(normal_options.output_attribute_name));
        REQUIRE(count_seam_edges(mesh, normal_seam_id) == 12);
    }

    SECTION("Sphere")
    {
        lagrange::testing::CreateOptions options;
        options.with_indexed_uv = true;
        options.with_indexed_normal = false;
        auto mesh = lagrange::testing::create_test_sphere<Scalar, Index>(options);
        auto normal_id =
            lagrange::compute_normal(mesh, static_cast<Scalar>(lagrange::internal::pi / 4));

        auto uv_seam_id = lagrange::compute_seam_edges(
            mesh,
            mesh.get_attribute_id(lagrange::AttributeName::texcoord));
        REQUIRE(count_seam_edges(mesh, uv_seam_id) == 22);

        auto normal_seam_id = lagrange::compute_seam_edges(mesh, normal_id);
        REQUIRE(count_seam_edges(mesh, normal_seam_id) == 0);
    }

    SECTION("Quad with boundary")
    {
        // Two triangles sharing one interior edge. The indexed UV uses distinct
        // indices on each side of that shared edge, so the interior edge is a seam.
        // The four other edges are boundary edges.
        lagrange::SurfaceMesh<Scalar, Index> mesh(2);
        mesh.add_vertex({0, 0});
        mesh.add_vertex({1, 0});
        mesh.add_vertex({1, 1});
        mesh.add_vertex({0, 1});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(0, 2, 3);

        // 6 distinct UV values (one per corner) so the shared interior edge is a seam.
        Scalar uv_values[] = {
            0,
            0,
            1,
            0,
            2,
            0, // Triangle 0 corners
            3,
            0,
            4,
            0,
            5,
            0, // Triangle 1 corners
        };
        Index uv_indices[] = {0, 1, 2, 3, 4, 5};
        auto uv_id = mesh.template create_attribute<Scalar>(
            "uv",
            lagrange::AttributeElement::Indexed,
            2,
            lagrange::AttributeUsage::UV,
            {uv_values, 12},
            {uv_indices, 6});

        auto seam_id = lagrange::compute_seam_edges(mesh, uv_id);
        REQUIRE(count_seam_edges(mesh, seam_id) == 1);

        lagrange::SeamEdgesOptions opts;
        opts.include_boundary_edges = true;
        opts.output_attribute_name = "@seam_edges_with_boundary";
        auto seam_with_boundary_id = lagrange::compute_seam_edges(mesh, uv_id, opts);
        REQUIRE(count_seam_edges(mesh, seam_with_boundary_id) == 5);
    }
}
