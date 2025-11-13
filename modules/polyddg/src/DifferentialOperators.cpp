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

#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/compute_area.h>
#include <lagrange/compute_centroid.h>
#include <lagrange/internal/find_attribute_utils.h>
#include <lagrange/polyddg/DifferentialOperators.h>
#include <lagrange/polyddg/api.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/invalid.h>
#include <lagrange/views.h>

#include <Eigen/Geometry>

#include <limits>
#include <vector>

namespace lagrange::polyddg {

namespace {

///
/// Compute bracket operator for a 3D vector.
///
/// @param[in] v Input vector (1x3).
///
/// @return A 3x3 skew-symmetric matrix corresponding to the bracket operator.
///
template <typename Scalar>
Eigen::Matrix<Scalar, 3, 3> bracket(const Eigen::Matrix<Scalar, 1, 3>& v)
{
    Eigen::Matrix<Scalar, 3, 3> m;
    m << 0, -v[2], v[1], v[2], 0, -v[0], -v[1], v[0], 0;
    return m;
}

///
/// Compute the Kronecker product of two matrices.
///
/// @param[in] A First input matrix.
/// @param[in] B Second input matrix.
///
/// @return The Kronecker product of A and B.
///
template <typename DerivedA, typename DerivedB>
Eigen::Matrix<typename DerivedA::Scalar, Eigen::Dynamic, Eigen::Dynamic> kronecker_product(
    const Eigen::MatrixBase<DerivedA>& A,
    const Eigen::MatrixBase<DerivedB>& B)
{
    using Scalar = typename DerivedA::Scalar;
    Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic> K(
        A.rows() * B.rows(),
        A.cols() * B.cols());
    for (Eigen::Index i = 0; i < A.rows(); i++) {
        for (Eigen::Index j = 0; j < A.cols(); j++) {
            K.block(i * B.rows(), j * B.cols(), B.rows(), B.cols()) = A(i, j) * B;
        }
    }
    return K;
}

///
/// Compute the Kronecker product of a sparse matrix A and a 2x2 identity matrix.
///
/// @param[in] A Input sparse matrix.
///
/// @return The Kronecker product of A and a 2x2 identity matrix.
///
template <typename Scalar>
Eigen::SparseMatrix<Scalar> kronecker_product_I2(const Eigen::SparseMatrix<Scalar>& A)
{
    std::vector<Eigen::Triplet<Scalar>> triplets;
    triplets.reserve(A.nonZeros() * 2);
    for (Eigen::Index i = 0; i < A.outerSize(); i++) {
        for (typename Eigen::SparseMatrix<Scalar>::InnerIterator it(A, i); it; ++it) {
            triplets.emplace_back(
                static_cast<Eigen::Index>(it.row() * 2),
                static_cast<Eigen::Index>(it.col() * 2),
                it.value());
            triplets.emplace_back(
                static_cast<Eigen::Index>(it.row() * 2 + 1),
                static_cast<Eigen::Index>(it.col() * 2 + 1),
                it.value());
        }
    }
    Eigen::SparseMatrix<Scalar> K(A.rows() * 2, A.cols() * 2);
    K.setFromTriplets(triplets.begin(), triplets.end());
    return K;
}

} // namespace

// Constructor
template <typename Scalar, typename Index>
DifferentialOperators<Scalar, Index>::DifferentialOperators(SurfaceMesh<Scalar, Index>& mesh)
    : m_mesh(mesh)
    , m_vector_area_id(invalid<AttributeId>())
    , m_centroid_id(invalid<AttributeId>())
{
    la_runtime_assert(m_mesh.get_dimension() == 3, "Only 3D meshes are supported.");
    m_mesh.initialize_edges();

    // Precompute vector area and centroid
    m_vector_area_id = compute_facet_vector_area(m_mesh);
    m_centroid_id = compute_facet_centroid(m_mesh);

    compute_vertex_normal_from_vector_area();
}

// Gradient operator
template <typename Scalar, typename Index>
Eigen::SparseMatrix<Scalar> DifferentialOperators<Scalar, Index>::gradient() const
{
    const Index num_vertices = m_mesh.get_num_vertices();
    const Index num_facets = m_mesh.get_num_facets();
    const Index num_corners = m_mesh.get_num_corners();

    auto vertices = vertex_view(m_mesh);
    auto vec_area = attribute_matrix_view<Scalar>(m_mesh, m_vector_area_id);

    // Implementation is based on equation 8 from [1].
    //
    // [1]: De Goes, Fernando, Andrew Butts, and Mathieu Desbrun. "Discrete differential operators
    // on polygonal meshes." ACM Transactions on Graphics (TOG) 39.4 (2020): 110-1.
    std::vector<Eigen::Triplet<Scalar>> entries(num_corners * 3);
    for (Index fid = 0; fid < num_facets; fid++) {
        const Index facet_size = m_mesh.get_facet_size(fid);
        const Vector a = vec_area.row(fid).template head<3>();
        const Scalar area_sq = a.squaredNorm();
        const Index c_begin = m_mesh.get_facet_corner_begin(fid);

        for (Index lv = 0; lv < facet_size; lv++) {
            const Index cid = c_begin + lv;
            const Index vid = m_mesh.get_facet_vertex(fid, lv);
            const Index vid_next = m_mesh.get_facet_vertex(fid, (lv + 1) % facet_size);
            const Index vid_prev = m_mesh.get_facet_vertex(fid, (lv + facet_size - 1) % facet_size);
            Vector e = (vertices.row(vid_prev) - vertices.row(vid_next)).template head<3>();
            Vector g = a.cross(e) / (2 * area_sq);

            entries[cid * 3] = Eigen::Triplet<Scalar>(
                static_cast<Eigen::Index>(fid * 3),
                static_cast<Eigen::Index>(vid),
                g[0]);
            entries[cid * 3 + 1] = Eigen::Triplet<Scalar>(
                static_cast<Eigen::Index>(fid * 3 + 1),
                static_cast<Eigen::Index>(vid),
                g[1]);
            entries[cid * 3 + 2] = Eigen::Triplet<Scalar>(
                static_cast<Eigen::Index>(fid * 3 + 2),
                static_cast<Eigen::Index>(vid),
                g[2]);
        }
    }

    Eigen::SparseMatrix<Scalar> G(num_facets * 3, num_vertices);
    G.setFromTriplets(entries.begin(), entries.end());
    return G;
}

// d0 operator
template <typename Scalar, typename Index>
Eigen::SparseMatrix<Scalar> DifferentialOperators<Scalar, Index>::d0() const
{
    const Index num_vertices = m_mesh.get_num_vertices();
    const Index num_edges = m_mesh.get_num_edges();

    std::vector<Eigen::Triplet<Scalar>> entries(num_edges * 2);
    for (Index eid = 0; eid < num_edges; eid++) {
        auto [v0, v1] = m_mesh.get_edge_vertices(eid);
        entries[2 * eid] = Eigen::Triplet<Scalar>(
            static_cast<Eigen::Index>(eid),
            static_cast<Eigen::Index>(v0),
            -1);
        entries[2 * eid + 1] = Eigen::Triplet<Scalar>(
            static_cast<Eigen::Index>(eid),
            static_cast<Eigen::Index>(v1),
            1);
    }

    Eigen::SparseMatrix<Scalar> D(num_edges, num_vertices);
    D.setFromTriplets(entries.begin(), entries.end());
    return D;
}

// d1 operator
template <typename Scalar, typename Index>
Eigen::SparseMatrix<Scalar> DifferentialOperators<Scalar, Index>::d1() const
{
    const Index num_edges = m_mesh.get_num_edges();
    const Index num_facets = m_mesh.get_num_facets();
    const Index num_corners = m_mesh.get_num_corners();

    std::vector<Eigen::Triplet<Scalar>> entries(num_corners);
    for (Index fid = 0; fid < num_facets; fid++) {
        const Index facet_size = m_mesh.get_facet_size(fid);
        const Index c_begin = m_mesh.get_facet_corner_begin(fid);

        for (Index lv = 0; lv < facet_size; lv++) {
            const Index vid = m_mesh.get_facet_vertex(fid, lv);
            const Index eid = m_mesh.get_edge(fid, lv);
            const Index cid = c_begin + lv;
            auto [v0, v1] = m_mesh.get_edge_vertices(eid);
            bool orientation = (v0 == vid);
            la_debug_assert(orientation || v1 == vid, "Inconsistent edge orientation");

            entries[cid] = Eigen::Triplet<Scalar>(
                static_cast<Eigen::Index>(fid),
                static_cast<Eigen::Index>(eid),
                orientation ? 1 : -1);
        }
    }

    Eigen::SparseMatrix<Scalar> D(num_facets, num_edges);
    D.setFromTriplets(entries.begin(), entries.end());
    return D;
}

// star0 operator
template <typename Scalar, typename Index>
Eigen::SparseMatrix<Scalar> DifferentialOperators<Scalar, Index>::star0() const
{
    return inner_product_0_form();
}

// star1 operator
template <typename Scalar, typename Index>
Eigen::SparseMatrix<Scalar> DifferentialOperators<Scalar, Index>::star1() const
{
    const Index num_edges = m_mesh.get_num_edges();

    auto vertices = vertex_view(m_mesh);
    auto facet_centroids = attribute_matrix_view<Scalar>(m_mesh, m_centroid_id);

    std::vector<Eigen::Triplet<Scalar>> entries(num_edges);
    for (Index eid = 0; eid < num_edges; eid++) {
        auto [v0, v1] = m_mesh.get_edge_vertices(eid);
        Scalar primal_edge_length = (vertices.row(v1) - vertices.row(v0)).norm();

        Vector c0, c1;

        auto cid = m_mesh.get_first_corner_around_edge(eid);
        la_debug_assert(cid != invalid<Index>(), "Invalid corner index for boundary edge.");
        auto fid = m_mesh.get_corner_facet(cid);
        c0 = facet_centroids.row(fid).template head<3>();
        auto edge_valence = m_mesh.count_num_corners_around_edge(eid);
        la_debug_assert(edge_valence > 0, "Edge valence must be positive.");

        if (edge_valence == 1) {
            c1 = (vertices.row(v0) + vertices.row(v1)) / 2;
        } else if (edge_valence == 2) {
            auto cid2 = m_mesh.get_next_corner_around_edge(cid);
            auto fid2 = m_mesh.get_corner_facet(cid2);
            c1 = facet_centroids.row(fid2).template head<3>();
        } else {
            throw std::runtime_error("star1 is only implemented for manifold meshes.");
        }
        Scalar dual_edge_length = (c1 - c0).norm();
        entries[eid] = Eigen::Triplet<Scalar>(
            static_cast<Eigen::Index>(eid),
            static_cast<Eigen::Index>(eid),
            dual_edge_length / primal_edge_length);
    }

    Eigen::SparseMatrix<Scalar> M(num_edges, num_edges);
    M.setFromTriplets(entries.begin(), entries.end());
    return M;
}

// star2 operator
template <typename Scalar, typename Index>
Eigen::SparseMatrix<Scalar> DifferentialOperators<Scalar, Index>::star2() const
{
    return inner_product_2_form();
}


// flat operator
template <typename Scalar, typename Index>
Eigen::SparseMatrix<Scalar> DifferentialOperators<Scalar, Index>::flat() const
{
    const Eigen::Matrix<Scalar, 3, 3> I = Eigen::Matrix<Scalar, 3, 3>::Identity();

    auto vertices = vertex_view(m_mesh);
    const Index num_edges = m_mesh.get_num_edges();
    const Index num_facets = m_mesh.get_num_facets();
    const Index num_corners = m_mesh.get_num_corners();

    auto vec_area = attribute_matrix_view<Scalar>(m_mesh, m_vector_area_id);

    std::vector<Eigen::Triplet<Scalar>> entries(num_corners * 3);
    for (Index fid = 0; fid < num_facets; fid++) {
        Index facet_size = m_mesh.get_facet_size(fid);
        const Vector a = vec_area.row(fid).template head<3>();
        const Scalar area_sq = a.squaredNorm();
        const Index c_begin = m_mesh.get_facet_corner_begin(fid);

        for (Index lv = 0; lv < facet_size; lv++) {
            const Index cid = c_begin + lv;
            const Index eid = m_mesh.get_edge(fid, lv);
            const Index edge_valence = m_mesh.count_num_corners_around_edge(eid);
            auto [v0, v1] = m_mesh.get_edge_vertices(eid);
            Vector v = (I - a.transpose() * a / area_sq) *
                       (vertices.row(v1) - vertices.row(v0)).transpose().template head<3>() /
                       edge_valence;
            entries[cid * 3] = Eigen::Triplet<Scalar>(
                static_cast<Eigen::Index>(eid),
                static_cast<Eigen::Index>(fid * 3 + 0),
                v[0]);
            entries[cid * 3 + 1] = Eigen::Triplet<Scalar>(
                static_cast<Eigen::Index>(eid),
                static_cast<Eigen::Index>(fid * 3 + 1),
                v[1]);
            entries[cid * 3 + 2] = Eigen::Triplet<Scalar>(
                static_cast<Eigen::Index>(eid),
                static_cast<Eigen::Index>(fid * 3 + 2),
                v[2]);
        }
    }

    Eigen::SparseMatrix<Scalar> V(num_edges, num_facets * 3);
    V.setFromTriplets(entries.begin(), entries.end());
    return V;
}

// sharp operator
template <typename Scalar, typename Index>
Eigen::SparseMatrix<Scalar> DifferentialOperators<Scalar, Index>::sharp() const
{
    auto vertices = vertex_view(m_mesh);
    const Index num_edges = m_mesh.get_num_edges();
    const Index num_facets = m_mesh.get_num_facets();
    const Index num_corners = m_mesh.get_num_corners();

    auto vec_area = attribute_matrix_view<Scalar>(m_mesh, m_vector_area_id);
    auto facet_centroids = attribute_matrix_view<Scalar>(m_mesh, m_centroid_id);

    std::vector<Eigen::Triplet<Scalar>> entries(num_corners * 3);
    for (Index fid = 0; fid < num_facets; fid++) {
        Index facet_size = m_mesh.get_facet_size(fid);
        const Vector a = vec_area.row(fid).template head<3>();
        const Vector c = facet_centroids.row(fid).template head<3>();
        const Scalar area_sq = a.squaredNorm();
        const Index c_begin = m_mesh.get_facet_corner_begin(fid);

        for (Index lv = 0; lv < facet_size; lv++) {
            Index cid = c_begin + lv;
            Index eid = m_mesh.get_corner_edge(cid);
            auto [v0, v1] = m_mesh.get_edge_vertices(eid);
            bool orientation = (v0 == m_mesh.get_facet_vertex(fid, lv));

            Vector e_mid = ((vertices.row(v0) + vertices.row(v1)) / 2).template head<3>();
            Vector u = a.cross(Vector(e_mid - c)) / area_sq * (orientation ? 1 : -1);
            entries[cid * 3] = Eigen::Triplet<Scalar>(
                static_cast<Eigen::Index>(fid * 3),
                static_cast<Eigen::Index>(eid),
                u[0]);
            entries[cid * 3 + 1] = Eigen::Triplet<Scalar>(
                static_cast<Eigen::Index>(fid * 3 + 1),
                static_cast<Eigen::Index>(eid),
                u[1]);
            entries[cid * 3 + 2] = Eigen::Triplet<Scalar>(
                static_cast<Eigen::Index>(fid * 3 + 2),
                static_cast<Eigen::Index>(eid),
                u[2]);
        }
    }

    Eigen::SparseMatrix<Scalar> U(num_facets * 3, num_edges);
    U.setFromTriplets(entries.begin(), entries.end());
    return U;
}

// projection operator
template <typename Scalar, typename Index>
Eigen::SparseMatrix<Scalar> DifferentialOperators<Scalar, Index>::projection() const
{
    Index num_edges = m_mesh.get_num_edges();
    Eigen::SparseMatrix<Scalar> I(num_edges, num_edges);
    I.setIdentity();

    auto U = sharp();
    auto V = flat();
    return I - V * U;
}

// inner_product_0_form
template <typename Scalar, typename Index>
Eigen::SparseMatrix<Scalar> DifferentialOperators<Scalar, Index>::inner_product_0_form() const
{
    auto vec_area = attribute_matrix_view<Scalar>(m_mesh, m_vector_area_id);

    const Index num_vertices = m_mesh.get_num_vertices();
    const Index num_facets = m_mesh.get_num_facets();
    const Index num_corners = m_mesh.get_num_corners();

    std::vector<Eigen::Triplet<Scalar>> entries(num_corners);
    for (Index fid = 0; fid < num_facets; fid++) {
        const Scalar area = vec_area.row(fid).norm();
        const Index facet_size = m_mesh.get_facet_size(fid);
        const Index c_begin = m_mesh.get_facet_corner_begin(fid);

        for (Index lv = 0; lv < facet_size; lv++) {
            const Index vid = m_mesh.get_facet_vertex(fid, lv);
            const Index cid = c_begin + lv;

            entries[cid] = Eigen::Triplet<Scalar>(
                static_cast<Eigen::Index>(vid),
                static_cast<Eigen::Index>(vid),
                area / facet_size);
        }
    }

    Eigen::SparseMatrix<Scalar> M(num_vertices, num_vertices);
    M.setFromTriplets(entries.begin(), entries.end());
    return M;
}

// inner_product_1_form
template <typename Scalar, typename Index>
Eigen::SparseMatrix<Scalar> DifferentialOperators<Scalar, Index>::inner_product_1_form(
    Scalar lambda) const
{
    auto vec_area = attribute_matrix_view<Scalar>(m_mesh, m_vector_area_id);
    const Index num_edges = m_mesh.get_num_edges();
    const Index num_facets = m_mesh.get_num_facets();
    std::vector<Eigen::Triplet<Scalar>> entries;
    Index total_triplets = 0;
    for (Index fid = 0; fid < num_facets; fid++) {
        total_triplets += m_mesh.get_facet_size(fid) * m_mesh.get_facet_size(fid);
    }
    entries.reserve(total_triplets);

    for (Index fid = 0; fid < num_facets; fid++) {
        const Index facet_size = m_mesh.get_facet_size(fid);
        const Scalar area = vec_area.row(fid).norm();
        auto Uf = sharp(fid);
        auto Pf = projection(fid);

        auto Mf = area * Uf.transpose() * Uf + lambda * Pf.transpose() * Pf;
        for (Index lv = 0; lv < facet_size; lv++) {
            Index eid = m_mesh.get_edge(fid, lv);
            for (Index lv2 = 0; lv2 < facet_size; lv2++) {
                Index eid2 = m_mesh.get_edge(fid, lv2);
                entries.push_back(
                    Eigen::Triplet<Scalar>(
                        static_cast<Eigen::Index>(eid),
                        static_cast<Eigen::Index>(eid2),
                        Mf(lv, lv2)));
            }
        }
    }

    Eigen::SparseMatrix<Scalar> M(num_edges, num_edges);
    M.setFromTriplets(entries.begin(), entries.end());
    return M;
}

// inner_product_2_form
template <typename Scalar, typename Index>
Eigen::SparseMatrix<Scalar> DifferentialOperators<Scalar, Index>::inner_product_2_form() const
{
    auto vec_area = attribute_matrix_view<Scalar>(m_mesh, m_vector_area_id);

    const Index num_facets = m_mesh.get_num_facets();

    std::vector<Eigen::Triplet<Scalar>> entries(num_facets);
    for (Index fid = 0; fid < num_facets; fid++) {
        const Scalar area = vec_area.row(fid).norm();
        entries[fid] = Eigen::Triplet<Scalar>(
            static_cast<Eigen::Index>(fid),
            static_cast<Eigen::Index>(fid),
            Scalar(1) / area);
    }

    Eigen::SparseMatrix<Scalar> M(num_facets, num_facets);
    M.setFromTriplets(entries.begin(), entries.end());
    return M;
}

// Divergence operator
template <typename Scalar, typename Index>
Eigen::SparseMatrix<Scalar> DifferentialOperators<Scalar, Index>::divergence(Scalar lambda) const
{
    auto D0 = d0();
    auto M = inner_product_1_form(lambda);
    return D0.transpose() * M;
}

// Curl operator
template <typename Scalar, typename Index>
Eigen::SparseMatrix<Scalar> DifferentialOperators<Scalar, Index>::curl() const
{
    Index num_corners = m_mesh.get_num_corners();
    Index num_edges = m_mesh.get_num_edges();
    Index num_facets = m_mesh.get_num_facets();

    std::vector<Eigen::Triplet<Scalar>> entries(num_corners);
    for (Index fid = 0; fid < num_facets; fid++) {
        Index facet_size = m_mesh.get_facet_size(fid);
        Index c_begin = m_mesh.get_facet_corner_begin(fid);
        for (Index lv = 0; lv < facet_size; lv++) {
            Index eid = m_mesh.get_edge(fid, lv);
            Index cid = c_begin + lv;
            auto [v0, v1] = m_mesh.get_edge_vertices(eid);
            bool orientation = (v0 == m_mesh.get_facet_vertex(fid, lv));

            entries[cid] = Eigen::Triplet<Scalar>(
                static_cast<Eigen::Index>(fid),
                static_cast<Eigen::Index>(eid),
                orientation ? 1 : -1);
        }
    }

    Eigen::SparseMatrix<Scalar> C(num_facets, num_edges);
    C.setFromTriplets(entries.begin(), entries.end());
    return C;
}

// laplacian
template <typename Scalar, typename Index>
Eigen::SparseMatrix<Scalar> DifferentialOperators<Scalar, Index>::laplacian(Scalar lambda) const
{
    auto D0 = d0();
    auto M = inner_product_1_form(lambda);
    return D0.transpose() * M * D0;
}

// Vertex tangent bases
template <typename Scalar, typename Index>
Eigen::SparseMatrix<Scalar> DifferentialOperators<Scalar, Index>::vertex_tangent_coordinates() const
{
    const Index num_vertices = m_mesh.get_num_vertices();
    std::vector<Eigen::Triplet<Scalar>> entries(num_vertices * 6);
    for (Index i = 0; i < num_vertices; i++) {
        auto Bv = vertex_basis(i);
        entries[i * 6] = Eigen::Triplet<Scalar>(
            static_cast<Eigen::Index>(i * 2),
            static_cast<Eigen::Index>(i * 3),
            Bv(0, 0));
        entries[i * 6 + 1] = Eigen::Triplet<Scalar>(
            static_cast<Eigen::Index>(i * 2),
            static_cast<Eigen::Index>(i * 3 + 1),
            Bv(1, 0));
        entries[i * 6 + 2] = Eigen::Triplet<Scalar>(
            static_cast<Eigen::Index>(i * 2),
            static_cast<Eigen::Index>(i * 3 + 2),
            Bv(2, 0));
        entries[i * 6 + 3] = Eigen::Triplet<Scalar>(
            static_cast<Eigen::Index>(i * 2 + 1),
            static_cast<Eigen::Index>(i * 3),
            Bv(0, 1));
        entries[i * 6 + 4] = Eigen::Triplet<Scalar>(
            static_cast<Eigen::Index>(i * 2 + 1),
            static_cast<Eigen::Index>(i * 3 + 1),
            Bv(1, 1));
        entries[i * 6 + 5] = Eigen::Triplet<Scalar>(
            static_cast<Eigen::Index>(i * 2 + 1),
            static_cast<Eigen::Index>(i * 3 + 2),
            Bv(2, 1));
    }

    Eigen::SparseMatrix<Scalar> B(num_vertices * 2, num_vertices * 3);
    B.setFromTriplets(entries.begin(), entries.end());
    return B;
}

// Facet tangent bases
template <typename Scalar, typename Index>
Eigen::SparseMatrix<Scalar> DifferentialOperators<Scalar, Index>::facet_tangent_coordinates() const
{
    const Index num_facets = m_mesh.get_num_facets();
    std::vector<Eigen::Triplet<Scalar>> entries(num_facets * 6);
    for (Index i = 0; i < num_facets; i++) {
        auto Bf = facet_basis(i);
        entries[i * 6] = Eigen::Triplet<Scalar>(
            static_cast<Eigen::Index>(i * 2),
            static_cast<Eigen::Index>(i * 3),
            Bf(0, 0));
        entries[i * 6 + 1] = Eigen::Triplet<Scalar>(
            static_cast<Eigen::Index>(i * 2),
            static_cast<Eigen::Index>(i * 3 + 1),
            Bf(1, 0));
        entries[i * 6 + 2] = Eigen::Triplet<Scalar>(
            static_cast<Eigen::Index>(i * 2),
            static_cast<Eigen::Index>(i * 3 + 2),
            Bf(2, 0));
        entries[i * 6 + 3] = Eigen::Triplet<Scalar>(
            static_cast<Eigen::Index>(i * 2 + 1),
            static_cast<Eigen::Index>(i * 3),
            Bf(0, 1));
        entries[i * 6 + 4] = Eigen::Triplet<Scalar>(
            static_cast<Eigen::Index>(i * 2 + 1),
            static_cast<Eigen::Index>(i * 3 + 1),
            Bf(1, 1));
        entries[i * 6 + 5] = Eigen::Triplet<Scalar>(
            static_cast<Eigen::Index>(i * 2 + 1),
            static_cast<Eigen::Index>(i * 3 + 2),
            Bf(2, 1));
    }

    Eigen::SparseMatrix<Scalar> B(num_facets * 2, num_facets * 3);
    B.setFromTriplets(entries.begin(), entries.end());
    return B;
}

// Levi-Civita connection
template <typename Scalar, typename Index>
Eigen::SparseMatrix<Scalar> DifferentialOperators<Scalar, Index>::levi_civita() const
{
    return levi_civita_nrosy(1);
}

// Levi-Civita connection for n-rosy fields
template <typename Scalar, typename Index>
Eigen::SparseMatrix<Scalar> DifferentialOperators<Scalar, Index>::levi_civita_nrosy(Index n) const
{
    const Index num_vertices = m_mesh.get_num_vertices();
    const Index num_facets = m_mesh.get_num_facets();
    const Index num_corners = m_mesh.get_num_corners();

    std::vector<Eigen::Triplet<Scalar>> entries(num_corners * 4);

    for (Index fid = 0; fid < num_facets; fid++) {
        const Index facet_size = m_mesh.get_facet_size(fid);
        const Index c_begin = m_mesh.get_facet_corner_begin(fid);
        for (Index lv = 0; lv < facet_size; lv++) {
            Index cid = c_begin + lv;
            Index vid = m_mesh.get_facet_vertex(fid, lv);
            auto R = levi_civita_nrosy(fid, lv, n);

            entries[cid * 4] = Eigen::Triplet<Scalar>(
                static_cast<Eigen::Index>(cid * 2),
                static_cast<Eigen::Index>(vid * 2),
                R(0, 0));
            entries[cid * 4 + 1] = Eigen::Triplet<Scalar>(
                static_cast<Eigen::Index>(cid * 2),
                static_cast<Eigen::Index>(vid * 2 + 1),
                R(0, 1));
            entries[cid * 4 + 2] = Eigen::Triplet<Scalar>(
                static_cast<Eigen::Index>(cid * 2 + 1),
                static_cast<Eigen::Index>(vid * 2),
                R(1, 0));
            entries[cid * 4 + 3] = Eigen::Triplet<Scalar>(
                static_cast<Eigen::Index>(cid * 2 + 1),
                static_cast<Eigen::Index>(vid * 2 + 1),
                R(1, 1));
        }
    }

    Eigen::SparseMatrix<Scalar> C(num_corners * 2, num_vertices * 2);
    C.setFromTriplets(entries.begin(), entries.end());
    return C;
}

// Covariant derivative
template <typename Scalar, typename Index>
Eigen::SparseMatrix<Scalar> DifferentialOperators<Scalar, Index>::covariant_derivative() const
{
    return covariant_derivative_nrosy(1);
}

// Covariant derivative for n-rosy fields
template <typename Scalar, typename Index>
Eigen::SparseMatrix<Scalar> DifferentialOperators<Scalar, Index>::covariant_derivative_nrosy(
    Index n) const
{
    const Index num_vertices = m_mesh.get_num_vertices();
    const Index num_facets = m_mesh.get_num_facets();
    const Index num_corners = m_mesh.get_num_corners();

    std::vector<Eigen::Triplet<Scalar>> entries(num_corners * 8);
    for (Index fid = 0; fid < num_facets; fid++) {
        auto G_cov = covariant_derivative_nrosy(fid, n);
        Index facet_size = m_mesh.get_facet_size(fid);
        Index c_begin = m_mesh.get_facet_corner_begin(fid);
        for (Index lv = 0; lv < facet_size; lv++) {
            Index vid = m_mesh.get_facet_vertex(fid, lv);
            Index cid = c_begin + lv;

            entries[cid * 8] = Eigen::Triplet<Scalar>(
                static_cast<Eigen::Index>(fid * 4),
                static_cast<Eigen::Index>(vid * 2),
                G_cov(0, lv * 2));
            entries[cid * 8 + 1] = Eigen::Triplet<Scalar>(
                static_cast<Eigen::Index>(fid * 4),
                static_cast<Eigen::Index>(vid * 2 + 1),
                G_cov(0, lv * 2 + 1));
            entries[cid * 8 + 2] = Eigen::Triplet<Scalar>(
                static_cast<Eigen::Index>(fid * 4 + 1),
                static_cast<Eigen::Index>(vid * 2),
                G_cov(1, lv * 2));
            entries[cid * 8 + 3] = Eigen::Triplet<Scalar>(
                static_cast<Eigen::Index>(fid * 4 + 1),
                static_cast<Eigen::Index>(vid * 2 + 1),
                G_cov(1, lv * 2 + 1));
            entries[cid * 8 + 4] = Eigen::Triplet<Scalar>(
                static_cast<Eigen::Index>(fid * 4 + 2),
                static_cast<Eigen::Index>(vid * 2),
                G_cov(2, lv * 2));
            entries[cid * 8 + 5] = Eigen::Triplet<Scalar>(
                static_cast<Eigen::Index>(fid * 4 + 2),
                static_cast<Eigen::Index>(vid * 2 + 1),
                G_cov(2, lv * 2 + 1));
            entries[cid * 8 + 6] = Eigen::Triplet<Scalar>(
                static_cast<Eigen::Index>(fid * 4 + 3),
                static_cast<Eigen::Index>(vid * 2),
                G_cov(3, lv * 2));
            entries[cid * 8 + 7] = Eigen::Triplet<Scalar>(
                static_cast<Eigen::Index>(fid * 4 + 3),
                static_cast<Eigen::Index>(vid * 2 + 1),
                G_cov(3, lv * 2 + 1));
        }
    }

    Eigen::SparseMatrix<Scalar> G_cov(num_facets * 4, num_vertices * 2);
    G_cov.setFromTriplets(entries.begin(), entries.end());
    return G_cov;
}

// Connection Laplacian
template <typename Scalar, typename Index>
Eigen::SparseMatrix<Scalar> DifferentialOperators<Scalar, Index>::connection_laplacian(
    Scalar lambda) const
{
    return connection_laplacian_nrosy(1, lambda);
}

// Connection Laplacian for n-rosy fields
template <typename Scalar, typename Index>
Eigen::SparseMatrix<Scalar> DifferentialOperators<Scalar, Index>::connection_laplacian_nrosy(
    Index n,
    Scalar lambda) const
{
    auto vec_area = attribute_matrix_view<Scalar>(m_mesh, m_vector_area_id);

    Index num_vertices = m_mesh.get_num_vertices();
    Index num_facets = m_mesh.get_num_facets();

    std::vector<Eigen::Triplet<Scalar>> entries;
    size_t total_triplets = 0;
    for (Index fid = 0; fid < num_facets; fid++) {
        Index facet_size = m_mesh.get_facet_size(fid);
        total_triplets += 4 * facet_size * facet_size;
    }
    entries.reserve(total_triplets);

    for (Index fid = 0; fid < num_facets; fid++) {
        Scalar a = vec_area.row(fid).norm();
        auto G_cov = covariant_derivative_nrosy(fid, n);
        auto P_cov = covariant_projection_nrosy(fid, n);
        auto L_c = a * G_cov.transpose() * G_cov + lambda * P_cov.transpose() * P_cov;

        Index facet_size = m_mesh.get_facet_size(fid);
        for (Index lv0 = 0; lv0 < facet_size; lv0++) {
            Index v0 = m_mesh.get_facet_vertex(fid, lv0);
            for (Index lv1 = 0; lv1 < facet_size; lv1++) {
                Index v1 = m_mesh.get_facet_vertex(fid, lv1);
                entries.emplace_back(
                    static_cast<Eigen::Index>(v0 * 2),
                    static_cast<Eigen::Index>(v1 * 2),
                    L_c(lv0 * 2, lv1 * 2));
                entries.emplace_back(
                    static_cast<Eigen::Index>(v0 * 2),
                    static_cast<Eigen::Index>(v1 * 2 + 1),
                    L_c(lv0 * 2, lv1 * 2 + 1));
                entries.emplace_back(
                    static_cast<Eigen::Index>(v0 * 2 + 1),
                    static_cast<Eigen::Index>(v1 * 2),
                    L_c(lv0 * 2 + 1, lv1 * 2));
                entries.emplace_back(
                    static_cast<Eigen::Index>(v0 * 2 + 1),
                    static_cast<Eigen::Index>(v1 * 2 + 1),
                    L_c(lv0 * 2 + 1, lv1 * 2 + 1));
            }
        }
    }

    Eigen::SparseMatrix<Scalar> Lc(num_vertices * 2, num_vertices * 2);
    Lc.setFromTriplets(entries.begin(), entries.end());
    return Lc;
}

// Per-facet gradient operator
template <typename Scalar, typename Index>
Eigen::Matrix<Scalar, 3, Eigen::Dynamic> DifferentialOperators<Scalar, Index>::gradient(
    Index fid) const
{
    auto vertices = vertex_view(m_mesh);
    auto vec_area = attribute_matrix_view<Scalar>(m_mesh, m_vector_area_id);

    const Index facet_size = m_mesh.get_facet_size(fid);
    const Vector a = vec_area.row(fid).template head<3>();
    const Scalar area_sq = a.squaredNorm();

    Eigen::Matrix<Scalar, 3, Eigen::Dynamic> G(3, facet_size);

    for (Index lv = 0; lv < facet_size; lv++) {
        const Index vid_next = m_mesh.get_facet_vertex(fid, (lv + 1) % facet_size);
        const Index vid_prev = m_mesh.get_facet_vertex(fid, (lv + facet_size - 1) % facet_size);
        Vector e = (vertices.row(vid_prev) - vertices.row(vid_next)).template head<3>();
        Vector g = a.cross(e) / (2 * area_sq);

        G.col(lv) = g.transpose();
    }

    return G;
}

// Per-facet d0 operator
template <typename Scalar, typename Index>
Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic> DifferentialOperators<Scalar, Index>::d0(
    Index fid) const
{
    const Index facet_size = m_mesh.get_facet_size(fid);

    Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic> D0(facet_size, facet_size);
    D0.setZero();

    for (Index lv = 0; lv < facet_size; lv++) {
        const Index vid = m_mesh.get_facet_vertex(fid, lv);
        const Index eid = m_mesh.get_edge(fid, lv);
        auto [v0, v1] = m_mesh.get_edge_vertices(eid);

        // Determine orientation: is the edge going from current vertex or to current vertex?
        bool orientation = (v0 == vid);
        la_debug_assert(orientation || v1 == vid, "Inconsistent edge orientation");

        // Find local vertex indices for v0 and v1
        Index lv0 = lv;
        Index lv1 = (lv + 1) % facet_size;

        if (!orientation) {
            std::swap(lv0, lv1);
        }

        // d0 is the incidence matrix: edge value = vertex_value[v1] - vertex_value[v0]
        D0(lv, lv0) = -1;
        D0(lv, lv1) = 1;
    }

    return D0;
}

// Per-facet d1 operator
template <typename Scalar, typename Index>
Eigen::Matrix<Scalar, 1, Eigen::Dynamic> DifferentialOperators<Scalar, Index>::d1(Index fid) const
{
    const Index facet_size = m_mesh.get_facet_size(fid);

    Eigen::Matrix<Scalar, 1, Eigen::Dynamic> D1(1, facet_size);

    for (Index lv = 0; lv < facet_size; lv++) {
        const Index vid = m_mesh.get_facet_vertex(fid, lv);
        const Index eid = m_mesh.get_edge(fid, lv);
        auto [v0, v1] = m_mesh.get_edge_vertices(eid);
        bool orientation = (v0 == vid);
        la_debug_assert(orientation || v1 == vid, "Inconsistent edge orientation");

        // d1 sums up the 1-form values with proper orientation
        D1(0, lv) = orientation ? 1 : -1;
    }

    return D1;
}

// Per-facet flat operator
template <typename Scalar, typename Index>
Eigen::Matrix<Scalar, Eigen::Dynamic, 3> DifferentialOperators<Scalar, Index>::flat(Index fid) const
{
    const Eigen::Matrix<Scalar, 3, 3> I = Eigen::Matrix<Scalar, 3, 3>::Identity();

    auto vertices = vertex_view(m_mesh);
    auto vec_area = attribute_matrix_view<Scalar>(m_mesh, m_vector_area_id);

    const Index facet_size = m_mesh.get_facet_size(fid);
    const Vector a = vec_area.row(fid).template head<3>();
    const Scalar area_sq = a.squaredNorm();

    Eigen::Matrix<Scalar, Eigen::Dynamic, 3> V(facet_size, 3);

    for (Index lv = 0; lv < facet_size; lv++) {
        const Index eid = m_mesh.get_edge(fid, lv);
        auto [v0, v1] = m_mesh.get_edge_vertices(eid);

        Vector v = (I - a.transpose() * a / area_sq) *
                   (vertices.row(v1) - vertices.row(v0)).transpose().template head<3>();
        V.row(lv) = v;
    }

    return V;
}

// Per-facet sharp operator
template <typename Scalar, typename Index>
Eigen::Matrix<Scalar, 3, Eigen::Dynamic> DifferentialOperators<Scalar, Index>::sharp(
    Index fid) const
{
    auto vertices = vertex_view(m_mesh);
    auto vec_area = attribute_matrix_view<Scalar>(m_mesh, m_vector_area_id);
    auto facet_centroids = attribute_matrix_view<Scalar>(m_mesh, m_centroid_id);

    const Index facet_size = m_mesh.get_facet_size(fid);
    const Vector a = vec_area.row(fid).template head<3>();
    const Vector c = facet_centroids.row(fid).template head<3>();
    const Scalar area_sq = a.squaredNorm();
    const Index c_begin = m_mesh.get_facet_corner_begin(fid);

    Eigen::Matrix<Scalar, 3, Eigen::Dynamic> U(3, facet_size);

    for (Index lv = 0; lv < facet_size; lv++) {
        Index cid = c_begin + lv;
        Index eid = m_mesh.get_corner_edge(cid);
        auto [v0, v1] = m_mesh.get_edge_vertices(eid);
        bool orientation = (v0 == m_mesh.get_facet_vertex(fid, lv));

        Vector e_mid = ((vertices.row(v0) + vertices.row(v1)) / 2).template head<3>();
        Vector u = a.cross(Vector(e_mid - c)) / area_sq * (orientation ? 1 : -1);
        U.col(lv) = u.transpose();
    }

    return U;
}

// Per-facet projection operator
template <typename Scalar, typename Index>
Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic>
DifferentialOperators<Scalar, Index>::projection(Index fid) const
{
    const Index facet_size = m_mesh.get_facet_size(fid);

    Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic> I =
        Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic>::Identity(facet_size, facet_size);

    auto U = sharp(fid);
    auto V = flat(fid);

    return I - V * U;
}

// Per-facet inner_product_0_form operator
template <typename Scalar, typename Index>
Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic>
DifferentialOperators<Scalar, Index>::inner_product_0_form(Index fid) const
{
    auto vec_area = attribute_matrix_view<Scalar>(m_mesh, m_vector_area_id);
    const Scalar area = vec_area.row(fid).norm();
    const Index facet_size = m_mesh.get_facet_size(fid);

    Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic> M =
        Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic>::Identity(facet_size, facet_size);
    M *= (area / facet_size);

    return M;
}

// Per-facet inner_product_1_form operator
template <typename Scalar, typename Index>
Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic>
DifferentialOperators<Scalar, Index>::inner_product_1_form(Index fid, Scalar lambda) const
{
    auto vec_area = attribute_matrix_view<Scalar>(m_mesh, m_vector_area_id);
    const Scalar area = vec_area.row(fid).norm();

    auto U = sharp(fid);
    auto P = projection(fid);

    return area * U.transpose() * U + lambda * P.transpose() * P;
}

// Per-facet inner_product_2_form operator
template <typename Scalar, typename Index>
Eigen::Matrix<Scalar, 1, 1> DifferentialOperators<Scalar, Index>::inner_product_2_form(
    Index fid) const
{
    auto vec_area = attribute_matrix_view<Scalar>(m_mesh, m_vector_area_id);
    const Scalar area = vec_area.row(fid).norm();

    Eigen::Matrix<Scalar, 1, 1> M;
    M(0, 0) = Scalar(1) / area;

    return M;
}

// Per-facet laplacian operator
template <typename Scalar, typename Index>
Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic>
DifferentialOperators<Scalar, Index>::laplacian(Index fid, Scalar lambda) const
{
    auto D0 = d0(fid);
    auto M1 = inner_product_1_form(fid, lambda);

    return D0.transpose() * M1 * D0;
}

// Vertex-facet Levi-Civita operator
template <typename Scalar, typename Index>
Eigen::Matrix<Scalar, 2, 2> DifferentialOperators<Scalar, Index>::levi_civita(Index fid, Index lv)
    const
{
    return levi_civita_nrosy(fid, lv, 1);
}

// Vertex-facet Levi-Civita operator for n-rosy fields
template <typename Scalar, typename Index>
Eigen::Matrix<Scalar, 2, 2>
DifferentialOperators<Scalar, Index>::levi_civita_nrosy(Index fid, Index lv, Index n) const
{
    auto vec_area = attribute_matrix_view<Scalar>(m_mesh, m_vector_area_id);
    auto vertex_normal = attribute_matrix_view<Scalar>(m_mesh, m_vertex_normal_id);

    const Index vid = m_mesh.get_facet_vertex(fid, lv);
    auto Tf = facet_basis(fid);
    auto Tv = vertex_basis(vid);

    Vector nf = vec_area.row(fid); // No need to normalize
    Vector nv = vertex_normal.row(vid);
    auto Q = Eigen::Quaternion<Scalar>::FromTwoVectors(nv, nf).matrix();

    if (n != 1) {
        la_debug_assert(n > 1, "n should be positive.");

        Eigen::Matrix<Scalar, 3, 3> R = Q;
        for (Index i = 1; i < n; i++) {
            R = R * Q;
        }
        Q = R;
    }

    return Tf.transpose() * Q * Tv;
}

// Per-facet Levi-Civita operator
template <typename Scalar, typename Index>
Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic>
DifferentialOperators<Scalar, Index>::levi_civita(Index fid) const
{
    return levi_civita_nrosy(fid, 1);
}

// Per-facet Levi-Civita operator for n-rosy fields
template <typename Scalar, typename Index>
Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic>
DifferentialOperators<Scalar, Index>::levi_civita_nrosy(Index fid, Index n) const
{
    const Index facet_size = m_mesh.get_facet_size(fid);
    Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic> R(facet_size * 2, facet_size * 2);
    R.setZero();
    for (Index lv = 0; lv < facet_size; lv++) {
        auto Rc = levi_civita_nrosy(fid, lv, n);
        R.block(lv * 2, lv * 2, 2, 2) = Rc;
    }
    return R;
}

// Per-facet covariant derivative operator
template <typename Scalar, typename Index>
Eigen::Matrix<Scalar, 4, Eigen::Dynamic> DifferentialOperators<Scalar, Index>::covariant_derivative(
    Index fid) const
{
    return covariant_derivative_nrosy(fid, 1);
}

// Per-facet covariant derivative operator for n-rosy fields
template <typename Scalar, typename Index>
Eigen::Matrix<Scalar, 4, Eigen::Dynamic>
DifferentialOperators<Scalar, Index>::covariant_derivative_nrosy(Index fid, Index n) const
{
    auto R = levi_civita_nrosy(fid, n);

    Eigen::Matrix<Scalar, 2, 2> I2;
    I2.setIdentity();
    auto G = gradient(fid);
    auto Tf = facet_basis(fid);

    return kronecker_product(Tf.transpose() * G, I2) * R;
}

// Per-facet covariant projection
template <typename Scalar, typename Index>
Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic>
DifferentialOperators<Scalar, Index>::covariant_projection(Index fid) const
{
    return covariant_projection_nrosy(fid, 1);
}

// Per-facet covariant projection for n-rosy fields
template <typename Scalar, typename Index>
Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic>
DifferentialOperators<Scalar, Index>::covariant_projection_nrosy(Index fid, Index n) const
{
    auto R = levi_civita_nrosy(fid, n);

    Eigen::Matrix<Scalar, 2, 2> I2;
    I2.setIdentity();
    auto D0 = d0(fid);
    auto P = projection(fid);

    return kronecker_product(P * D0, I2) * R;
}

// Per-facet connection laplacian
template <typename Scalar, typename Index>
Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic>
DifferentialOperators<Scalar, Index>::connection_laplacian(Index fid, Scalar lambda) const
{
    return connection_laplacian_nrosy(fid, 1, lambda);
}

// Per-facet connection laplacian for n-rosy fields
template <typename Scalar, typename Index>
Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic>
DifferentialOperators<Scalar, Index>::connection_laplacian_nrosy(Index fid, Index n, Scalar lambda)
    const
{
    auto vec_area = attribute_matrix_view<Scalar>(m_mesh, m_vector_area_id);
    const Scalar area = vec_area.row(fid).norm();

    auto G_cov = covariant_derivative_nrosy(fid, n);
    auto P_cov = covariant_projection_nrosy(fid, n);

    return area * G_cov.transpose() * G_cov + lambda * P_cov.transpose() * P_cov;
}

// Compute vertex normals from vector area
template <typename Scalar, typename Index>
void DifferentialOperators<Scalar, Index>::compute_vertex_normal_from_vector_area()
{
    auto vec_area = attribute_matrix_view<Scalar>(m_mesh, m_vector_area_id);
    const Index num_facets = m_mesh.get_num_facets();
    constexpr char const* vertex_normal_name = "polyddg::vertex_normal";

    m_vertex_normal_id = internal::find_or_create_attribute<Scalar>(
        m_mesh,
        vertex_normal_name,
        AttributeElement::Vertex,
        AttributeUsage::Normal,
        3,
        internal::ResetToDefault::No);
    auto vertex_normals = attribute_matrix_ref<Scalar>(m_mesh, m_vertex_normal_id);
    vertex_normals.setZero();

    // Accumulate area-weighted facet normals to vertices
    for (Index fid = 0; fid < num_facets; fid++) {
        const Index facet_size = m_mesh.get_facet_size(fid);

        for (Index lv = 0; lv < facet_size; lv++) {
            const Index vid = m_mesh.get_facet_vertex(fid, lv);
            vertex_normals.row(vid) += vec_area.row(fid);
        }
    }
    vertex_normals.rowwise().normalize();
}

// Facet basis
template <typename Scalar, typename Index>
Eigen::Matrix<Scalar, 3, 2> DifferentialOperators<Scalar, Index>::facet_basis(Index fid) const
{
    constexpr Scalar tol = std::numeric_limits<Scalar>::epsilon() * 10;
    auto vertices = vertex_view(m_mesh);
    auto vec_area = attribute_matrix_view<Scalar>(m_mesh, m_vector_area_id);

    Vector n = vec_area.row(fid).template head<3>().stableNormalized();
    Index c0 = m_mesh.get_facet_corner_begin(fid);
    Index v0 = m_mesh.get_corner_vertex(c0);
    Vector u = Vector::Zero();
    Index c_end = m_mesh.get_facet_corner_end(fid);
    Index c1 = c0 + 1;
    for (; c1 < c_end; c1++) {
        Index v1 = m_mesh.get_corner_vertex(c1);
        u = vertices.row(v1) - vertices.row(v0);
        Scalar l = u.norm();
        if (l > tol) {
            u /= l;
            break;
        }
    }
    if (c1 == c_end) {
        // Entire facet is degenerate, pick arbitrary u
        u = n.unitOrthogonal();
    }
    Vector v = n.cross(u);

    Eigen::Matrix<Scalar, 3, 2> B;
    B << u.transpose(), v.transpose();
    return B;
}

// Vertex basis
template <typename Scalar, typename Index>
Eigen::Matrix<Scalar, 3, 2> DifferentialOperators<Scalar, Index>::vertex_basis(Index vid) const
{
    constexpr Scalar tol = std::numeric_limits<Scalar>::epsilon() * 10;
    auto vertices = vertex_view(m_mesh);
    auto vertex_normals = attribute_matrix_view<Scalar>(m_mesh, m_vertex_normal_id);

    Vector n = vertex_normals.row(vid).template head<3>().stableNormalized();
    Index c = m_mesh.get_first_corner_around_vertex(vid);
    Vector u = Vector::Zero();
    while (c != invalid<Index>()) {
        Index e = m_mesh.get_corner_edge(c);
        auto [v0, v1] = m_mesh.get_edge_vertices(e);
        if (v1 == vid) std::swap(v0, v1);
        la_debug_assert(v0 == vid, "Inconsistent edge orientation.");
        u = vertices.row(v1) - vertices.row(v0);
        Scalar l = u.norm();
        if (l > tol) {
            u /= l;
            break;
        } else {
            c = m_mesh.get_next_corner_around_vertex(c);
        }
    }

    if (c == invalid<Index>()) {
        // All incident edges are degenerate, pick arbitrary u
        u = n.unitOrthogonal();
    }
    Vector v = n.cross(u);

    Eigen::Matrix<Scalar, 3, 2> B;
    B << u.transpose(), v.transpose();
    return B;
}


#define LA_X_DifferentialOperators(_, Scalar, Index) \
    template LA_POLYDDG_API class DifferentialOperators<Scalar, Index>;
LA_SURFACE_MESH_X(DifferentialOperators, 0)

} // namespace lagrange::polyddg
