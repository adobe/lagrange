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
#include <lagrange/testing/common.h>
#include <catch2/catch_approx.hpp>

#include <lagrange/Attribute.h>
#include <lagrange/compute_facet_normal.h>
#include <lagrange/internal/find_attribute_utils.h>
#include <lagrange/io/save_mesh.h>
#include <lagrange/select_facets_by_normal_similarity.h>
#include <lagrange/views.h>

namespace {

//
// Compare the selection with some predefined expectations on a cylinder.
//
template <typename Scalar>
void run()
{
    // =========================
    // Set this to true, only if the results need to be visualized.
    // =========================
    // const bool should_dump_meshes = false;


    // =========================
    // Helper typedefs
    // =========================
    using namespace lagrange;

    using Index = uint32_t;
    using MeshType = SurfaceMesh<Scalar, Index>;
    using VertexType = typename Eigen::Vector3<Scalar>;

    // =========================
    // Helper lambdas
    // =========================

    // return number of true indices in a bit vector
    auto get_bitvector_num_true = [](const lagrange::span<uint8_t> is_selected) -> Index {
        Index ans = 0;
        for (Index i = 0; i < static_cast<Index>(is_selected.size()); ++i) {
            if (is_selected[i]) {
                ++ans;
            }
        }
        return ans;
    };


    // Get midpoint of a facet
    auto get_facet_midpoint = [](const MeshType& mesh, const Index facet_id) -> VertexType {
        Index nvertices = mesh.get_facet_size(facet_id);
        VertexType midpoint = VertexType::Zero();
        for (Index i = 0; i < nvertices; i++) {
            const auto v = mesh.get_position(mesh.get_facet_vertex(facet_id, i));
            midpoint += VertexType(v.data());
        }
        return static_cast<Scalar>(1.0 / double(nvertices)) * midpoint;
    };

    // generate a cylinder -- copied from test_compute_curvature
    auto generate_cylinder = [](const Scalar radius,
                                const Scalar height,
                                const Index n_radial_segments,
                                const Index n_vertical_segments) -> MeshType {
        // 2 vertices for closing the top and bottom
        const Index n_vertices = n_radial_segments * (n_vertical_segments + 1) + 2;
        MeshType mesh;
        mesh.add_vertices(n_vertices);

        auto vertices = vertex_ref(mesh); // returns an Eigen::Map

        for (const auto h : range(n_vertical_segments + 1)) {
            for (const auto r : range(n_radial_segments)) {
                const double angle = 2 * M_PI * r / n_radial_segments;
                vertices.row(h * n_radial_segments + r) << radius * cos(angle), radius * sin(angle),
                    height * h / n_vertical_segments;
            }
        }
        vertices.row(n_vertices - 2) << 0, 0, 0;
        vertices.row(n_vertices - 1) << 0, 0, height;

        // set the triangles on the sides
        for (const auto h : range(n_vertical_segments)) {
            for (const auto r : range(n_radial_segments)) {
                const Index i0 = h * n_radial_segments + r;
                const Index i1 = h * n_radial_segments + (r + 1) % n_radial_segments;
                const Index j0 = (h + 1) * n_radial_segments + r;
                const Index j1 = (h + 1) * n_radial_segments + (r + 1) % n_radial_segments;
                // Alternate the diagonal edges
                if (r % 2 == 0) {
                    mesh.add_triangle(i0, i1, j1);
                    mesh.add_triangle(i0, j1, j0);
                } else {
                    mesh.add_triangle(i0, i1, j0);
                    mesh.add_triangle(j0, i1, j1);
                }
            }
        }

        // triangles on the top
        for (const auto r : range(n_radial_segments)) {
            const Index center = n_vertices - 2;
            const Index i0 = r;
            const Index i1 = (r + 1) % n_radial_segments;
            mesh.add_triangle(center, i0, i1);
        }

        // triangles on the bottom
        for (const auto r : range(n_radial_segments)) {
            const Index center = n_vertices - 1;
            const Index i0 = n_vertical_segments * n_radial_segments + r;
            const Index i1 = n_vertical_segments * n_radial_segments + (r + 1) % n_radial_segments;
            mesh.add_triangle(center, i0, i1);
        }

        return mesh;
    };

    // =========================
    // A simple case, the cylinder mesh
    // =========================
    SECTION("cylinder")
    {
        // Properties of the cylinder
        const Index n_radial_segments = 25;
        const Index n_vertical_segments = 15;
        const Scalar height = 1.5;
        const Scalar radius = 2.;

        const Index n_side_facets = 2 * n_radial_segments * n_vertical_segments;
        const Index face_on_bottom_id = n_side_facets + 1;
        const Index face_on_side_id = 1;

        const Index n_vertices = n_radial_segments * (n_vertical_segments + 1) + 2;
        const Index vertex_on_bottom_mid_id = n_vertices - 2;

        auto mesh = generate_cylinder(radius, height, n_radial_segments, n_vertical_segments);

        SelectFacetsByNormalSimilarityOptions options;
        options.flood_error_limit = 0.1;
        SECTION("select-on-bottom")
        {
            SelectFacetsByNormalSimilarityOptions options_bottom = options;
            // Set the seed and run the selection
            // but prevent one face to be selected
            const Index seed_id = face_on_bottom_id;
            options_bottom.num_smooth_iterations = 0;
            options_bottom.is_facet_selectable_attribute_name = "@is_facet_selectable";

            const Index dont_select_this = face_on_bottom_id + 1;
            AttributeId selectability_id = internal::find_or_create_attribute<uint8_t>(
                mesh,
                options_bottom.is_facet_selectable_attribute_name.value(),
                Facet,
                AttributeUsage::Scalar,
                /* number of channels*/ 1,
                internal::ResetToDefault::Yes);
            lagrange::span<uint8_t> attr_ref =
                mesh.template ref_attribute<uint8_t>(selectability_id).ref_all();
            std::fill(attr_ref.begin(), attr_ref.end(), 1);
            attr_ref[dont_select_this] = 0;


            lagrange::select_facets_by_normal_similarity(mesh, seed_id, options_bottom);

            // Make sure only and only the bottom is selected
            AttributeId selected_id = internal::find_attribute<uint8_t>(
                mesh,
                options_bottom.output_attribute_name,
                Facet,
                AttributeUsage::Scalar,
                /* num channels */ 1);
            const lagrange::span<uint8_t> is_facet_selected =
                mesh.template ref_attribute<uint8_t>(selected_id).ref_all();
            for (const auto facet_id : range(mesh.get_num_facets())) {
                const auto midpoint = get_facet_midpoint(mesh, facet_id);
                if (facet_id == dont_select_this) {
                    REQUIRE(is_facet_selected[facet_id] == 0);
                } else if (midpoint.z() == Catch::Approx(0.)) {
                    REQUIRE(is_facet_selected[facet_id] != 0);
                } else {
                    REQUIRE(is_facet_selected[facet_id] == 0);
                }
            } // end of testing

        } // end of select on bottom
        SECTION("select-on-side")
        {
            SelectFacetsByNormalSimilarityOptions options_side = options;
            // Set the seed and run the selection
            const Index seed_id = face_on_side_id;
            options_side.num_smooth_iterations = 0;

            lagrange::select_facets_by_normal_similarity(mesh, seed_id, options_side);

            AttributeId selected_id = internal::find_attribute<uint8_t>(
                mesh,
                options_side.output_attribute_name,
                Facet,
                AttributeUsage::Scalar,
                /* num channels */ 1);
            const lagrange::span<uint8_t> is_facet_selected =
                mesh.template ref_attribute<uint8_t>(selected_id).ref_all();

            // Make sure only the faces on the row of the seed
            // and the adjacent rows are selected
            for (const auto facet_id : range(mesh.get_num_facets())) {
                const auto facet_midpoint = get_facet_midpoint(mesh, facet_id);
                const double dtheta = 2 * M_PI / n_radial_segments;
                const Scalar y_min_lim = -sin(dtheta) * radius;
                const Scalar y_max_lim = sin(2 * dtheta) * radius;
                const Scalar x_min_lim = 0;
                const bool is_face_in_the_right_region = (facet_midpoint.y() < y_max_lim) &&
                                                         (facet_midpoint.y() > y_min_lim) &&
                                                         (facet_midpoint.x() > x_min_lim);

                if (facet_midpoint.z() == Catch::Approx(0.)) { // don't select bottom
                    REQUIRE(is_facet_selected[facet_id] == 0);
                } else if (facet_midpoint.z() == Catch::Approx(height)) { // don't select top
                    REQUIRE(is_facet_selected[facet_id] == 0);
                } else if (is_face_in_the_right_region) { // only select right part of the side
                    REQUIRE(is_facet_selected[facet_id] != 0);
                } else { // don't select other parts
                    REQUIRE(is_facet_selected[facet_id] == 0);
                }
            } // end of testing

        } // end of select on side
        SECTION("smooth-selection-boundary")
        {
            SelectFacetsByNormalSimilarityOptions options_nosmoothbdry = options;
            SelectFacetsByNormalSimilarityOptions options_smoothbdry = options;
            // Move the bottom vertex of the mesh, so that some of the bottom triangles
            // also get selected. I push  it towards the seed.
            {
                auto p = mesh.ref_position(vertex_on_bottom_mid_id);
                p[0] = 1.9f;
                p[1] = 0.3f;
                p[2] = -.3f;
                FacetNormalOptions fn_options;
                fn_options.output_attribute_name = options_smoothbdry.facet_normal_attribute_name;
                compute_facet_normal(mesh, fn_options);
            }

            // Set the seed and run the selection
            // once with and once without smoothing the selection boundary
            const Index seed_id = face_on_side_id;

            options_nosmoothbdry.num_smooth_iterations = 0;
            options_nosmoothbdry.output_attribute_name = "@is_selected_nosmoothbdry";
            lagrange::select_facets_by_normal_similarity(mesh, seed_id, options_nosmoothbdry);

            AttributeId selected_id_nosmoothbdry = internal::find_attribute<uint8_t>(
                mesh,
                options_nosmoothbdry.output_attribute_name,
                Facet,
                AttributeUsage::Scalar,
                /* num channels */ 1);
            const lagrange::span<uint8_t> is_facet_selected_nosmoothbdry =
                mesh.template ref_attribute<uint8_t>(selected_id_nosmoothbdry).ref_all();

            options_smoothbdry.num_smooth_iterations = 3;
            options_smoothbdry.output_attribute_name = "@is_selected_smoothbdry";
            lagrange::select_facets_by_normal_similarity(mesh, seed_id, options_smoothbdry);

            AttributeId selected_id_smoothbdry = internal::find_attribute<uint8_t>(
                mesh,
                options_smoothbdry.output_attribute_name,
                Facet,
                AttributeUsage::Scalar,
                /* num channels */ 1);
            const lagrange::span<uint8_t> is_facet_selected_smoothbdry =
                mesh.template ref_attribute<uint8_t>(selected_id_smoothbdry).ref_all();


            // Smoothing should allow one extra triangle to appear
            const Index n_selected_nosmoothbdry =
                get_bitvector_num_true(is_facet_selected_nosmoothbdry);
            const Index n_selected_smoothbdry =
                get_bitvector_num_true(is_facet_selected_smoothbdry);
            REQUIRE(n_selected_nosmoothbdry + 1 == n_selected_smoothbdry);

        } // end of smooth-boundary
    } // end of cylinder
} // end of run
} // end of namespace

TEST_CASE("select_facets_by_normal_similarity<float>", "[select_facets_by_normal_similarity]")
{
    run<float>();
}
TEST_CASE("select_facets_by_normal_similarity<double>", "[select_facets_by_normal_similarity]")
{
    run<double>();
}
