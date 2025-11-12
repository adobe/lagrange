/*
 * Copyright 2025 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */

#include <lagrange/polyddg/DifferentialOperators.h>
#include <lagrange/testing/common.h>
#include <lagrange/views.h>

#include <Eigen/Core>
#include <Eigen/Eigenvalues>
#include <Eigen/Sparse>

#include <catch2/matchers/catch_matchers_floating_point.hpp>

TEST_CASE("DifferentialOperators", "[polyddg]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    // Shared test meshes
    SurfaceMesh<Scalar, Index> triangle_mesh;
    triangle_mesh.add_vertex({1.0, 0.0, 0.0});
    triangle_mesh.add_vertex({0.0, 1.0, 0.0});
    triangle_mesh.add_vertex({0.0, 0.0, 1.0});
    triangle_mesh.add_triangle(0, 1, 2);

    SurfaceMesh<Scalar, Index> pyramid_mesh;
    pyramid_mesh.add_vertex({0.0, 0.0, 0.0});
    pyramid_mesh.add_vertex({1.0, 0.0, 0.0});
    pyramid_mesh.add_vertex({1.0, 1.0, 0.0});
    pyramid_mesh.add_vertex({0.0, 1.0, 0.0});
    pyramid_mesh.add_vertex({0.5, 0.5, 1.0});
    pyramid_mesh.add_triangle(0, 1, 4);
    pyramid_mesh.add_triangle(1, 2, 4);
    pyramid_mesh.add_triangle(2, 3, 4);
    pyramid_mesh.add_triangle(3, 0, 4);
    pyramid_mesh.add_quad(0, 3, 2, 1);

    SurfaceMesh<Scalar, Index> quad_mesh;
    quad_mesh.add_vertex({0.0, 0.0, 1.0});
    quad_mesh.add_vertex({1.0, 0.0, 0.0});
    quad_mesh.add_vertex({1.0, 1.0, 1.0});
    quad_mesh.add_vertex({0.0, 1.0, 0.0});
    quad_mesh.add_quad(0, 1, 2, 3);

    SECTION("gradient")
    {
        SECTION("triangle")
        {
            polyddg::DifferentialOperators<Scalar, Index> diff_ops(triangle_mesh);
            auto G = diff_ops.gradient();
            REQUIRE(G.rows() == 3);
            REQUIRE(G.cols() == 3);

            Eigen::Matrix<Scalar, 3, 3> G_dense(G);
            Eigen::Matrix<Scalar, 3, 3> G_expected;
            G_expected.row(0) << 1.0, -0.5, -0.5;
            G_expected.row(1) << -0.5, 1.0, -0.5;
            G_expected.row(2) << -0.5, -0.5, 1.0;
            G_expected /= 1.5;

            REQUIRE_THAT((G_dense - G_expected).norm(), Catch::Matchers::WithinAbs(0.0, 1e-12));
        }

        SECTION("quad")
        {
            polyddg::DifferentialOperators<Scalar, Index> diff_ops(quad_mesh);
            auto G = diff_ops.gradient();
            REQUIRE(G.rows() == 3);
            REQUIRE(G.cols() == 4);

            Eigen::Vector4d v0(1, 1, 1, 1);
            Eigen::Vector3d g0 = G * v0;
            REQUIRE_THAT(g0.norm(), Catch::Matchers::WithinAbs(0.0, 1e-12));

            Eigen::Vector4d v1(1, -1, 1, -1);
            Eigen::Vector3d g1 = G * v1;
            REQUIRE_THAT(g1.norm(), Catch::Matchers::WithinAbs(0.0, 1e-12));

            Eigen::Vector4d v2(-1, -1, 1, 1);
            Eigen::Vector3d g2 = G * v2;
            Eigen::Vector3d g2_expected(0, 2, 0);
            REQUIRE_THAT((g2 - g2_expected).norm(), Catch::Matchers::WithinAbs(0.0, 1e-12));
        }
    }

    SECTION("d0")
    {
        SECTION("triangle")
        {
            polyddg::DifferentialOperators<Scalar, Index> diff_ops(triangle_mesh);
            auto D0 = diff_ops.d0();
            REQUIRE(D0.rows() == 3);
            REQUIRE(D0.cols() == 3);

            Eigen::Vector3d v0(1, 1, 1);
            Eigen::Vector3d d0 = D0 * v0;
            REQUIRE_THAT(d0.norm(), Catch::Matchers::WithinAbs(0.0, 1e-12));

            Eigen::Vector3d v1(1, 0, 0);
            Eigen::Vector3d d1 = D0 * v1;
            REQUIRE_THAT(d1.sum(), Catch::Matchers::WithinAbs(0.0, 1e-12));

            auto D1 = diff_ops.d1();
            auto DD = D1 * D0;
            REQUIRE_THAT(DD.norm(), Catch::Matchers::WithinAbs(0.0, 1e-12));
        }

        SECTION("pyramid")
        {
            polyddg::DifferentialOperators<Scalar, Index> diff_ops(pyramid_mesh);
            auto D0 = diff_ops.d0();
            REQUIRE(D0.rows() == 8);
            REQUIRE(D0.cols() == 5);

            Eigen::VectorXd v0(5);
            v0 << 1, 1, 1, 1, 1;
            Eigen::VectorXd d0 = D0 * v0;
            REQUIRE_THAT(d0.norm(), Catch::Matchers::WithinAbs(0.0, 1e-12));

            auto D1 = diff_ops.d1();
            auto DD = D1 * D0;
            REQUIRE_THAT(DD.norm(), Catch::Matchers::WithinAbs(0.0, 1e-12));
        }
    }

    SECTION("d1")
    {
        SECTION("triangle")
        {
            polyddg::DifferentialOperators<Scalar, Index> diff_ops(triangle_mesh);
            auto D1 = diff_ops.d1();
            REQUIRE(D1.rows() == 1);
            REQUIRE(D1.cols() == 3);

            Eigen::Vector3d v0(1, 1, 1);
            Eigen::VectorXd d0 = D1 * v0;
            REQUIRE_THAT(d0(0, 0), Catch::Matchers::WithinAbs(3, 1e-12));

            Eigen::Vector3d v1(-1, 2, -1);
            Eigen::VectorXd d1 = D1 * v1;
            REQUIRE_THAT(d1(0, 0), Catch::Matchers::WithinAbs(0, 1e-12));
        }

        SECTION("pyramid")
        {
            polyddg::DifferentialOperators<Scalar, Index> diff_ops(pyramid_mesh);
            auto D1 = diff_ops.d1();
            REQUIRE(D1.rows() == 5);
            REQUIRE(D1.cols() == 8);

            for (Index fid = 0; fid < pyramid_mesh.get_num_facets(); fid++) {
                auto D0f = diff_ops.d0(fid);
                auto D1f = diff_ops.d1(fid);
                REQUIRE_THAT((D1f * D0f).norm(), Catch::Matchers::WithinAbs(0.0, 1e-12));
            }
        }
    }

    SECTION("flat")
    {
        SECTION("triangle")
        {
            polyddg::DifferentialOperators<Scalar, Index> diff_ops(triangle_mesh);
            auto V = diff_ops.flat();
            REQUIRE(V.rows() == 3);
            REQUIRE(V.cols() == 3);

            // Lemma 5: P * V = 0 for a single facet
            auto P = diff_ops.projection();
            REQUIRE_THAT((P * V).norm(), Catch::Matchers::WithinAbs(0.0, 1e-12));
        }

        SECTION("non-planar polygon")
        {
            polyddg::DifferentialOperators<Scalar, Index> diff_ops(quad_mesh);
            auto V = diff_ops.flat();
            REQUIRE(V.rows() == 4);
            REQUIRE(V.cols() == 3);

            // Lemma 5: P * V = 0 for a single facet
            auto P = diff_ops.projection();
            REQUIRE_THAT((P * V).norm(), Catch::Matchers::WithinAbs(0.0, 1e-12));
        }

        SECTION("pyramid")
        {
            polyddg::DifferentialOperators<Scalar, Index> diff_ops(pyramid_mesh);
            auto V = diff_ops.flat();
            REQUIRE(V.rows() == 8);
            REQUIRE(V.cols() == 15);
        }
    }

    SECTION("sharp")
    {
        SECTION("triangle")
        {
            polyddg::DifferentialOperators<Scalar, Index> diff_ops(triangle_mesh);
            auto U = diff_ops.sharp();
            REQUIRE(U.rows() == 3);
            REQUIRE(U.cols() == 3);

            auto D0 = diff_ops.d0();
            auto G = diff_ops.gradient();
            REQUIRE_THAT((G - U * D0).norm(), Catch::Matchers::WithinAbs(0.0, 1e-12));
        }

        SECTION("pyramid")
        {
            polyddg::DifferentialOperators<Scalar, Index> diff_ops(pyramid_mesh);
            auto U = diff_ops.sharp();
            REQUIRE(U.rows() == 15);
            REQUIRE(U.cols() == 8);

            auto D0 = diff_ops.d0();
            auto V = diff_ops.flat();
            auto G = diff_ops.gradient();

            // Lemma 4 check: U * V * df = df for any scalar field f.
            Eigen::VectorXd f(5);
            f << 0, 0, 0, 0, 1;
            Eigen::VectorXd df = G * f;
            Eigen::VectorXd vdf = V * df;
            Eigen::VectorXd df2 = U * V * df;
            REQUIRE_THAT((df - df2).norm(), Catch::Matchers::WithinAbs(0.0, 1e-12));

            // Lemma 2 check: G = U * D0
            REQUIRE_THAT((G - U * D0).norm(), Catch::Matchers::WithinAbs(0.0, 1e-12));
        }
    }

    SECTION("projection")
    {
        SECTION("triangle")
        {
            polyddg::DifferentialOperators<Scalar, Index> diff_ops(triangle_mesh);
            auto P = diff_ops.projection();
            REQUIRE(P.rows() == 3);
            REQUIRE(P.cols() == 3);

            // Lemma 5: P * V = 0 for a single facet
            auto V = diff_ops.flat();
            REQUIRE_THAT((P * V).norm(), Catch::Matchers::WithinAbs(0.0, 1e-12));

            // Lemma 6: P * P = P
            REQUIRE_THAT((P * P - P).norm(), Catch::Matchers::WithinAbs(0.0, 1e-12));
        }

        SECTION("pyramid")
        {
            polyddg::DifferentialOperators<Scalar, Index> diff_ops(pyramid_mesh);
            auto P = diff_ops.projection();
            REQUIRE(P.rows() == 8);
            REQUIRE(P.cols() == 8);

            for (Index fid = 0; fid < pyramid_mesh.get_num_facets(); fid++) {
                auto Pf = diff_ops.projection(fid);
                auto Vf = diff_ops.flat(fid);
                auto Uf = diff_ops.sharp(fid);

                // The following lemmas only hold per-facet. They do not hold globally due to the
                // averaging behavior of the flat operator (and consequently the projection
                // operator).

                // Lemma 5: Pf * Vf = 0
                REQUIRE_THAT((Pf * Vf).norm(), Catch::Matchers::WithinAbs(0.0, 1e-12));

                // Lemma 6: Pf * Pf = Pf
                REQUIRE_THAT((Pf * Pf - Pf).norm(), Catch::Matchers::WithinAbs(0.0, 1e-12));

                // Lemma 7: Uf * Pf = 0
                REQUIRE_THAT((Uf * Pf).norm(), Catch::Matchers::WithinAbs(0.0, 1e-12));
            }
        }
    }

    SECTION("inner_product")
    {
        SECTION("triangle")
        {
            polyddg::DifferentialOperators<Scalar, Index> diff_ops(triangle_mesh);

            auto M0 = diff_ops.inner_product_0_form();
            REQUIRE(M0.rows() == 3);
            REQUIRE(M0.cols() == 3);

            auto M1 = diff_ops.inner_product_1_form();
            REQUIRE(M1.rows() == 3);
            REQUIRE(M1.cols() == 3);

            auto M2 = diff_ops.inner_product_2_form();
            REQUIRE(M2.rows() == 1);
            REQUIRE(M2.cols() == 1);
        }

        SECTION("pyramid")
        {
            polyddg::DifferentialOperators<Scalar, Index> diff_ops(pyramid_mesh);

            auto M0 = diff_ops.inner_product_0_form();
            REQUIRE(M0.rows() == 5);
            REQUIRE(M0.cols() == 5);

            auto M1 = diff_ops.inner_product_1_form();
            REQUIRE(M1.rows() == 8);
            REQUIRE(M1.cols() == 8);

            auto M2 = diff_ops.inner_product_2_form();
            REQUIRE(M2.rows() == 5);
            REQUIRE(M2.cols() == 5);
        }
    }

    SECTION("star0")
    {
        SECTION("triangle")
        {
            polyddg::DifferentialOperators<Scalar, Index> diff_ops(triangle_mesh);

            auto S0 = diff_ops.star0();
            REQUIRE(S0.rows() == 3);
            REQUIRE(S0.cols() == 3);
        }

        SECTION("pyramid")
        {
            polyddg::DifferentialOperators<Scalar, Index> diff_ops(pyramid_mesh);

            auto S0 = diff_ops.star0();
            REQUIRE(S0.rows() == 5);
            REQUIRE(S0.cols() == 5);
        }
    }

    SECTION("star1")
    {
        SECTION("triangle")
        {
            polyddg::DifferentialOperators<Scalar, Index> diff_ops(triangle_mesh);

            auto S1 = diff_ops.star1();
            REQUIRE(S1.rows() == 3);
            REQUIRE(S1.cols() == 3);
        }

        SECTION("pyramid")
        {
            polyddg::DifferentialOperators<Scalar, Index> diff_ops(pyramid_mesh);

            auto S1 = diff_ops.star1();
            REQUIRE(S1.rows() == 8);
            REQUIRE(S1.cols() == 8);
        }
    }

    SECTION("star2")
    {
        SECTION("triangle")
        {
            polyddg::DifferentialOperators<Scalar, Index> diff_ops(triangle_mesh);

            auto S2 = diff_ops.star2();
            REQUIRE(S2.rows() == 1);
            REQUIRE(S2.cols() == 1);
        }

        SECTION("pyramid")
        {
            polyddg::DifferentialOperators<Scalar, Index> diff_ops(pyramid_mesh);

            auto S2 = diff_ops.star2();
            REQUIRE(S2.rows() == 5);
            REQUIRE(S2.cols() == 5);
        }
    }

    SECTION("Laplacian")
    {
        SECTION("triangle")
        {
            polyddg::DifferentialOperators<Scalar, Index> diff_ops(triangle_mesh);

            auto L = diff_ops.laplacian();
            REQUIRE(L.rows() == 3);
            REQUIRE(L.cols() == 3);

            Eigen::SparseMatrix<Scalar> LT = L.transpose();
            REQUIRE_THAT((L - LT).norm(), Catch::Matchers::WithinAbs(0.0, 1e-12));

            Eigen::VectorXd ones = Eigen::VectorXd::Ones(3);
            REQUIRE_THAT((L * ones).norm(), Catch::Matchers::WithinAbs(0.0, 1e-10));
        }

        SECTION("pyramid")
        {
            polyddg::DifferentialOperators<Scalar, Index> diff_ops(pyramid_mesh);

            auto L = diff_ops.laplacian();
            REQUIRE(L.rows() == 5);
            REQUIRE(L.cols() == 5);

            Eigen::SparseMatrix<Scalar> LT = L.transpose();
            REQUIRE_THAT((L - LT).norm(), Catch::Matchers::WithinAbs(0.0, 1e-12));

            Eigen::VectorXd ones = Eigen::VectorXd::Ones(5);
            REQUIRE_THAT((L * ones).norm(), Catch::Matchers::WithinAbs(0.0, 1e-10));

            Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> es(L);
            auto eigenvalues = es.eigenvalues();

            // Smallest eigenvalue should be ~0 (corresponds to constant function)
            REQUIRE_THAT(std::abs(eigenvalues(0)), Catch::Matchers::WithinAbs(0.0, 1e-10));

            // Other eigenvalues should be positive
            for (Index i = 1; i < eigenvalues.size(); i++) {
                REQUIRE(eigenvalues(i) > 1e-10);
            }
        }
    }

    SECTION("Connection Laplacian")
    {
        SECTION("triangle")
        {
            polyddg::DifferentialOperators<Scalar, Index> diff_ops(triangle_mesh);

            auto L = diff_ops.connection_laplacian();
            REQUIRE(L.rows() == 6);
            REQUIRE(L.cols() == 6);

            Eigen::SparseMatrix<Scalar> LT = L.transpose();
            REQUIRE_THAT((L - LT).norm(), Catch::Matchers::WithinAbs(0.0, 1e-12));

            Eigen::Matrix<Scalar, 9, 1> v;
            v.setOnes();

            auto B = diff_ops.vertex_tangent_coordinates();
            REQUIRE(B.rows() == 6);
            REQUIRE(B.cols() == 9);

            Eigen::Matrix<Scalar, 6, 1> Lv = L * B * v;
            REQUIRE(Lv.norm() < 1e-10);
        }

        SECTION("pyramid")
        {
            polyddg::DifferentialOperators<Scalar, Index> diff_ops(pyramid_mesh);

            auto L = diff_ops.connection_laplacian();
            REQUIRE(L.rows() == 10);
            REQUIRE(L.cols() == 10);

            Eigen::SparseMatrix<Scalar> LT = L.transpose();
            REQUIRE_THAT((L - LT).norm(), Catch::Matchers::WithinAbs(0.0, 1e-12));

            Index num_facets = pyramid_mesh.get_num_facets();
            Eigen::Matrix<Scalar, 1, 3> test_vec(0, 0, 1);

            for (Index fid = 0; fid < num_facets; fid++) {
                auto Lf = diff_ops.connection_laplacian(fid);
                Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic> LfT = Lf.transpose();
                REQUIRE_THAT((Lf - LfT).norm(), Catch::Matchers::WithinAbs(0.0, 1e-12));

                auto basis_f = diff_ops.facet_basis(fid);
                Index facet_size = pyramid_mesh.get_facet_size(fid);

                Eigen::Matrix<Scalar, Eigen::Dynamic, 1> tv_test(facet_size * 2);
                for (Index lv = 0; lv < facet_size; lv++) {
                    auto R = diff_ops.levi_civita(fid, lv);
                    tv_test.template segment<2>(lv * 2) =
                        (R.inverse() * (basis_f.transpose() * test_vec.transpose())).transpose();
                }

                Eigen::Matrix<Scalar, Eigen::Dynamic, 1> r = Lf * tv_test;
                REQUIRE_THAT(r.norm(), Catch::Matchers::WithinAbs(0.0, 1e-10));
            }
        }
    }
}
