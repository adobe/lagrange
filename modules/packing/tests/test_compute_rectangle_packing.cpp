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
#include <lagrange/common.h>
#include <lagrange/create_mesh.h>
#include <lagrange/packing/compute_rectangle_packing.h>
#include <lagrange/testing/common.h>

#include <vector>

namespace packing_internal {

template <typename Derived>
bool is_disjoint(
    const Eigen::MatrixBase<Derived>& bbox_min_1,
    const Eigen::MatrixBase<Derived>& bbox_max_1,
    const Eigen::MatrixBase<Derived>& bbox_min_2,
    const Eigen::MatrixBase<Derived>& bbox_max_2)
{
    const auto dim = bbox_min_1.cols();
    for (auto i : lagrange::range(dim)) {
        if (bbox_max_1[i] < bbox_min_2[i]) return true;
        if (bbox_min_1[i] > bbox_max_2[i]) return true;
    }
    return false;
}

template <typename BBoxType, typename CenterType>
bool is_disjoint(
    const BBoxType& bbox_mins,
    const BBoxType& bbox_maxs,
    const CenterType& centers,
    const std::vector<bool>& flipped)
{
    using Scalar = typename BBoxType::Scalar;
    const int num_boxes = lagrange::safe_cast<int>(flipped.size());

    auto extract_bbox = [&](int i) {
        Eigen::Matrix<Scalar, 1, 2> bbox_min, bbox_max;
        if (flipped[i]) {
            bbox_min[0] = centers(i, 0) - 0.5 * (bbox_maxs(i, 1) - bbox_mins(i, 1));
            bbox_min[1] = centers(i, 1) - 0.5 * (bbox_maxs(i, 0) - bbox_mins(i, 0));
            bbox_max[0] = centers(i, 0) + 0.5 * (bbox_maxs(i, 1) - bbox_mins(i, 1));
            bbox_max[1] = centers(i, 1) + 0.5 * (bbox_maxs(i, 0) - bbox_mins(i, 0));
        } else {
            bbox_min = centers.row(i) - 0.5 * (bbox_maxs.row(i) - bbox_mins.row(i));
            bbox_max = centers.row(i) + 0.5 * (bbox_maxs.row(i) - bbox_mins.row(i));
        }
        return std::make_tuple(bbox_min, bbox_max);
    };

    Eigen::Matrix<Scalar, 1, 2> bbox_min_i, bbox_max_i, bbox_min_j, bbox_max_j;
    for (int i = 0; i < num_boxes; i++) {
        std::tie(bbox_min_i, bbox_max_i) = extract_bbox(i);
        for (int j = i + 1; j < num_boxes; j++) {
            std::tie(bbox_min_j, bbox_max_j) = extract_bbox(j);
            if (!is_disjoint(bbox_min_i, bbox_max_i, bbox_min_j, bbox_max_j)) {
                return false;
            }
        }
    }
    return true;
}

template <typename MeshType>
bool is_disjoint(const MeshType& mesh_1, const MeshType& mesh_2)
{
    const auto& vertices_1 = mesh_1.get_vertices();
    const auto& vertices_2 = mesh_2.get_vertices();
    return is_disjoint(
        vertices_1.colwise().minCoeff().eval(),
        vertices_1.colwise().maxCoeff().eval(),
        vertices_2.colwise().minCoeff().eval(),
        vertices_2.colwise().maxCoeff().eval());
}

} // namespace packing_internal

#ifdef RECTANGLE_BIN_PACK_OSS
    #define LA_HIDDEN_TAG "[.]"
#else
    #define LA_HIDDEN_TAG
#endif

TEST_CASE("pack_boxes", "[packing]" LA_HIDDEN_TAG)
{
    using namespace lagrange;

    SECTION("Unit boxes")
    {
        Eigen::Matrix<double, Eigen::Dynamic, 2, Eigen::RowMajor> bbox_mins(2, 2);
        Eigen::Matrix<double, Eigen::Dynamic, 2, Eigen::RowMajor> bbox_maxs(2, 2);
        bbox_mins.setZero();
        bbox_maxs.setOnes();

        Eigen::Matrix<double, Eigen::Dynamic, 2, Eigen::RowMajor> centers;
        std::vector<bool> flipped;
        std::tie(centers, flipped) = packing::pack_boxes(bbox_mins, bbox_maxs, true);

        REQUIRE(centers.rows() == 2);
        REQUIRE(flipped.size() == 2);

        REQUIRE((centers.row(0) - centers.row(1)).cwiseAbs().maxCoeff() >= 1.0);
        REQUIRE(packing_internal::is_disjoint(bbox_mins, bbox_maxs, centers, flipped));
    }

    SECTION("Long rectangle")
    {
        Eigen::Matrix<double, Eigen::Dynamic, 2, Eigen::RowMajor> bbox_mins(2, 2);
        Eigen::Matrix<double, Eigen::Dynamic, 2, Eigen::RowMajor> bbox_maxs(2, 2);
        bbox_mins.setZero();
        bbox_maxs << 10.0, 1.0, 10.0, 1.0;

        Eigen::Matrix<double, Eigen::Dynamic, 2, Eigen::RowMajor> centers;
        std::vector<bool> flipped;
        std::tie(centers, flipped) = packing::pack_boxes(bbox_mins, bbox_maxs, true);

        REQUIRE(centers.rows() == 2);
        REQUIRE(flipped.size() == 2);

        REQUIRE((centers.row(0) - centers.row(1)).cwiseAbs().maxCoeff() >= 1.0);
        REQUIRE(!flipped[0]);
        REQUIRE(!flipped[1]);
        REQUIRE(packing_internal::is_disjoint(bbox_mins, bbox_maxs, centers, flipped));
    }

    SECTION("Long rectangle 2")
    {
        Eigen::Matrix<double, Eigen::Dynamic, 2, Eigen::RowMajor> bbox_mins(2, 2);
        Eigen::Matrix<double, Eigen::Dynamic, 2, Eigen::RowMajor> bbox_maxs(2, 2);
        bbox_mins.setZero();
        bbox_maxs << 10.0, 1.0, 1.0, 10.0;

        Eigen::Matrix<double, Eigen::Dynamic, 2, Eigen::RowMajor> centers;
        std::vector<bool> flipped;
        std::tie(centers, flipped) = packing::pack_boxes(bbox_mins, bbox_maxs, true);

        REQUIRE(centers.rows() == 2);
        REQUIRE(flipped.size() == 2);
        REQUIRE(packing_internal::is_disjoint(bbox_mins, bbox_maxs, centers, flipped));
    }

    SECTION("Nearly degenerate boxes")
    {
        constexpr double EPS = std::numeric_limits<double>::epsilon();
        const double M = std::sqrt(std::numeric_limits<double>::max());

        Eigen::Matrix<double, Eigen::Dynamic, 2, Eigen::RowMajor> bbox_mins(2, 2);
        Eigen::Matrix<double, Eigen::Dynamic, 2, Eigen::RowMajor> bbox_maxs(2, 2);
        bbox_mins.setZero();
        bbox_maxs << M, EPS, EPS, M;

        Eigen::Matrix<double, Eigen::Dynamic, 2, Eigen::RowMajor> centers;
        std::vector<bool> flipped;
        std::tie(centers, flipped) = packing::pack_boxes(bbox_mins, bbox_maxs, true);

        REQUIRE(centers.rows() == 2);
        REQUIRE(flipped.size() == 2);
        REQUIRE(centers.array().isFinite().all());
        REQUIRE(packing_internal::is_disjoint(bbox_mins, bbox_maxs, centers, flipped));
    }

    SECTION("Exactly degenerate boxes")
    {
        // Even when input boxes are degenerate, the margin we add should make
        // pakcing a well defined problem. :D
        double M;

        SECTION("Semi-degenerate")
        {
            M = std::sqrt(std::numeric_limits<double>::max());
        }
        SECTION("Completely degenerate")
        {
            M = 0;
        }

        Eigen::Matrix<double, Eigen::Dynamic, 2, Eigen::RowMajor> bbox_mins(2, 2);
        Eigen::Matrix<double, Eigen::Dynamic, 2, Eigen::RowMajor> bbox_maxs(2, 2);
        bbox_mins.setZero();
        bbox_maxs << M, 0, 0, M;

        Eigen::Matrix<double, Eigen::Dynamic, 2, Eigen::RowMajor> centers;
        std::vector<bool> flipped;
        std::tie(centers, flipped) = packing::pack_boxes(bbox_mins, bbox_maxs, true);

        REQUIRE(centers.rows() == 2);
        REQUIRE(flipped.size() == 2);
        REQUIRE(centers.array().isFinite().all());
        REQUIRE(packing_internal::is_disjoint(bbox_mins, bbox_maxs, centers, flipped));
    }

    SECTION("Flip check")
    {
        Eigen::Matrix<double, Eigen::Dynamic, 2, Eigen::RowMajor> bbox_mins(2, 2);
        Eigen::Matrix<double, Eigen::Dynamic, 2, Eigen::RowMajor> bbox_maxs(2, 2);
        bbox_mins.setZero();
        bbox_maxs << 10, 4, 4, 10;

        Eigen::Matrix<double, Eigen::Dynamic, 2, Eigen::RowMajor> centers;
        std::vector<bool> flipped;

        SECTION("Allow flips")
        {
            std::tie(centers, flipped) = packing::pack_boxes(bbox_mins, bbox_maxs, true);
            REQUIRE(flipped.size() == 2);
            REQUIRE((flipped[0] || flipped[1])); // One box must be flipped.
        }
        SECTION("No flips")
        {
            std::tie(centers, flipped) = packing::pack_boxes(bbox_mins, bbox_maxs, false);
            REQUIRE(flipped.size() == 2);
            REQUIRE_FALSE(flipped[0]);
            REQUIRE_FALSE(flipped[1]);
        }

        REQUIRE(centers.rows() == 2);
        REQUIRE(flipped.size() == 2);
        REQUIRE(centers.array().isFinite().all());
        REQUIRE(packing_internal::is_disjoint(bbox_mins, bbox_maxs, centers, flipped));
    }
}

TEST_CASE("pack_2d_meshes", "[packing]")
{
    using namespace lagrange;

    Vertices2D vertices(3, 2);
    vertices << 0.0, 0.0, 1.0, 0.0, 1.0, 1.0;

    Triangles facets(1, 3);
    facets << 0, 1, 2;

    auto mesh_1 = create_mesh(vertices, facets);
    auto mesh_2 = create_mesh(vertices, facets);

    SECTION("Two triangles")
    {
        std::vector<TriangleMesh2D*> meshes{mesh_1.get(), mesh_2.get()};
        packing::PackingOptions opt;
#ifndef RECTANGLE_BIN_PACK_OSS
        opt.allow_flip = false;
#endif
        packing::compute_rectangle_packing(meshes, opt);
        REQUIRE(packing_internal::is_disjoint(*meshes[0], *meshes[1]));
    }
}
