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
#include <lagrange/io/load_mesh.h>
#include <lagrange/raycasting/project_attributes.h>
#include <lagrange/raycasting/project_attributes_closest_point.h>
#include <lagrange/raycasting/project_attributes_directional.h>

#include <lagrange/testing/common.h>
#include <atomic>
#include <random>

namespace {

// Compute min edge length
template <typename DerivedV, typename DerivedF>
auto min_edge_length(
    const Eigen::MatrixBase<DerivedV>& vertices,
    const Eigen::MatrixBase<DerivedF>& facets) -> typename DerivedV::Scalar
{
    using Scalar = typename DerivedV::Scalar;
    using Index = Eigen::Index;

    Scalar len = std::numeric_limits<Scalar>::max();
    for (Index f = 0; f < facets.rows(); ++f) {
        for (Index lv = 0; lv < facets.cols(); ++lv) {
            auto p1 = vertices.row(facets(f, lv));
            auto p2 = vertices.row(facets(f, (lv + 1) % facets.cols()));
            len = std::min(len, (p1 - p2).norm());
        }
    }

    return len;
}

// Perturb vertices of the mesh
template <typename MeshType>
std::unique_ptr<MeshType> perturb_mesh(const MeshType& mesh, double s, bool z_only = false)
{
    using VertexArray = typename MeshType::VertexArray;
    VertexArray vertices = mesh.get_vertices();

    auto h = min_edge_length(mesh.get_vertices(), mesh.get_facets());
    std::mt19937 gen;
    std::uniform_real_distribution<double> dist(0, s * h);
    for (int i = 0; i < mesh.get_num_vertices(); ++i) {
        if (z_only) {
            vertices(i, 2) += dist(gen);
        } else {
            for (int c = 0; c < 3; ++c) {
                vertices(i, c) += dist(gen);
            }
        }
    }

    return lagrange::create_mesh(vertices, mesh.get_facets());
}

template <typename MeshType, typename DerivedV, typename DerivedP, typename DerivedI>
void naive_closest_points(
    const MeshType& mesh,
    const Eigen::MatrixBase<DerivedV>& queries,
    Eigen::PlainObjectBase<DerivedP>& closest_points,
    Eigen::PlainObjectBase<DerivedI>& facet_indices)
{
    closest_points.resize(queries.rows(), queries.cols());
    facet_indices.resize(queries.rows());
    for (int v = 0; v < queries.rows(); ++v) {
        Eigen::Vector3d q = queries.row(v).transpose();
        double best_d2 = std::numeric_limits<double>::max();

        for (int f = 0; f < mesh.get_num_facets(); ++f) {
            auto face = mesh.get_facets().row(f);
            Eigen::Vector3d v0 = mesh.get_vertices().row(face[0]).transpose();
            Eigen::Vector3d v1 = mesh.get_vertices().row(face[1]).transpose();
            Eigen::Vector3d v2 = mesh.get_vertices().row(face[2]).transpose();
            Eigen::Vector3d p;
            double l1, l2, l3;
            double d2 = lagrange::point_triangle_squared_distance(q, v0, v1, v2, p, l1, l2, l3);
            if (d2 < best_d2) {
                best_d2 = d2;
                closest_points.row(v) = p.transpose();
                facet_indices(v) = f;
            }
        }
    }
}

} // namespace

TEST_CASE("project_attributes_directional", "[raycasting]")
{
    auto source =
        lagrange::testing::load_mesh<lagrange::TriangleMesh3D>("open/core/hemisphere.obj");

    {
        using MeshType = decltype(source)::element_type;
        typename MeshType::AttributeArray P = source->get_vertices().leftCols(2);
        source->add_vertex_attribute("pos");
        source->set_vertex_attribute("pos", P);
    }

    Eigen::Vector3d direction(0, 0, 1);

    SECTION("wrong attribute")
    {
        // No existing attr, should throw
        auto target = perturb_mesh(*source, 0.1);
        LA_REQUIRE_THROWS(lagrange::raycasting::project_attributes_directional(
            *source,
            *target,
            {"new_attr"},
            direction));
    }

    SECTION("perturbed")
    {
        auto target = perturb_mesh(*source, 0.1);

        // Not a bool since we write to this guy in parallel
        std::vector<char> ishit(target->get_num_vertices(), true);

        lagrange::raycasting::project_attributes_directional(
            *source,
            *target,
            {"pos"},
            direction,
            lagrange::raycasting::CastMode::BOTH_WAYS,
            lagrange::raycasting::WrapMode::CONSTANT,
            0,
            [&](int v, bool hit) { ishit[v] = hit; });
        target->has_vertex_attribute("pos");
        const auto& V = target->get_vertices();
        const auto& P = target->get_vertex_attribute("pos");
        REQUIRE(P.cols() == 2);
        for (int v = 0; v < target->get_num_vertices(); ++v) {
            if (ishit[v]) {
                REQUIRE((P.row(v) - V.row(v).head<2>()).norm() < 1e-7);
            }
        }
    }

    SECTION("perturbed in z")
    {
        auto target = perturb_mesh(*source, 0.1, true);

        std::atomic_bool all_hit(true);
        lagrange::raycasting::project_attributes_directional(
            *source,
            *target,
            {"pos"},
            direction,
            lagrange::raycasting::CastMode::BOTH_WAYS,
            lagrange::raycasting::WrapMode::CONSTANT,
            0,
            [&](int /*v*/, bool hit) {
                if (!hit) all_hit = false;
            });
        REQUIRE(all_hit);
        target->has_vertex_attribute("pos");
        const auto& V = target->get_vertices();
        const auto& P = target->get_vertex_attribute("pos");
        REQUIRE(P.cols() == 2);
        for (int v = 0; v < target->get_num_vertices(); ++v) {
            REQUIRE((P.row(v) - V.row(v).head<2>()).norm() < 1e-7);
        }
    }

    SECTION("exact copy")
    {
        auto target = lagrange::create_mesh(source->get_vertices(), source->get_facets());

        std::atomic_bool all_hit(true);
        lagrange::raycasting::project_attributes_directional(
            *source,
            *target,
            {"pos"},
            direction,
            lagrange::raycasting::CastMode::BOTH_WAYS,
            lagrange::raycasting::WrapMode::CONSTANT,
            0,
            [&](int /*v*/, bool hit) {
                if (!hit) all_hit = false;
            });
        REQUIRE(all_hit);
        target->has_vertex_attribute("pos");
        const auto& V = target->get_vertices();
        const auto& P = target->get_vertex_attribute("pos");
        REQUIRE(P.cols() == 2);
        for (int v = 0; v < target->get_num_vertices(); ++v) {
            CAPTURE(v, P.row(v), V.row(v));
            REQUIRE((P.row(v) - V.row(v).head<2>()).norm() < 1e-7);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

TEST_CASE("project_attributes_closest_point", "[raycasting]")
{
    auto source =
        lagrange::testing::load_mesh<lagrange::TriangleMesh3D>("open/core/hemisphere.obj");

    {
        using MeshType = decltype(source)::element_type;
        typename MeshType::AttributeArray P = source->get_vertices().leftCols(2);
        source->add_vertex_attribute("pos");
        source->set_vertex_attribute("pos", P);
    }

    SECTION("wrong attribute")
    {
        // No existing attr, should throw
        auto target = perturb_mesh(*source, 0.1);
        LA_REQUIRE_THROWS(
            lagrange::raycasting::project_attributes_closest_point(*source, *target, {"new_attr"}));
    }

    SECTION("perturbed")
    {
        auto target = perturb_mesh(*source, 0.1);

        lagrange::raycasting::project_attributes_closest_point(*source, *target, {"pos"});
        const auto& S = source->get_vertex_attribute("pos");
        target->has_vertex_attribute("pos");
        const auto& P = target->get_vertex_attribute("pos");
        REQUIRE(P.cols() == 2);
        Eigen::MatrixXd C;
        Eigen::VectorXi I;
        naive_closest_points(*source, target->get_vertices(), C, I);
        for (int v = 0; v < C.rows(); ++v) {
            auto face = source->get_facets().row(I(v));
            Eigen::Vector3d v0 = source->get_vertices().row(face[0]).transpose();
            Eigen::Vector3d v1 = source->get_vertices().row(face[1]).transpose();
            Eigen::Vector3d v2 = source->get_vertices().row(face[2]).transpose();
            Eigen::Vector3d p = C.row(v).transpose();
            Eigen::Vector3d bary = lagrange::compute_barycentric_coordinates(v0, v1, v2, p);

            Eigen::RowVector2d attr;
            attr.setZero();
            for (int lv = 0; lv < 3; ++lv) {
                attr += S.row(face[lv]) * bary[lv];
            }
            REQUIRE((attr - P.row(v)).norm() < 1e-7);
        }
    }

    SECTION("exact copy")
    {
        auto target = lagrange::create_mesh(source->get_vertices(), source->get_facets());

        lagrange::raycasting::project_attributes_closest_point(*source, *target, {"pos"});
        target->has_vertex_attribute("pos");
        const auto& V = target->get_vertices();
        const auto& P = target->get_vertex_attribute("pos");
        REQUIRE(P.cols() == 2);
        for (int v = 0; v < target->get_num_vertices(); ++v) {
            CAPTURE(v, P.row(v), V.row(v));
            REQUIRE((P.row(v) - V.row(v).head<2>()).norm() < 1e-16);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

TEST_CASE("project_attributes: reproducibility", "[raycasting]")
{
    auto source =
        lagrange::testing::load_mesh<lagrange::TriangleMesh3D>("open/core/hemisphere.obj");

    {
        using MeshType = decltype(source)::element_type;
        typename MeshType::AttributeArray P = source->get_vertices().leftCols(2);
        source->add_vertex_attribute("pos");
        source->set_vertex_attribute("pos", P);
    }

    using ProjectMode = lagrange::raycasting::ProjectMode;

    for (auto proj :
         {ProjectMode::CLOSEST_VERTEX, ProjectMode::CLOSEST_POINT, ProjectMode::RAY_CASTING}) {
        auto target1 = perturb_mesh(*source, 0.1);
        auto target2 = perturb_mesh(*source, 0.1);
        REQUIRE(source->get_vertices() != target2->get_vertices());
        REQUIRE(target1->get_vertices() == target2->get_vertices());
        REQUIRE(!target1->has_vertex_attribute("pos"));
        REQUIRE(!target2->has_vertex_attribute("pos"));

        lagrange::raycasting::project_attributes(*source, *target1, {"pos"}, proj);
        lagrange::raycasting::project_attributes(*source, *target2, {"pos"}, proj);
        const auto& P1 = target1->get_vertex_attribute("pos");
        const auto& P2 = target2->get_vertex_attribute("pos");
        REQUIRE(P1 == P2);
    }
}
