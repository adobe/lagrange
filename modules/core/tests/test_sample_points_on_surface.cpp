/*
 * Copyright 2020 Adobe. All rights reserved.
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

#include <lagrange/Mesh.h>
#include <lagrange/common.h>
#include <lagrange/create_mesh.h>
#include <lagrange/io/save_mesh.h>
#include <lagrange/sample_points_on_surface.h>

TEST_CASE("SamplePointsOnSurface", "[sample_points_on_surface][triangle_mesh]")
{
    // Set this to true, only if the results need to be visualized.
    // DON'T push to develop with true
    const bool should_dump_meshes = false;

    // Helper typedefs
    using namespace lagrange;
    using Mesh2D = Mesh<Vertices2D, Triangles>;
    using Mesh3D = Mesh<Vertices3D, Triangles>;
    using Scalar = Mesh2D::Scalar;
    using Index = Mesh2D::Index;
    using IndexList = Mesh3D::IndexList;
    using MatrixXS = Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic>;
    using MatrixXI = Eigen::Matrix<Index, Eigen::Dynamic, Eigen::Dynamic>;
    using RowVector3S = Eigen::Matrix<Mesh3D::Scalar, 1, 3>;

    // Copying create cube here, because I want to make sure where the vertices are
    // placed.
    auto create_cube = [](const Mesh3D::VertexType& dims) -> std::shared_ptr<Mesh3D> {
        Mesh3D::VertexArray vertices(8, 3);
        vertices.row(0) << -1, -1, -1;
        vertices.row(1) << 1, -1, -1;
        vertices.row(2) << 1, 1, -1;
        vertices.row(3) << -1, 1, -1;
        vertices.row(4) << -1, -1, 1;
        vertices.row(5) << 1, -1, 1;
        vertices.row(6) << 1, 1, 1;
        vertices.row(7) << -1, 1, 1;

        // Scale the mesh
        vertices.col(0) *= (dims(0) / 2.);
        vertices.col(1) *= (dims(1) / 2.);
        vertices.col(2) *= (dims(2) / 2.);


        Mesh3D::FacetArray facets(12, 3);
        facets.row(0) << 0, 2, 1;
        facets.row(1) << 0, 3, 2;
        facets.row(2) << 4, 5, 6;
        facets.row(3) << 4, 6, 7;
        facets.row(4) << 1, 2, 6;
        facets.row(5) << 1, 6, 5;
        facets.row(6) << 3, 0, 7;
        facets.row(7) << 7, 0, 4;
        facets.row(8) << 2, 3, 7;
        facets.row(9) << 2, 7, 6;
        facets.row(10) << 0, 1, 4;
        facets.row(11) << 4, 1, 5;

        return create_mesh(vertices, facets);
    };

    // Helper to check if the barycentric coordinates
    // and facet_id check out. Also optionally check sample
    // positioning to be equally spaced.
    auto verify_samples = [](const MatrixXS& V,
                             const MatrixXI& F,
                             const MatrixXS& positions,
                             const IndexList& facets,
                             const MatrixXS& barycentrics,
                             bool check_distance_from_closest_neighbour_std_dev = false) {
        REQUIRE(safe_cast<size_t>(barycentrics.rows()) == facets.size());
        REQUIRE(safe_cast<size_t>(positions.rows()) == facets.size());

        for (auto i : range(barycentrics.rows())) {
            const MatrixXS position_method0 = positions.row(i).eval();
            const Index facet_id = facets[i];
            const MatrixXS bary = barycentrics.row(i).eval();
            //
            const MatrixXS position_method1 = V.row(F(facet_id, 0)) * bary(0) +
                                              V.row(F(facet_id, 1)) * bary(1) +
                                              V.row(F(facet_id, 2)) * bary(2);
            //
            for (auto j : range(position_method0.cols())) {
                REQUIRE(position_method0(j) == Approx(position_method1(j)).margin(1e-10));
            }
        }

        // We can also check the standard deviation of the distance of each point to its
        // closest neighbour.

        // distance from closest neighbour
        if (check_distance_from_closest_neighbour_std_dev) {
            // Find the square of distance from each neighbour
            MatrixXS D2 =
                MatrixXS::Constant(positions.rows(), 1, std::numeric_limits<Scalar>::max());
            for (auto vid : range(positions.rows())) {
                for (auto nid : range(positions.rows())) {
                    const Scalar d2 = (positions.row(vid) - positions.row(nid)).squaredNorm();
                    if ((vid != nid) && d2 < D2(vid)) {
                        D2(vid) = d2;
                    } // end of if()
                } // end of neighbours
            } // end of vertices

            // Check spacing to be reasonably equispaced.
            MatrixXS D = D2.array().sqrt();
            MatrixXS Dn = D / D.mean();
            Scalar sigma = (Dn.array() - 1.).pow(2).sum() / positions.rows();
            REQUIRE(sigma < 0.1);
            // std::cout << sigma << "; " << Dn.mean() << std::endl;
        } // end of check std dev
    };


    SECTION("2d_triangles")
    {
        Mesh2D::VertexArray vertices(6, 2);
        vertices.row(0) << 0.0, 0.0;
        vertices.row(1) << 1.0, 0.0;
        vertices.row(2) << 0.0, 1.0;
        vertices.row(3) << 1.0, 1.0;
        vertices.row(4) << 2.0, 0.0;
        vertices.row(5) << 2.0, 1.0;
        Mesh2D::FacetArray facets(4, 3);
        facets.row(0) << 0, 1, 2;
        facets.row(1) << 1, 3, 2;
        facets.row(2) << 1, 4, 3;
        facets.row(3) << 3, 4, 5;
        Mesh2D::FacetArray subfacets(2, 3);
        subfacets << facets.row(1), facets.row(3);

        auto mesh = create_mesh(vertices, facets);
        auto submesh = create_mesh(vertices, subfacets);

        // Sample from the 2 triangle mesh, but only activate the second and last triangles
        auto out =
            sample_points_on_surface(*mesh, 300, std::vector<bool>{false, true, false, true});

        // Sample from the single mesh triangle
        auto subout = sample_points_on_surface(*submesh, 300);

        // The positions and the barycentric coordinates should be matching
        verify_samples(
            mesh->get_vertices(),
            mesh->get_facets(),
            out.positions,
            out.facet_ids,
            out.barycentrics,
            true);
        verify_samples(
            submesh->get_vertices(),
            submesh->get_facets(),
            subout.positions,
            subout.facet_ids,
            subout.barycentrics,
            false);

        // Sampling should give the same result
        auto diff = (subout.positions - out.positions).eval();
        REQUIRE(diff.norm() < 1e-10);

        if (should_dump_meshes) {
            io::save_mesh("test_sample_points_on_mesh_uniformly__mesh_0.vtk", *mesh);
            io::save_mesh(
                "test_sample_points_on_mesh_uniformly__points_0.vtk",
                *create_mesh(out.positions, Triangles(0, 3)));
        }
    }
    SECTION("3d_triangles")
    {
        Mesh3D::VertexArray vertices(6, 3);
        vertices.row(0) << 0.0, 0.0, 1.0;
        vertices.row(1) << 1.0, 0.0, 0.0;
        vertices.row(2) << 0.0, 1.0, 0.5;
        vertices.row(3) << 1.0, 1.0, -2.0;
        vertices.row(4) << 2.0, 0.0, 0.1;
        vertices.row(5) << 2.0, 1.0, -0.8;
        Mesh3D::FacetArray facets(4, 3);
        facets.row(0) << 0, 1, 2;
        facets.row(1) << 1, 3, 2;
        facets.row(2) << 1, 4, 3;
        facets.row(3) << 3, 4, 5;
        Mesh3D::FacetArray subfacets(2, 3);
        subfacets << facets.row(1), facets.row(3);

        auto mesh = create_mesh(vertices, facets);
        auto submesh = create_mesh(vertices, subfacets);

        // Sample from the 2 triangle mesh, but only activate the second and last triangles
        auto out =
            sample_points_on_surface(*mesh, 300, std::vector<bool>{false, true, false, true});

        // Sample from the single mesh triangle
        auto subout = sample_points_on_surface(*submesh, 300);

        // The positions and the barycentric coordinates should be matching
        verify_samples(
            mesh->get_vertices(),
            mesh->get_facets(),
            out.positions,
            out.facet_ids,
            out.barycentrics,
            true);
        verify_samples(
            submesh->get_vertices(),
            submesh->get_facets(),
            subout.positions,
            subout.facet_ids,
            subout.barycentrics,
            false);

        // Sampling should give the same result
        auto diff = (subout.positions - out.positions).eval();
        REQUIRE(diff.norm() < 1e-10);

        if (should_dump_meshes) {
            io::save_mesh("test_sample_points_on_mesh_uniformly__mesh_1.vtk", *mesh);
            io::save_mesh(
                "test_sample_points_on_mesh_uniformly__points_1.vtk",
                *create_mesh(out.positions, Triangles(0, 3)));
        }
    }

    // Sample points on a cube, and check if the covariance matrix
    // match that of the surface.
    SECTION("covariance_matrix")
    {
        // Sample 1000 points
        const Index approx_num_points = 2000;
        RowVector3S dims(0.5, 2, 3);
        auto mesh = create_cube(dims);
        auto out = sample_points_on_surface(*mesh, approx_num_points);
        verify_samples(
            mesh->get_vertices(),
            mesh->get_facets(),
            out.positions,
            out.facet_ids,
            out.barycentrics,
            false);

        // Now compute area per sample
        const Scalar mesh_area =
            2 * dims(0) * dims(1) + 2 * dims(1) * dims(2) + 2 * dims(0) * dims(2);
        const Scalar per_point_area = mesh_area / out.num_samples;

        // Expected moments of area
        // Note that it is a shell, not a filled solid!
        const Scalar a = dims(0);
        const Scalar b = dims(1);
        const Scalar c = dims(2);
        Scalar Ix_expected = 0;
        Scalar Iy_expected = 0;
        Scalar Iz_expected = 0;
        Scalar Ixx_expected = 0.5 * (a * a * b * c) + 1 / 6. * (a * a * a) * (b + c);
        Scalar Iyy_expected = 0.5 * (b * b * a * c) + 1 / 6. * (b * b * b) * (a + c);
        Scalar Izz_expected = 0.5 * (c * c * b * a) + 1 / 6. * (c * c * c) * (b + a);
        Scalar Ixy_expected = 0;
        Scalar Ixz_expected = 0;
        Scalar Iyz_expected = 0;

        // Now compute the moments of area based on samples
        // http://www.kwon3d.com/theory/moi/iten.html
        Scalar Ix_computed = 0;
        Scalar Iy_computed = 0;
        Scalar Iz_computed = 0;
        Scalar Ixx_computed = 0;
        Scalar Iyy_computed = 0;
        Scalar Izz_computed = 0;
        Scalar Ixy_computed = 0;
        Scalar Ixz_computed = 0;
        Scalar Iyz_computed = 0;
        for (auto i : range(out.num_samples)) {
            const MatrixXS pos = out.positions.row(i);
            Ix_computed += per_point_area * pos(0);
            Iy_computed += per_point_area * pos(1);
            Iz_computed += per_point_area * pos(2);
            Ixx_computed += per_point_area * pos(0) * pos(0);
            Iyy_computed += per_point_area * pos(1) * pos(1);
            Izz_computed += per_point_area * pos(2) * pos(2);
            Ixy_computed += per_point_area * pos(0) * pos(1);
            Ixz_computed += per_point_area * pos(0) * pos(2);
            Iyz_computed += per_point_area * pos(1) * pos(2);
        }

        // Make sure they are the same
        const double eps = 0.5;
        SECTION("Ix") { REQUIRE(Ix_expected == Approx(Ix_computed).margin(eps)); }
        SECTION("Iy") { REQUIRE(Iy_expected == Approx(Iy_computed).margin(eps)); }
        SECTION("Iz") { REQUIRE(Iz_expected == Approx(Iz_computed).margin(eps)); }
        SECTION("Ixx") { REQUIRE(Ixx_expected == Approx(Ixx_computed).margin(eps)); }
        SECTION("Iyy") { REQUIRE(Iyy_expected == Approx(Iyy_computed).margin(eps)); }
        SECTION("Izz") { REQUIRE(Izz_expected == Approx(Izz_computed).margin(eps)); }
        SECTION("Ixy") { REQUIRE(Ixy_expected == Approx(Ixy_computed).margin(eps)); }
        SECTION("Ixz") { REQUIRE(Ixz_expected == Approx(Ixz_computed).margin(eps)); }
        SECTION("Iyz") { REQUIRE(Iyz_expected == Approx(Iyz_computed).margin(eps)); }

        // Save the mesh if rquired
        if (should_dump_meshes) {
            io::save_mesh("test_sample_points_on_mesh_uniformly__mesh_4.vtk", *mesh);
            io::save_mesh(
                "test_sample_points_on_mesh_uniformly__points_4.vtk",
                *create_mesh(out.positions, Triangles(0, 3)));
        }
    }

    // Final todo: check agains SampleViaGrid
}
