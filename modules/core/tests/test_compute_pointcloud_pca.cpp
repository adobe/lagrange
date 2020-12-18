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
#include <cmath>
#include <iostream>

#include <lagrange/testing/common.h>

#include <Eigen/Geometry>

#include <lagrange/common.h>
#include <lagrange/compute_pointcloud_pca.h>
#include <lagrange/utils/safe_cast.h>

TEST_CASE("ComputePointcloudPCA", "[compute_pointcloud_pca][symmetry]")
{
    using namespace lagrange;


    using MatrixX3dRowMajor = Eigen::Matrix<double, Eigen::Dynamic, 3, Eigen::RowMajor>;
    using MatrixX3dColMajor = Eigen::Matrix<double, Eigen::Dynamic, 3, Eigen::ColMajor>;
    const double eps = 1e-10;

    // An arbitrary rotation
    const Eigen::Matrix3d rotation =
        Eigen::AngleAxisd(M_PI * 0.2657, Eigen::Vector3d(-1, 4, -7).normalized())
            .toRotationMatrix();

    // An arbitrary translation
    const Eigen::Vector3d translation(1.34, -5.214, 0.35654);


    // Some points on the x,y, and z axes
    const double a = 0.1;
    const double b = 0.4;
    const double c = 1.2;

    MatrixX3dRowMajor points(6, 3);
    points.row(0) << a, 0, 0;
    points.row(1) << -a, 0, 0;
    points.row(2) << 0, -b, 0;
    points.row(3) << 0, b, 0;
    points.row(4) << 0, 0, c;
    points.row(5) << 0, 0, -c;

    // Make sure that the pca was corrent
    auto verify_pca = [eps, a, b, c](
                          const double mass,
                          const Eigen::MatrixXd& pts,
                          const Eigen::MatrixXd& weights,
                          const Eigen::MatrixXd& components,
                          const Eigen::MatrixXd& R,
                          const Eigen::VectorXd& t) {
        const double a2 = a * a;
        const double b2 = b * b;
        const double c2 = c * c;

        auto approx0 = Approx(0).margin(eps);

        REQUIRE(weights(0) == Approx(mass * 2 * a2));
        REQUIRE(weights(1) == Approx(mass * 2 * b2));
        REQUIRE(weights(2) == Approx(mass * 2 * c2));
        REQUIRE((components.col(0) - R * Eigen::Vector3d(1, 0, 0)).norm() == approx0);
        REQUIRE((components.col(1) - R * Eigen::Vector3d(0, 1, 0)).norm() == approx0);
        REQUIRE((components.col(2) - R * Eigen::Vector3d(0, 0, 1)).norm() == approx0);

        Eigen::MatrixXd pminustr = pts.rowwise() - t.transpose();
        REQUIRE(
            (mass * pminustr.transpose() * pminustr -
             components * weights.asDiagonal() * components.transpose())
                .norm() == approx0);
    };

    SECTION("Column Major")
    {
        using MatrixType = MatrixX3dColMajor;
        SECTION("Simple case")
        {
            MatrixType points_tr = points;
            auto out =
                compute_pointcloud_pca(points_tr, false /*shift_center*/, false /*normalize*/);
            verify_pca(
                1 /*mass*/,
                points_tr,
                out.weights,
                out.components,
                Eigen::Matrix3d::Identity(),
                Eigen::Vector3d::Zero());
        }

        SECTION("With rotation")
        {
            MatrixType points_tr = points * rotation.transpose();
            auto out =
                compute_pointcloud_pca(points_tr, false /*shift_center*/, false /*normalize*/);
            verify_pca(
                1 /* mass */,
                points_tr,
                out.weights,
                out.components,
                rotation,
                Eigen::Vector3d::Zero());
            REQUIRE(out.center.norm() == Approx(0.).margin(eps));
        }

        SECTION("With rotation and translation")
        {
            MatrixType points_tr =
                (points * rotation.transpose()).rowwise() + translation.transpose();
            auto out =
                compute_pointcloud_pca(points_tr, true /*shift_center*/, false /*normalize*/);
            verify_pca(1 /* mass */, points_tr, out.weights, out.components, rotation, translation);
            REQUIRE((out.center - translation).norm() == Approx(0.).margin(eps));
        }

        SECTION("With rotation and translation, also scale the covariance matrix")
        {
            MatrixType points_tr =
                (points * rotation.transpose()).rowwise() + translation.transpose();
            const double mass = safe_cast<double>(1.) / (points.rows());
            auto out = compute_pointcloud_pca(points_tr, true /*shift_center*/, true /*normalize*/);
            verify_pca(mass, points_tr, out.weights, out.components, rotation, translation);
            REQUIRE((out.center - translation).norm() == Approx(0.).margin(eps));
        }
    }

    SECTION("ROW MAJOR")
    {
        using MatrixType = MatrixX3dRowMajor;
        SECTION("Simple case")
        {
            MatrixType points_tr = points;
            auto out =
                compute_pointcloud_pca(points_tr, false /*shift_center*/, false /*normalize*/);
            verify_pca(
                1 /*mass*/,
                points_tr,
                out.weights,
                out.components,
                Eigen::Matrix3d::Identity(),
                Eigen::Vector3d::Zero());
        }

        SECTION("With rotation")
        {
            MatrixType points_tr = points * rotation.transpose();
            auto out =
                compute_pointcloud_pca(points_tr, false /*shift_center*/, false /*normalize*/);
            verify_pca(
                1 /* mass */,
                points_tr,
                out.weights,
                out.components,
                rotation,
                Eigen::Vector3d::Zero());
            REQUIRE(out.center.norm() == Approx(0.).margin(eps));
        }

        SECTION("With rotation and translation")
        {
            MatrixType points_tr =
                (points * rotation.transpose()).rowwise() + translation.transpose();
            auto out =
                compute_pointcloud_pca(points_tr, true /*shift_center*/, false /*normalize*/);
            verify_pca(1 /* mass */, points_tr, out.weights, out.components, rotation, translation);
            REQUIRE((out.center - translation).norm() == Approx(0.).margin(eps));
        }

        SECTION("With rotation and translation, also scale the covariance matrix")
        {
            MatrixType points_tr =
                (points * rotation.transpose()).rowwise() + translation.transpose();
            const double mass = safe_cast<double>(1.) / (points.rows());
            auto out = compute_pointcloud_pca(points_tr, true /*shift_center*/, true /*normalize*/);
            verify_pca(mass, points_tr, out.weights, out.components, rotation, translation);
            REQUIRE((out.center - translation).norm() == Approx(0.).margin(eps));
        }
    }

} // end of TEST
