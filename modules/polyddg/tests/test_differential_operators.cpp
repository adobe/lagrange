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
#include <lagrange/primitive/generate_torus.h>
#include <lagrange/testing/common.h>
#include <lagrange/testing/create_test_mesh.h>
#include <lagrange/triangulate_polygonal_facets.h>
#include <lagrange/views.h>

#include <Eigen/Core>
#include <Eigen/Eigenvalues>
#include <Eigen/Sparse>

#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <limits>
#include <vector>

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

    SECTION("shape operator")
    {
        // ---- symmetry on simple meshes ----

        SECTION("symmetry - triangle")
        {
            polyddg::DifferentialOperators<Scalar, Index> ops(triangle_mesh);
            Eigen::Matrix<Scalar, 2, 2> S = ops.shape_operator(0);
            REQUIRE_THAT((S - S.transpose()).norm(), Catch::Matchers::WithinAbs(0.0, 1e-12));
        }

        SECTION("symmetry - pyramid")
        {
            polyddg::DifferentialOperators<Scalar, Index> ops(pyramid_mesh);
            for (Index fid = 0; fid < pyramid_mesh.get_num_facets(); fid++) {
                Eigen::Matrix<Scalar, 2, 2> S = ops.shape_operator(fid);
                REQUIRE_THAT((S - S.transpose()).norm(), Catch::Matchers::WithinAbs(0.0, 1e-12));
            }
        }

        // ---- global operator dimensions on simple meshes ----

        SECTION("global operator dimensions - triangle")
        {
            polyddg::DifferentialOperators<Scalar, Index> ops(triangle_mesh);
            auto S = ops.shape_operator();
            REQUIRE(S.rows() == 4); // 1 facet * 4
            REQUIRE(S.cols() == 9); // 3 vertices * 3
        }

        SECTION("global operator dimensions - pyramid")
        {
            polyddg::DifferentialOperators<Scalar, Index> ops(pyramid_mesh);
            auto S = ops.shape_operator();
            REQUIRE(S.rows() == 20); // 5 facets * 4
            REQUIRE(S.cols() == 15); // 5 vertices * 3
        }

        // ---- unit sphere ----

        SECTION("unit sphere")
        {
            auto sphere = lagrange::testing::create_test_sphere<Scalar, Index>(
                lagrange::testing::CreateOptions{false, false});
            polyddg::DifferentialOperators<Scalar, Index> ops(sphere);

            const Index num_vertices = sphere.get_num_vertices();
            const Index num_facets = sphere.get_num_facets();

            SECTION("symmetry at every facet")
            {
                for (Index fid = 0; fid < num_facets; fid++) {
                    Eigen::Matrix<Scalar, 2, 2> S = ops.shape_operator(fid);
                    REQUIRE_THAT(
                        (S - S.transpose()).norm(),
                        Catch::Matchers::WithinAbs(0.0, 1e-12));
                }
            }

            SECTION("mean curvature is positive at every facet")
            {
                // For a unit sphere with outward normals, H = trace(S) / 2 = 1 > 0.
                for (Index fid = 0; fid < num_facets; fid++) {
                    const Scalar H = ops.shape_operator(fid).trace() / Scalar(2);
                    REQUIRE(H > 0.0);
                }
            }

            SECTION("mean curvature is close to 1 at every facet")
            {
                // The icosphere is coarse, so a generous tolerance is needed.
                for (Index fid = 0; fid < num_facets; fid++) {
                    const Scalar H = ops.shape_operator(fid).trace() / Scalar(2);
                    REQUIRE_THAT(H, Catch::Matchers::WithinAbs(1.0, 0.5));
                }
            }

            SECTION("global operator is consistent with per-facet shape_operator(fid)")
            {
                // Build the flat vertex-normal input vector (size #V * 3).
                // shape_operator(fid) reads the raw stored vertex normals directly,
                // so we use the same attribute here for consistency.
                auto vertex_normals =
                    attribute_matrix_view<Scalar>(sphere, ops.get_vertex_normal_attribute_id());
                Eigen::Matrix<Scalar, Eigen::Dynamic, 1> normals_flat(num_vertices * 3);
                for (Index vid = 0; vid < num_vertices; vid++) {
                    normals_flat.template segment<3>(vid * 3) =
                        vertex_normals.row(vid).template head<3>().transpose();
                }

                Eigen::SparseMatrix<Scalar> S_global = ops.shape_operator();
                REQUIRE(S_global.rows() == static_cast<Eigen::Index>(num_facets * 4));
                REQUIRE(S_global.cols() == static_cast<Eigen::Index>(num_vertices * 3));

                Eigen::Matrix<Scalar, Eigen::Dynamic, 1> result = S_global * normals_flat;

                // Each 4-element block in result must match shape_operator(fid).
                // Layout per facet: [S(0,0), S(0,1), S(1,0), S(1,1)]
                for (Index fid = 0; fid < num_facets; fid++) {
                    Eigen::Matrix<Scalar, 2, 2> S_per_f = ops.shape_operator(fid);
                    Eigen::Matrix<Scalar, 2, 2> S_from_global;
                    S_from_global(0, 0) = result[fid * 4 + 0];
                    S_from_global(0, 1) = result[fid * 4 + 1];
                    S_from_global(1, 0) = result[fid * 4 + 2];
                    S_from_global(1, 1) = result[fid * 4 + 3];
                    REQUIRE_THAT(
                        (S_per_f - S_from_global).norm(),
                        Catch::Matchers::WithinAbs(0.0, 1e-10));
                }
            }
        }
    }

    SECTION("adjoint shape operator - unit sphere")
    {
        // Build the sphere and its differential operators.
        auto sphere = lagrange::testing::create_test_sphere<Scalar, Index>(
            lagrange::testing::CreateOptions{false, false});
        polyddg::DifferentialOperators<Scalar, Index> ops(sphere);

        const Index num_vertices = sphere.get_num_vertices();
        const Index num_facets = sphere.get_num_facets();

        // ---- per-vertex method ----

        SECTION("mean curvature is positive at every vertex")
        {
            for (Index vid = 0; vid < num_vertices; vid++) {
                Eigen::Matrix<Scalar, 2, 2> S = ops.adjoint_shape_operator(vid);

                // S must be symmetric.
                REQUIRE_THAT((S - S.transpose()).norm(), Catch::Matchers::WithinAbs(0.0, 1e-12));

                // Mean curvature H = trace(S) / 2.
                // For a unit sphere with outward normals H = 1 > 0.
                const Scalar H = S.trace() / Scalar(2);
                REQUIRE(H > 0.0);
            }
        }

        SECTION("mean curvature is close to 1 everywhere")
        {
            // The icosphere approximation of a unit sphere is coarse (42 vertices),
            // so we allow a generous tolerance.
            for (Index vid = 0; vid < num_vertices; vid++) {
                const Scalar H = ops.adjoint_shape_operator(vid).trace() / Scalar(2);
                REQUIRE_THAT(H, Catch::Matchers::WithinAbs(1.0, 0.5));
            }
        }

        // ---- global operator ----

        SECTION("global operator is consistent with per-vertex method")
        {
            // Build the face-normal input vector (size 3*#F): [n_f0^x, n_f0^y, n_f0^z, n_f1^x, ...]
            auto vec_area =
                attribute_matrix_view<Scalar>(sphere, ops.get_vector_area_attribute_id());
            Eigen::Matrix<Scalar, Eigen::Dynamic, 1> face_normals(num_facets * 3);
            for (Index fid = 0; fid < num_facets; fid++) {
                face_normals.template segment<3>(fid * 3) =
                    vec_area.row(fid).template head<3>().stableNormalized().transpose();
            }

            // Apply the global operator.
            Eigen::SparseMatrix<Scalar> S_global = ops.adjoint_shape_operator();
            REQUIRE(S_global.rows() == static_cast<Eigen::Index>(num_vertices * 4));
            REQUIRE(S_global.cols() == static_cast<Eigen::Index>(num_facets * 3));

            Eigen::Matrix<Scalar, Eigen::Dynamic, 1> result = S_global * face_normals;

            // Compare each per-vertex result to the per-vertex method.
            for (Index vid = 0; vid < num_vertices; vid++) {
                Eigen::Matrix<Scalar, 2, 2> S_per_v = ops.adjoint_shape_operator(vid);
                Eigen::Matrix<Scalar, 2, 2> S_from_global;
                // Row-major layout: [S(0,0), S(0,1), S(1,0), S(1,1)]
                S_from_global(0, 0) = result[vid * 4 + 0];
                S_from_global(0, 1) = result[vid * 4 + 1];
                S_from_global(1, 0) = result[vid * 4 + 2];
                S_from_global(1, 1) = result[vid * 4 + 3];
                REQUIRE_THAT(
                    (S_per_v - S_from_global).norm(),
                    Catch::Matchers::WithinAbs(0.0, 1e-10));
            }
        }

        // ---- adjoint gradient size and symmetry sanity ----

        SECTION("adjoint gradient has correct dimensions")
        {
            for (Index vid = 0; vid < num_vertices; vid++) {
                auto G_adj = ops.adjoint_gradient(vid);
                REQUIRE(G_adj.rows() == 3);
                REQUIRE(G_adj.cols() > 0); // every vertex has at least one incident face
            }

            Eigen::SparseMatrix<Scalar> G_adj_global = ops.adjoint_gradient();
            REQUIRE(G_adj_global.rows() == static_cast<Eigen::Index>(num_vertices * 3));
            REQUIRE(G_adj_global.cols() == static_cast<Eigen::Index>(num_facets));
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

    SECTION("Levi-Civita edge transport consistency converges on torus")
    {
        // For edge (v0, v1) shared by faces f0 and f1, the parallel transport
        // v0→f0→v1 should equal v0→f1→v1:
        //   R_{f0,v1}^T * R_{f0,v0}  ==  R_{f1,v1}^T * R_{f1,v0}
        // The inconsistency comes from composing shortest-arc rotations through
        // different face normals and converges to zero at O(h²) under refinement.
        auto compute_max_inconsistency = [](SurfaceMesh<Scalar, Index>& mesh_ref) {
            polyddg::DifferentialOperators<Scalar, Index> ops_ref(mesh_ref);
            Scalar max_inconsistency = 0;
            for (Index eid = 0; eid < mesh_ref.get_num_edges(); ++eid) {
                auto ev = mesh_ref.get_edge_vertices(eid);
                Index v0 = ev[0], v1 = ev[1];

                std::vector<Eigen::Matrix<Scalar, 2, 2>> Ts;
                mesh_ref.foreach_facet_around_edge(eid, [&](Index fid) {
                    Index f_size = mesh_ref.get_facet_size(fid);
                    Eigen::Matrix<Scalar, 2, 2> R0, R1;
                    for (Index lv = 0; lv < f_size; ++lv) {
                        if (mesh_ref.get_facet_vertex(fid, lv) == v0)
                            R0 = ops_ref.levi_civita(fid, lv);
                        else if (mesh_ref.get_facet_vertex(fid, lv) == v1)
                            R1 = ops_ref.levi_civita(fid, lv);
                    }
                    Ts.push_back(R1.transpose() * R0);
                });

                if (Ts.size() == 2) {
                    max_inconsistency = std::max(max_inconsistency, (Ts[0] - Ts[1]).norm());
                }
            }
            return max_inconsistency;
        };

        Scalar prev_inc = std::numeric_limits<Scalar>::max();
        for (int res : {20, 40, 80}) {
            primitive::TorusOptions fine_opts;
            fine_opts.major_radius = 3.0;
            fine_opts.minor_radius = 1.0;
            fine_opts.ring_segments = static_cast<size_t>(res * 3 / 2);
            fine_opts.pipe_segments = static_cast<size_t>(res);

            auto torus_quad = primitive::generate_torus<Scalar, Index>(fine_opts);
            Scalar inc_quad = compute_max_inconsistency(torus_quad);

            auto tri_mesh = primitive::generate_torus<Scalar, Index>(fine_opts);
            triangulate_polygonal_facets(tri_mesh);
            Scalar inc_tri = compute_max_inconsistency(tri_mesh);

            Scalar inc = std::max(inc_quad, inc_tri);
            INFO("res=" << res << ": quad=" << inc_quad << ", tri=" << inc_tri);
            REQUIRE(inc < prev_inc);
            prev_inc = inc;
        }
    }

    SECTION("vertex_basis orthogonal to vertex normal")
    {
        auto check_orthogonality = [](SurfaceMesh<Scalar, Index>& mesh) {
            polyddg::DifferentialOperators<Scalar, Index> diff_ops(mesh);
            auto vertex_normals =
                attribute_matrix_view<Scalar>(mesh, diff_ops.get_vertex_normal_attribute_id());

            for (Index vid = 0; vid < mesh.get_num_vertices(); vid++) {
                Eigen::Matrix<Scalar, 3, 2> B = diff_ops.vertex_basis(vid);
                Eigen::Matrix<Scalar, 3, 1> n =
                    vertex_normals.row(vid).template head<3>().stableNormalized();

                // Both tangent vectors must be orthogonal to the vertex normal.
                REQUIRE_THAT(std::abs(n.dot(B.col(0))), Catch::Matchers::WithinAbs(0.0, 1e-12));
                REQUIRE_THAT(std::abs(n.dot(B.col(1))), Catch::Matchers::WithinAbs(0.0, 1e-12));
            }
        };

        SECTION("triangle")
        {
            check_orthogonality(triangle_mesh);
        }
        SECTION("quad")
        {
            check_orthogonality(quad_mesh);
        }
        SECTION("pyramid")
        {
            check_orthogonality(pyramid_mesh);
        }
    }
}
