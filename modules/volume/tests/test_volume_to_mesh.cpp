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
#include <lagrange/compute_vertex_normal.h>
#include <lagrange/create_mesh.h>
#include <lagrange/find_matching_attributes.h>
#include <lagrange/io/save_mesh.h>
#include <lagrange/mesh_convert.h>
#include <lagrange/testing/common.h>
#include <lagrange/testing/create_test_mesh.h>
#include <lagrange/views.h>
#include <lagrange/volume/mesh_to_volume.h>
#include <lagrange/volume/volume_to_mesh.h>

#include <catch2/matchers/catch_matchers_floating_point.hpp>

TEST_CASE("volume_to_mesh", "[volume]")
{
    using Scalar = float;
    using Index = uint32_t;
    using SurfaceMeshType = lagrange::SurfaceMesh<Scalar, Index>;
    auto mesh_in = lagrange::to_surface_mesh_copy<Scalar, Index>(*lagrange::create_sphere(3));
    lagrange::volume::MeshToVolumeOptions m2v_opt;
    m2v_opt.voxel_size = 0.1;
    auto grid = lagrange::volume::mesh_to_volume(mesh_in, m2v_opt);

    SECTION("without normals")
    {
        lagrange::volume::VolumeToMeshOptions v2m_opt;
        v2m_opt.normal_attribute_name = "";
        auto mesh_out = lagrange::volume::volume_to_mesh<SurfaceMeshType>(*grid, v2m_opt);
        REQUIRE(!lagrange::find_matching_attribute(mesh_out, lagrange::AttributeUsage::Normal)
                     .has_value());
    }

    SECTION("with normals")
    {
        lagrange::volume::VolumeToMeshOptions v2m_opt;
        v2m_opt.normal_attribute_name = "normal";
        auto mesh_out = lagrange::volume::volume_to_mesh<SurfaceMeshType>(*grid, v2m_opt);
        lagrange::AttributeMatcher matcher;
        matcher.usages = lagrange::AttributeUsage::Normal;
        matcher.element_types = lagrange::AttributeElement::Vertex;
        matcher.num_channels = 3;
        REQUIRE(lagrange::find_matching_attribute(mesh_out, matcher).has_value());

        auto normals_grid =
            lagrange::attribute_matrix_view<Scalar>(mesh_out, v2m_opt.normal_attribute_name);

        auto normals_mesh = lagrange::attribute_matrix_view<Scalar>(
            mesh_out,
            lagrange::compute_vertex_normal(mesh_out));

        for (Index v = 0; v < mesh_out.get_num_vertices(); ++v) {
            Eigen::RowVector3<Scalar> n_grid = normals_grid.row(v);
            REQUIRE_THAT(n_grid.norm(), Catch::Matchers::WithinRel(1, 1e-3f));
        }
        for (Index v = 0; v < mesh_out.get_num_vertices(); ++v) {
            Eigen::RowVector3<Scalar> n_grid = normals_grid.row(v);
            Eigen::RowVector3<Scalar> n_mesh = normals_mesh.row(v);
            CAPTURE(n_grid, n_mesh, (n_grid - n_mesh).norm());
            REQUIRE_THAT((n_grid - n_mesh).norm(), Catch::Matchers::WithinAbs(0, 1e-1f));
        }
    }
}
