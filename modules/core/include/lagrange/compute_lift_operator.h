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
#pragma once

#include <Eigen/Sparse>

namespace lagrange {

///
/// This compute the sparse bilinear map from mesh vertex attributes to point cloud attributes.
/// Input points are supposed to lie on facets on the input mesh.
/// Each row of element_indices is a input triangle index of which the point at the same row is supposed to lie.
/// It uses barycentric coordinates on each triangle to fill in coeffs of a sparse matrix which is then returned.
/// This bilinear sparse mapping (a.k.a. the returned sparse matrix) can used extend mesh defined fields to fields defined in R^3.
/// This is useful for interpolating positions or scalar curvatures to a sampled point cloud for example.
/// This is suited for use with sampling functions like random_sample_uniform that naturally build the third arg.
///
/// @return Sparse n*m matrix, where n is the number of vertex in the input mesh and m is the number of vertex in the input point cloud.
///
template <typename MeshType, typename Cloud, typename Indices>
auto compute_lift_operator_from_sampling(const MeshType& mesh /**< [in] Input triangular mesh.*/, const Cloud& closest_points /**< [in] Input point cloud. should lie on input mesh.*/, const Indices& element_indices /**< [in] Input mesh triangle indices for each point of the input point cloud.*/)
{
    using Scalar = typename MeshType::Scalar;
    using Operator = Eigen::SparseMatrix<Scalar>;
    using Index = typename Operator::StorageIndex;
    using Triplet = Eigen::Triplet<Scalar, Index>;
    using Triplets = std::vector<Triplet>;
    using std::get;

    const auto& vertices = mesh.get_vertices();
    const auto& facets = mesh.get_facets();

    assert(closest_points.rows() == element_indices.rows());

    Triplets triplets;
    for (Index kk = 0, kk_max = static_cast<Index>(closest_points.rows()); kk < kk_max; kk++) {
        const auto element_index = element_indices(kk);

        assert(element_index >= 0);
        assert(element_index < facets.rows());
        const auto facet = facets.row(element_index).template cast<Index>();

        const auto p0 = vertices.row(facet(0));
        const auto p1 = vertices.row(facet(1));
        const auto p2 = vertices.row(facet(2));
        const auto pp = closest_points.row(kk);

        const auto f0 = p0 - pp;
        const auto f1 = p1 - pp;
        const auto f2 = p2 - pp;

        const auto aa = (p1 - p0).cross(p2 - p0).norm();
        const auto w0 = f1.cross(f2).norm() / aa;
        const auto w1 = f2.cross(f0).norm() / aa;
        const auto w2 = f0.cross(f1).norm() / aa;

        assert(fabs(w0 + w1 + w2 - 1) < 1e-5);
        assert((w0 * p0 + w1 * p1 + w2 * p2 - pp).array().abs().maxCoeff() < 1e-5);

        triplets.emplace_back(Triplet{kk, facet(0), w0});
        triplets.emplace_back(Triplet{kk, facet(1), w1});
        triplets.emplace_back(Triplet{kk, facet(2), w2});
    }

    Operator ope(element_indices.size(), vertices.rows());
    ope.setFromTriplets(std::cbegin(triplets), std::cend(triplets));

    return ope;
}

///
/// This compute the sparse bilinear map from mesh vertex attributes to point cloud attributes.
/// It uses barycentric coordinates on each triangle to fill in coeffs of a sparse matrix which is then returned.
/// This bilinear sparse mapping (a.k.a. the returned sparse matrix) can used extend mesh defined fields to fields defined in R^3.
/// This is useful for interpolating positions or scalar curvatures to a sampled point cloud for example.
/// This is suited for use with bvh structure whose data return by batch_query can be used as the second arg.
///
/// @return Sparse n*m matrix, where n is the number of vertex in the input mesh and m is the number of vertex in the input point cloud.
///
template <typename MeshType, typename ClosestPoints>
auto compute_lift_operator_from_projections(
    const MeshType& mesh /**< [in] input triangular mesh.*/,
    const ClosestPoints&
        projections /**< [in] projections data returned by a call to BVH batch_query.*/)
{
    using Scalar = typename MeshType::Scalar;
    using Operator = Eigen::SparseMatrix<Scalar>;
    using Index = typename Operator::StorageIndex;
    using Triplet = Eigen::Triplet<Scalar, Index>;
    using Triplets = std::vector<Triplet>;

    const auto& vertices = mesh.get_vertices();
    const auto& facets = mesh.get_facets();

    Triplets triplets;
    Index out_index = 0;
    for (const auto& projection : projections) {
        assert(projection.embedding_element_idx >= 0);
        assert(projection.embedding_element_idx < facets.rows());
        const auto facet = facets.row(projection.embedding_element_idx).template cast<Index>();

        const auto p0 = vertices.row(facet(0));
        const auto p1 = vertices.row(facet(1));
        const auto p2 = vertices.row(facet(2));

        const auto f0 = p0 - projection.closest_point;
        const auto f1 = p1 - projection.closest_point;
        const auto f2 = p2 - projection.closest_point;

        const auto aa = (p1 - p0).cross(p2 - p0).norm();
        const auto w0 = f1.cross(f2).norm() / aa;
        const auto w1 = f2.cross(f0).norm() / aa;
        const auto w2 = f0.cross(f1).norm() / aa;

        assert(fabs(w0 + w1 + w2 - 1) < 1e-5);
        assert(
            (w0 * p0 + w1 * p1 + w2 * p2 - projection.closest_point).array().abs().maxCoeff() <
            1e-5);

        triplets.emplace_back(Triplet{out_index, facet(0), w0});
        triplets.emplace_back(Triplet{out_index, facet(1), w1});
        triplets.emplace_back(Triplet{out_index, facet(2), w2});

        out_index++;
    }

    Operator ope(projections.size(), vertices.rows());
    ope.setFromTriplets(std::cbegin(triplets), std::cend(triplets));

    return ope;
}

} // namespace lagrange
