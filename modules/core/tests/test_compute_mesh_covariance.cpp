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


#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS

    #include <lagrange/common.h>
    #include <lagrange/create_mesh.h>
    #include <lagrange/legacy/compute_mesh_covariance.h>
    #include <lagrange/testing/common.h>
    #include <lagrange/utils/fmt_eigen.h> // debugging
    #include <lagrange/utils/safe_cast.h>
    #include <lagrange/views.h> // debugging

    #include <Eigen/Geometry>
    #include <catch2/catch_approx.hpp>

    #include <cmath>


TEST_CASE("legacy::compute_mesh_covariance", "[mesh][covariance]")
{
    using namespace lagrange;
    using namespace lagrange::legacy;

    Vertices3D ref_vertices(6 + 3, 3);
    Triangles facets(4 + 1, 3);

    const double a = 0.5;
    const double b = 2.0;
    const double large_number = 1000;

    ref_vertices.row(0) << -a / 2., -b / 2., 0;
    ref_vertices.row(1) << +a / 2., -b / 2., 0;
    ref_vertices.row(2) << +a / 2., 0, 0;
    ref_vertices.row(3) << +a / 2., b / 4., 0;
    ref_vertices.row(4) << +a / 2., b / 2., 0;
    ref_vertices.row(5) << -a / 2., b / 2., 0;
    // Don't include in computation
    ref_vertices.row(6) << large_number, large_number, large_number;
    ref_vertices.row(7) << -large_number, -large_number, -large_number;
    ref_vertices.row(8) << 2 * large_number, 2 * large_number, 2 * large_number;


    facets.row(0) << 0, 1, 2;
    // Don't include in computation
    facets.row(1) << 6, 7, 8;
    //
    facets.row(2) << 0, 2, 3;
    facets.row(3) << 0, 3, 4;
    facets.row(4) << 0, 4, 5;

    // Reference values without transformations
    const double ref_area = a * b;
    Eigen::Vector3d ref_center(0, 0, 0);
    Eigen::Matrix3d ref_covariance = Eigen::Matrix3d::Zero();
    ref_covariance(0, 0) = b * a * a * a / 12.;
    ref_covariance(1, 1) = a * b * b * b / 12.;

    // Translate
    // Eigen::Vector3d tr(0,0,0);
    Eigen::Vector3d tr(-1, 3, 4);

    // Rotate
    // Eigen::Matrix3d rot = Eigen::Matrix3d::Identity();
    Eigen::Matrix3d rot =
        Eigen::AngleAxisd(1.2365, Eigen::Vector3d(-1, 2, 5.1).normalized()).toRotationMatrix();
    Eigen::Matrix3d rot_covariance = rot * ref_covariance * rot.transpose();
    Eigen::Matrix3d rot_covariance_tr = rot_covariance + ref_area * tr * tr.transpose();

    // Create the vertices
    Vertices3D vertices = (ref_vertices * rot.transpose()).rowwise() + tr.transpose();

    auto mesh_unique = lagrange::create_mesh(vertices, facets);
    auto out_covariance_at_zero =
        compute_mesh_covariance(*mesh_unique, Eigen::RowVector3d::Zero(), {0, 2, 3, 4});
    auto out_covariance_at_centroid =
        compute_mesh_covariance(*mesh_unique, tr.transpose(), {0, 2, 3, 4});

    CHECK((out_covariance_at_zero - rot_covariance_tr).norm() == Catch::Approx(0.).margin(1e-10));
    CHECK((out_covariance_at_centroid - rot_covariance).norm() == Catch::Approx(0.).margin(1e-10));

} // end of TEST
#endif

#include <lagrange/compute_mesh_covariance.h>
#include <lagrange/transform_mesh.h>

#include <Eigen/Core>
#include <Eigen/Geometry>

TEST_CASE("compute_mesh_covariance", "[mesh][covariance]")
{
    using namespace lagrange;

    const double a = 0.5;
    const double b = 2.0;
    const double large_number = 1000;

    SurfaceMesh<double, uint32_t> mesh;
    mesh.add_vertex({-a / 2., -b / 2., 0});
    mesh.add_vertex({+a / 2., -b / 2., 0});
    mesh.add_vertex({+a / 2., 0, 0});
    mesh.add_vertex({+a / 2., b / 4., 0});
    mesh.add_vertex({+a / 2., b / 2., 0});
    mesh.add_vertex({-a / 2., b / 2., 0});
    mesh.add_vertex({large_number, large_number, large_number});
    mesh.add_vertex({-large_number, -large_number, -large_number});
    mesh.add_vertex({2 * large_number, 2 * large_number, 2 * large_number});

    lagrange::logger().debug("Mesh vertex_view: {}", vertex_view(mesh));

    mesh.add_triangle(0, 1, 2);
    mesh.add_triangle(6, 7, 8); // this face is subsequently excluded using active_facets
    mesh.add_triangle(0, 2, 3);
    mesh.add_triangle(0, 3, 4);
    mesh.add_triangle(0, 4, 5);

    // Reference values without transformations
    const double ref_area = a * b;
    Eigen::Vector3d ref_center(0, 0, 0);
    Eigen::Matrix3d ref_covariance = Eigen::Matrix3d::Zero();
    ref_covariance(0, 0) = b * a * a * a / 12.;
    ref_covariance(1, 1) = a * b * b * b / 12.;

    // Translate
    Eigen::Vector3d tr(-1, 3, 4);

    // Rotate
    Eigen::Matrix3d rot =
        Eigen::AngleAxisd(1.2365, Eigen::Vector3d(-1, 2, 5.1).normalized()).toRotationMatrix();
    Eigen::Matrix3d rot_covariance = rot * ref_covariance * rot.transpose();
    Eigen::Matrix3d rot_covariance_tr = rot_covariance + ref_area * tr * tr.transpose();

    Eigen::Transform<double, 3, Eigen::Affine> transform;
    transform.matrix().block<3, 3>(0, 0) = rot;
    transform.matrix().block<3, 1>(0, 3) = tr;

    transform_mesh(mesh, transform);

    std::vector<uint8_t> active_facets({1, 0, 1, 1, 1});
    mesh.wrap_as_attribute<uint8_t>(
        "@active",
        AttributeElement::Facet,
        AttributeUsage::Vector,
        /* num_channels */ 1,
        active_facets);


    // covariance around zero
    MeshCovarianceOptions options_zero;
    options_zero.active_facets_attribute_name = "@active";
    options_zero.center = {0, 0, 0};
    std::array<std::array<double, 3>, 3> out_covariance_at_zero =
        compute_mesh_covariance(mesh, options_zero);

    // covariance around centroid
    MeshCovarianceOptions options_centroid;
    options_centroid.active_facets_attribute_name = "@active";
    std::copy(tr.data(), tr.data() + 3, options_centroid.center.begin());
    std::array<std::array<double, 3>, 3> out_covariance_at_centroid =
        compute_mesh_covariance(mesh, options_centroid);

    Eigen::Matrix3d cov_zero_eigen(reinterpret_cast<double*>(out_covariance_at_zero.data()));
    Eigen::Matrix3d cov_centroid_eigen(
        reinterpret_cast<double*>(out_covariance_at_centroid.data()));

    CHECK((cov_zero_eigen - rot_covariance_tr).norm() == Catch::Approx(0.).margin(1e-10));
    CHECK((cov_centroid_eigen - rot_covariance).norm() == Catch::Approx(0.).margin(1e-10));


} // end of TEST
