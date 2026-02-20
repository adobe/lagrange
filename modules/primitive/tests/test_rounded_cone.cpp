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
#include <lagrange/Attribute.h>
#include <lagrange/common.h>
#include <lagrange/io/save_mesh.h>
#include <lagrange/mesh_cleanup/detect_degenerate_triangles.h>
#include <lagrange/primitive/SemanticLabel.h>
#include <lagrange/primitive/generate_rounded_cone.h>
#include <lagrange/separate_by_components.h>
#include <lagrange/separate_by_facet_groups.h>
#include <lagrange/testing/common.h>
#include <lagrange/topology.h>
#include <lagrange/unify_index_buffer.h>
#include <lagrange/views.h>
#include <catch2/catch_approx.hpp>

#include "primitive_test_utils.h"

TEST_CASE("generate_rounded_cone", "[primitive][surface]")
{
    using namespace lagrange;
    using Scalar = float;
    using Index = uint32_t;

    auto check_topology = [](SurfaceMesh<Scalar, Index>& mesh) {
        REQUIRE(is_closed(mesh));
        REQUIRE(is_manifold(mesh));
        REQUIRE(compute_euler(mesh) == 2);
    };

    auto check_semantic_labels = [](SurfaceMesh<Scalar, Index>& mesh,
                                    primitive::RoundedConeOptions setting) {
        REQUIRE(mesh.has_attribute(setting.semantic_label_attribute_name));
        const auto& semantic_label_attr =
            mesh.get_attribute<uint8_t>(setting.semantic_label_attribute_name);
        REQUIRE(semantic_label_attr.get_element_type() == AttributeElement::Facet);
        auto labels = semantic_label_attr.get_all();
        for (auto l : labels) {
            REQUIRE(l < static_cast<uint8_t>(primitive::SemanticLabel::Unknown));
        }
    };

    auto check_normals = [](SurfaceMesh<Scalar, Index>& mesh,
                            primitive::RoundedConeOptions setting,
                            size_t num_smooth_patches) {
        auto mesh2 = unify_named_index_buffer(mesh, {setting.normal_attribute_name});
        auto patches = separate_by_components(mesh2);
        REQUIRE(patches.size() == num_smooth_patches);
    };

    primitive::RoundedConeOptions setting;

    SECTION("Simple cone")
    {
        setting.triangulate = true;
        auto mesh = primitive::generate_rounded_cone<Scalar, Index>(setting);
        check_topology(mesh);
        check_semantic_labels(mesh, setting);
        check_normals(mesh, setting, 2); // Side and bottom
    }

    SECTION("Tet")
    {
        setting.triangulate = true;
        setting.radial_sections = 3;
        auto mesh = primitive::generate_rounded_cone<Scalar, Index>(setting);
        check_topology(mesh);
        check_semantic_labels(mesh, setting);
        check_normals(mesh, setting, 4); // Normals should be sharp on all sides.
    }

    SECTION("Sphere")
    {
        setting.triangulate = true;
        setting.radius_top = 1.0f;
        setting.radius_bottom = 1.0f;
        setting.height = 2.0f;
        setting.bevel_radius_top = 1.0f;
        setting.bevel_radius_bottom = 1.0f;
        setting.bevel_segments_top = 16;
        setting.bevel_segments_bottom = 16;
        auto mesh = primitive::generate_rounded_cone<Scalar, Index>(setting);
        check_topology(mesh);
        check_semantic_labels(mesh, setting);
        check_normals(mesh, setting, 1); // A sphere is everywhere smooth.
    }

    SECTION("Cylinder")
    {
        setting.triangulate = true;
        setting.radius_top = 1.0f;
        setting.radius_bottom = 1.0f;
        setting.height = 2.0f;
        auto mesh = primitive::generate_rounded_cone<Scalar, Index>(setting);
        check_topology(mesh);
        check_semantic_labels(mesh, setting);
        check_normals(mesh, setting, 3); // Side, top and bottom
    }

    SECTION("Generic rounded cone")
    {
        setting.triangulate = true;
        setting.radius_top = 1.0f;
        setting.radius_bottom = 2.0f;
        setting.height = 3.0f;
        setting.bevel_radius_top = 0.5f;
        setting.bevel_radius_bottom = 0.25f;
        setting.bevel_segments_top = 32;
        setting.bevel_segments_bottom = 16;
        auto mesh = primitive::generate_rounded_cone<Scalar, Index>(setting);
        check_topology(mesh);
        check_semantic_labels(mesh, setting);
        check_normals(mesh, setting, 1); // Should be smooth everywhere.
    }

    SECTION("Zero height")
    {
        setting.triangulate = true;
        setting.radius_top = 1.0f;
        setting.radius_bottom = 2.0f;
        setting.height = 0.0f;
        auto mesh = primitive::generate_rounded_cone<Scalar, Index>(setting);
        REQUIRE(mesh.get_num_vertices() == 0);
    }

    SECTION("Zero radius")
    {
        setting.triangulate = true;
        setting.radius_top = 0.0f;
        setting.radius_bottom = 0.0f;
        setting.height = 1.0f;
        auto mesh = primitive::generate_rounded_cone<Scalar, Index>(setting);
        REQUIRE(mesh.get_num_vertices() == 0);
    }
}

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS

TEST_CASE("RoundedCone", "[primitive][rounded_cone]" LA_SLOW_DEBUG_FLAG)
{
    using namespace lagrange;

    {
        using MeshType = lagrange::TriangleMesh3D;
        using Scalar = MeshType::Scalar;
        using Index = MeshType::Index;

        auto check_dimension = [](MeshType& mesh, const Scalar radius, const Scalar height) {
            const auto& vertices = mesh.get_vertices();
            const auto x_range = vertices.col(0).maxCoeff() - vertices.col(0).minCoeff();
            const auto y_range = vertices.col(1).maxCoeff() - vertices.col(1).minCoeff();
            const auto z_range = vertices.col(2).maxCoeff() - vertices.col(2).minCoeff();
            REQUIRE(x_range <= Catch::Approx(2 * radius));
            REQUIRE(y_range == Catch::Approx(height));
            REQUIRE(z_range <= Catch::Approx(2 * radius));
        };

        SECTION("Simple cone")
        {
            Scalar r_top = 0.0, r_bottom = 2.0, h = 5.0, b_top = 0.0, b_bottom = 0.0;
            Index seg_top = 1, seg_bottom = 1;
            Index n = 1;
            SECTION("Min sections")
            {
                n = 3;
            }
            SECTION("Many sections")
            {
                n = (Index)1e2;
            }
            auto mesh = lagrange::primitive::generate_rounded_cone<MeshType>(
                r_top,
                r_bottom,
                h,
                b_top,
                b_bottom,
                n,
                seg_top,
                seg_bottom);
            primitive_test_utils::validate_primitive(*mesh);
            primitive_test_utils::check_degeneracy(*mesh);
            check_dimension(*mesh, r_bottom, h);
            primitive_test_utils::check_semantic_labels(*mesh);
            primitive_test_utils::check_UV(*mesh);
        }

        SECTION("Rounded cone: bottom")
        {
            Scalar r_top = 0.0, r_bottom = 2.0, h = 5.0, b_top = 0.0, b_bottom = 0.5;
            Index sections = 50, seg_top = 1;
            Index n = 1;
            SECTION("Single segment")
            {
                n = 1;
            }
            SECTION("Many segments")
            {
                n = (Index)1e2;
            }
            auto mesh = lagrange::primitive::generate_rounded_cone<MeshType>(
                r_top,
                r_bottom,
                h,
                b_top,
                b_bottom,
                sections,
                seg_top,
                n);
            primitive_test_utils::validate_primitive(*mesh);
            primitive_test_utils::check_degeneracy(*mesh);
            check_dimension(*mesh, r_bottom, h);
            primitive_test_utils::check_semantic_labels(*mesh);
            primitive_test_utils::check_UV(*mesh);
        }

        SECTION("Rounded cone: top and bottom")
        {
            Scalar r_top = 2.0, r_bottom = 3.0, h = 5.0, b_top = 1, b_bottom = 1;
            Index sections = 50, n_top = 1, n_bottom = 1;
            SECTION("Single segment")
            {
                n_top = 1;
                n_bottom = 1;
            }
            SECTION("Many segments")
            {
                n_top = (Index)1e2;
                n_bottom = (Index)1e2;
            }
            auto mesh = lagrange::primitive::generate_rounded_cone<MeshType>(
                r_top,
                r_bottom,
                h,
                b_top,
                b_bottom,
                sections,
                n_top,
                n_bottom);
            primitive_test_utils::validate_primitive(*mesh);
            primitive_test_utils::check_degeneracy(*mesh);
            check_dimension(*mesh, r_bottom, h);
            primitive_test_utils::check_semantic_labels(*mesh);
            primitive_test_utils::check_UV(*mesh);
        }

        SECTION("Rounded cylinder, Comparison to 2*lagrange::internal::pi and slice")
        {
            Scalar r_top = 2.0, r_bottom = 3.0, h = 5.0, b_top = 1, b_bottom = 1;
            Index sections = 50, n_top = 1, n_bottom = 1;
            Scalar begin_angle = 0.0;
            Scalar sweep_angle;
            SECTION("sweep1")
            {
                sweep_angle = 2 * lagrange::internal::pi + 2e-8;
            }
            SECTION("sweep2")
            {
                sweep_angle = 3 / 4. * lagrange::internal::pi;
            }
            auto mesh = lagrange::primitive::generate_rounded_cone<MeshType>(
                r_top,
                r_bottom,
                h,
                b_top,
                b_bottom,
                sections,
                n_top,
                n_bottom,
                begin_angle,
                sweep_angle);
            primitive_test_utils::validate_primitive(*mesh);
            primitive_test_utils::check_degeneracy(*mesh);
            check_dimension(*mesh, r_bottom, h);
            primitive_test_utils::check_semantic_labels(*mesh);
            primitive_test_utils::check_UV(*mesh);
        }

        SECTION("Simple Cone: Zero geometry")
        {
            Scalar r_top = 0.0, r_bottom = 2.0, h = 5.0, b_top = 0.0, b_bottom = 0.0;
            Index seg_top = 1, seg_bottom = 1, sections = 50;
            SECTION("r=0")
            {
                r_top = 0.0;
                r_bottom = 0.0;
            }

            SECTION("h=0")
            {
                h = 0.0;
            }

            SECTION("r=0, h=0")
            {
                r_top = 0.0;
                r_bottom = 0.0;
                h = 0.0;
            }

            auto mesh = lagrange::primitive::generate_rounded_cone<MeshType>(
                r_top,
                r_bottom,
                h,
                b_top,
                b_bottom,
                sections,
                seg_top,
                seg_bottom);
            REQUIRE(mesh->get_vertices().hasNaN() == false);
        }

        SECTION("Invalid dimension")
        {
            Scalar r_top = -2.0, r_bottom = 3.0, h = 5.0, b_top = -0.5, b_bottom = 0.5;
            Index sections = 50, seg_top = 1, seg_bottom = 1;
            auto mesh = lagrange::primitive::generate_rounded_cone<MeshType>(
                r_top,
                r_bottom,
                h,
                b_top,
                b_bottom,
                sections,
                seg_top,
                seg_bottom);
            check_dimension(*mesh, r_bottom, h);
            primitive_test_utils::check_semantic_labels(*mesh);
        }

        SECTION("Cone normal with apex")
        {
            Scalar r_top = 0.0, r_bottom = 2.5, h = 2.5, b_top = 0.0, b_bottom = 0.0;
            Index seg_top = 1, seg_bottom = 1;
            Index n = 100;
            Scalar start_sweep_angle, end_sweep_angle;
            SECTION("No slicing")
            {
                start_sweep_angle = 0;
                end_sweep_angle = 2 * lagrange::internal::pi;
            }
            SECTION("Half slicing")
            {
                start_sweep_angle = 0;
                end_sweep_angle = lagrange::internal::pi;
            }
            SECTION("30 degree slicing")
            {
                start_sweep_angle = 0;
                end_sweep_angle = lagrange::internal::pi / 6;
            }
            SECTION("330 degree slicing")
            {
                start_sweep_angle = 0;
                end_sweep_angle = lagrange::internal::pi * 11 / 6;
            }
            auto mesh = lagrange::primitive::generate_rounded_cone<MeshType>(
                r_top,
                r_bottom,
                h,
                b_top,
                b_bottom,
                n,
                seg_top,
                seg_bottom,
                start_sweep_angle,
                end_sweep_angle);

            primitive_test_utils::validate_primitive(*mesh);
            primitive_test_utils::check_degeneracy(*mesh);
            check_dimension(*mesh, r_bottom, h);
            primitive_test_utils::check_semantic_labels(*mesh);
            primitive_test_utils::check_UV(*mesh);

            REQUIRE(mesh->has_indexed_attribute("normal"));
            const auto& r = mesh->get_indexed_attribute("normal");
            const auto& normals = std::get<0>(r);
            for (auto ni : normals.rowwise()) {
                if (ni[1] < -1e-2) {
                    REQUIRE(ni[1] == Catch::Approx(-1));
                } else if (ni[1] < 1e-2) {
                    REQUIRE(ni[1] == Catch::Approx(0).margin(1e-6));
                } else {
                    REQUIRE(ni[1] == Catch::Approx(std::sqrt(2) * 0.5).margin(1e-1));
                }
            }
        }

        SECTION("Truncated cone normal")
        {
            Scalar r_top = 1.0, r_bottom = 3.5, h = 2.5, b_top = 0.0, b_bottom = 0.0;
            Index seg_top = 1, seg_bottom = 1;
            Index n = 100;
            auto mesh = lagrange::primitive::generate_rounded_cone<MeshType>(
                r_top,
                r_bottom,
                h,
                b_top,
                b_bottom,
                n,
                seg_top,
                seg_bottom);
            primitive_test_utils::validate_primitive(*mesh);
            primitive_test_utils::check_degeneracy(*mesh);
            check_dimension(*mesh, r_bottom, h);
            primitive_test_utils::check_semantic_labels(*mesh);
            primitive_test_utils::check_UV(*mesh);

            REQUIRE(mesh->has_indexed_attribute("normal"));
            const auto& r = mesh->get_indexed_attribute("normal");
            const auto& normals = std::get<0>(r);
            for (auto ni : normals.rowwise()) {
                if (ni[1] < 0) {
                    REQUIRE(ni[1] == Catch::Approx(-1));
                } else if (ni[1] > 0.8) {
                    REQUIRE(ni[1] == Catch::Approx(1));
                } else {
                    REQUIRE(ni[1] == Catch::Approx(std::sqrt(2) * 0.5).margin(1e-3));
                }
            }
        }
    }

    SECTION("Int overflow bug")
    {
        using Scalar = float;
        using Index = uint32_t;
        using VertexArray = Eigen::Matrix<Scalar, Eigen::Dynamic, 3>;
        using FacetArray = Eigen::Matrix<Index, Eigen::Dynamic, 3>;
        using MeshType = Mesh<VertexArray, FacetArray>;

        Scalar r_top = 0, r_bottom = 5, h = 10, b_top = 0.0, b_bottom = 0.0;
        Index seg_top = 1, seg_bottom = 1;
        Index n = 50;
        auto mesh = lagrange::primitive::generate_rounded_cone<MeshType>(
            r_top,
            r_bottom,
            h,
            b_top,
            b_bottom,
            n,
            seg_top,
            seg_bottom);
        primitive_test_utils::validate_primitive(*mesh);
        primitive_test_utils::check_degeneracy(*mesh);
        primitive_test_utils::check_semantic_labels(*mesh);
    }

    SECTION("Config struct")
    {
        using Scalar = float;
        using Index = uint32_t;
        using VertexArray = Eigen::Matrix<Scalar, Eigen::Dynamic, 3>;
        using FacetArray = Eigen::Matrix<Index, Eigen::Dynamic, 3>;
        using MeshType = Mesh<VertexArray, FacetArray>;

        lagrange::primitive::RoundedConeConfig config;
        config.radius_top = 0.1;
        config.output_normals = false;
        auto mesh = lagrange::primitive::generate_rounded_cone<MeshType>(config);
        primitive_test_utils::validate_primitive(*mesh);
        primitive_test_utils::check_degeneracy(*mesh);
        primitive_test_utils::check_semantic_labels(*mesh);

        REQUIRE(!mesh->has_indexed_attribute("normal"));
    }
}

#endif
