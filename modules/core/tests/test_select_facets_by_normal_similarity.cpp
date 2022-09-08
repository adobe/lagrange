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

#include <lagrange/io/save_mesh.h>
#include <lagrange/select_facets_by_normal_similarity.h>


//
// Compare the selection with some predefined expectations on a cylinder.
//
TEST_CASE("select_facets_by_normal_similarity", "[select_facets_by_normal_similarity]")
{
    // =========================
    // Set this to true, only if the results need to be visualized.
    // =========================
    const bool should_dump_meshes = false;


    // =========================
    // Helper typedefs
    // =========================
    using namespace lagrange;

    using MeshType = TriangleMesh3D;
    using Parameters = SelectFacetsByNormalSimilarityParameters<MeshType>;
    using Index = MeshType::Index;
    using AttributeArray = MeshType::AttributeArray;
    using Scalar = MeshType::Scalar;
    using VertexType = MeshType::VertexType;

    // =========================
    // Helper lambdas
    // =========================

    // return number of true indices in a bit vector
    auto get_bitvector_num_true = [](const std::vector<bool>& is_selected) -> Index {
        Index ans = 0;
        for (Index i = 0; i < static_cast<Index>(is_selected.size()); ++i) {
            if (is_selected[i]) {
                ++ans;
            }
        }
        return ans;
    };

    // convert bitvector to attribute array
    auto get_bitvector_as_attribute = [](const std::vector<bool>& is_selected) -> AttributeArray {
        AttributeArray ans(is_selected.size(), 1);
        for (Index i = 0; i < static_cast<Index>(is_selected.size()); ++i) {
            ans(i) = static_cast<Scalar>(is_selected[i]);
        }
        return ans;
    };

    // Get midpoint of a facet
    auto get_facet_midpoint = [](std::shared_ptr<MeshType> mesh,
                                 const Index facet_id) -> VertexType {
        const auto face_vertex_ids = mesh->get_facets().row(facet_id).eval();
        const auto v0 = mesh->get_vertices().row(face_vertex_ids(0)).eval();
        const auto v1 = mesh->get_vertices().row(face_vertex_ids(1)).eval();
        const auto v2 = mesh->get_vertices().row(face_vertex_ids(2)).eval();
        const auto midpoint = ((v0 + v1 + v2) / 3.).eval();
        return midpoint;
    };

    // generate a cylinder -- copied from test_compute_curvature
    auto generate_cylinder = [](const Scalar radius,
                                const Scalar height,
                                const Index n_radial_segments,
                                const Index n_vertical_segments) -> std::shared_ptr<MeshType> {
        // 2 vertices for closing the top and bottom
        const Index n_vertices = n_radial_segments * (n_vertical_segments + 1) + 2;
        Vertices3D vertices(n_vertices, 3);

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
        const Index n_top_facets = n_radial_segments;
        const Index n_bottom_facets = n_radial_segments;
        const Index n_side_facets = 2 * n_radial_segments * n_vertical_segments;
        const Index n_facets = n_top_facets + n_bottom_facets + n_side_facets;

        Triangles facets(n_facets, 3);
        for (const auto h : range(n_vertical_segments)) {
            for (const auto r : range(n_radial_segments)) {
                const Index i0 = h * n_radial_segments + r;
                const Index i1 = h * n_radial_segments + (r + 1) % n_radial_segments;
                const Index j0 = (h + 1) * n_radial_segments + r;
                const Index j1 = (h + 1) * n_radial_segments + (r + 1) % n_radial_segments;
                // Alternate the diagonal edges
                if (r % 2 == 0) {
                    facets.row(2 * h * n_radial_segments + 2 * r) << i0, i1, j1;
                    facets.row(2 * h * n_radial_segments + 2 * r + 1) << i0, j1, j0;
                } else {
                    facets.row(2 * h * n_radial_segments + 2 * r) << i0, i1, j0;
                    facets.row(2 * h * n_radial_segments + 2 * r + 1) << j0, i1, j1;
                }
            }
        }

        // triangles on the top
        for (const auto r : range(n_radial_segments)) {
            const Index center = n_vertices - 2;
            const Index i0 = r;
            const Index i1 = (r + 1) % n_radial_segments;
            facets.row(n_side_facets + r) << center, i0, i1;
        }

        // triangles on the bottom
        for (const auto r : range(n_radial_segments)) {
            const Index center = n_vertices - 1;
            const Index i0 = n_vertical_segments * n_radial_segments + r;
            const Index i1 = n_vertical_segments * n_radial_segments + (r + 1) % n_radial_segments;
            facets.row(n_side_facets + n_top_facets + r) << center, i0, i1;
        }

        return create_mesh(vertices, facets);
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
        //
        const Index n_side_facets = 2 * n_radial_segments * n_vertical_segments;
        const Index face_on_bottom_id = n_side_facets + 1;
        const Index face_on_side_id = 1;
        //
        const Index n_vertices = n_radial_segments * (n_vertical_segments + 1) + 2;
        const Index vertex_on_bottom_mid_id = n_vertices - 2;
        //
        auto mesh = generate_cylinder(radius, height, n_radial_segments, n_vertical_segments);

        Parameters parameters;
        parameters.flood_error_limit = 0.1;

        SECTION("select-on-bottom")
        {
            // Set the seed and run the selection
            // but prevent one face to be selected
            const Index seed_id = face_on_bottom_id;
            parameters.should_smooth_boundary = false;
            //
            const Index dont_select_this = face_on_bottom_id + 1;
            // parameters.is_facet_selectable.resize(mesh->get_num_facets(), true);
            // parameters.is_facet_selectable[dont_select_this] = false;
            std::vector<Index> selectables;
            for (auto i : range(mesh->get_num_facets())) {
                if (i != dont_select_this) {
                    selectables.push_back(i);
                }
            }
            parameters.set_selectable_facets(*mesh, selectables);

            auto is_facet_selected =
                lagrange::select_facets_by_normal_similarity(*mesh, seed_id, parameters);

            // Make sure only and only the bottom is selected
            for (const auto facet_id : range(mesh->get_num_facets())) {
                const auto midpoint = get_facet_midpoint(mesh, facet_id);
                if (facet_id == dont_select_this) {
                    REQUIRE(is_facet_selected[facet_id] == false);
                } else if (midpoint.z() == Catch::Approx(0.)) {
                    REQUIRE(is_facet_selected[facet_id] == true);
                } else {
                    REQUIRE(is_facet_selected[facet_id] == false);
                }
            } // end of testing

            // Dump the mesh if needed
            if (should_dump_meshes) {
                if (!mesh->has_facet_attribute("is_selected")) {
                    mesh->add_facet_attribute("is_selected");
                }
                auto attrib = get_bitvector_as_attribute(is_facet_selected);
                mesh->import_facet_attribute("is_selected", attrib);
                io::save_mesh("test_select_facets_by_normal_similarity__cylinder_0.vtk", *mesh);
                io::save_mesh("test_select_facets_by_normal_similarity__cylinder_0.obj", *mesh);
            }
        } // end of select on bottom
        SECTION("select-on-side")
        {
            // Set the seed and run the selection
            const Index seed_id = face_on_side_id;
            parameters.should_smooth_boundary = false;
            //
            std::vector<bool> is_facet_selected =
                lagrange::select_facets_by_normal_similarity(*mesh, seed_id, parameters);

            // Make sure only the faces on the row of the seed
            // and the adjacent rows are selected
            for (const auto facet_id : range(mesh->get_num_facets())) {
                const auto facet_midpoint = get_facet_midpoint(mesh, facet_id);
                const double dtheta = 2 * M_PI / n_radial_segments;
                const Scalar y_min_lim = -sin(dtheta) * radius;
                const Scalar y_max_lim = sin(2 * dtheta) * radius;
                const Scalar x_min_lim = 0;
                const bool is_face_in_the_right_region = (facet_midpoint.y() < y_max_lim) &&
                                                         (facet_midpoint.y() > y_min_lim) &&
                                                         (facet_midpoint.x() > x_min_lim);

                if (facet_midpoint.z() == Catch::Approx(0.)) { // don't select bottom
                    REQUIRE(is_facet_selected[facet_id] == false);
                } else if (facet_midpoint.z() == Catch::Approx(height)) { // don't select top
                    REQUIRE(is_facet_selected[facet_id] == false);
                } else if (is_face_in_the_right_region) { // only select right part of the side
                    // std::cout << facet_id << ";" << facet_midpoint.y() << "; " << y_min_lim <<
                    // ";" << y_max_lim << std::endl;
                    REQUIRE(is_facet_selected[facet_id] == true);
                } else { // don't select other parts
                    REQUIRE(is_facet_selected[facet_id] == false);
                }
            } // end of testing

            if (should_dump_meshes) {
                if (!mesh->has_facet_attribute("is_selected")) {
                    mesh->add_facet_attribute("is_selected");
                }
                auto attrib = get_bitvector_as_attribute(is_facet_selected);
                mesh->import_facet_attribute("is_selected", attrib);
                io::save_mesh("test_select_facets_by_normal_similarity__cylinder_1.vtk", *mesh);
                io::save_mesh("test_select_facets_by_normal_similarity__cylinder_1.obj", *mesh);
            }
        } // end of select on side
        SECTION("smooth-selection-boundary")
        {
            // Move the bottom vertex of the mesh, so that some of the bottom triangles
            // also get selected. I push  it towards the seed.
            {
                auto vertices = mesh->get_vertices();
                vertices.row(vertex_on_bottom_mid_id) << 1.9, 0.3, -.3;
                mesh->initialize(vertices, mesh->get_facets());
            }

            // Set the seed and run the selection
            // once with and once without smoothing the selection boundary
            const Index seed_id = face_on_side_id;
            //
            parameters.should_smooth_boundary = false;
            std::vector<bool> is_facet_selected_no_smooth =
                lagrange::select_facets_by_normal_similarity(*mesh, seed_id, parameters);
            //
            parameters.should_smooth_boundary = true;
            std::vector<bool> is_facet_selected_with_smooth =
                lagrange::select_facets_by_normal_similarity(*mesh, seed_id, parameters);


            // Smoothing should allow one extra triangle to appear
            const Index n_selected_no_smooth = get_bitvector_num_true(is_facet_selected_no_smooth);
            const Index n_selected_with_smooth =
                get_bitvector_num_true(is_facet_selected_with_smooth);
            REQUIRE(n_selected_no_smooth + 1 == n_selected_with_smooth);

            if (should_dump_meshes) {
                if (!mesh->has_facet_attribute("is_selected_no_smooth")) {
                    mesh->add_facet_attribute("is_selected_no_smooth");
                }
                auto attrib = get_bitvector_as_attribute(is_facet_selected_no_smooth);
                mesh->import_facet_attribute("is_selected_no_smooth", attrib);
                //
                if (!mesh->has_facet_attribute("is_selected_with_smooth")) {
                    mesh->add_facet_attribute("is_selected_with_smooth");
                }
                attrib = get_bitvector_as_attribute(is_facet_selected_with_smooth);
                mesh->import_facet_attribute("is_selected_with_smooth", attrib);
                //
                io::save_mesh("test_select_facets_by_normal_similarity__cylinder_2.vtk", *mesh);
                io::save_mesh("test_select_facets_by_normal_similarity__cylinder_2.obj", *mesh);
            }
        } // end of smooth-boundary
    } // end of cuboid

} // end of the test section
