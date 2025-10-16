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
#pragma once

// Should we move the definition to a .cpp file? Any .cpp file
// seeing Eigen/Eigenvalues would take ages to compile.
#include <Eigen/Core>
#include <Eigen/Eigenvalues>

#include <lagrange/legacy/inline.h>
#include <lagrange/utils/assert.h>

namespace lagrange {
LAGRANGE_LEGACY_INLINE
namespace legacy {

// compute_pointcloud_pca()
//
// Finds the principle components for a pointcloud
// Assumes that the points are supplied in a matrix where each
// ``row'' is a point.
//
// This is closely related to the inertia tensor, principal directions
// and principal moments. But it is not exactly the same.
//
// COVARIANCE_MATRIX (tensor) = (P-eC)^T (P-eC)
// Where C is the centroid, and e is column vector of ones.
// eigenvalues and eigenvectors of this matrix would be
// the principal weights and components.
//
// MOMENT OF INERTIA (tensor) = trace(P^T P)I - P^T P
// eigenvalues and eigenvectors of this matrix would be
// the principal moments and directions.
//
// This refactors segmentation/Pca.h

template <typename Scalar>
struct ComputePointcloudPCAOutput
{
    // The point around which the covariance matrix is evaluated.
    // Col vector to be consistent with `components` and weights.
    Eigen::Matrix<Scalar, Eigen::Dynamic, 1> center;
    // Each column is a component, sorted by weight magnitudes.
    // n_rows == n_cols == space dimension
    Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic> components;
    // Each entry is a weight for the corresponding principal component
    // n_rows == space dimension
    // Col vector to be consistent with `components`.
    Eigen::Matrix<Scalar, Eigen::Dynamic, 1> weights;
};

// Input:
// points, Each row should be a point in the point cloud.
//         To be consistent with the rest of Lagrange.
// should_shift_centeroid,
//   when true: covariance = (P-centroid)^T (P-centroid)
//   when false: covariance = (P)^T (P)
// should_normalize, should we divide the result by number of points?
template <typename Derived1>
ComputePointcloudPCAOutput<typename Derived1::Scalar> compute_pointcloud_pca(
    const Eigen::MatrixBase<Derived1>& points,
    const bool should_shift_centeroid,
    const bool should_normalize)
{
    //
    using Scalar = typename Derived1::Scalar;
    using MatrixXS =
        Eigen::Matrix<Scalar, Derived1::ColsAtCompileTime, Derived1::ColsAtCompileTime>;
    using RowVectorXS = Eigen::Matrix<Scalar, 1, Derived1::ColsAtCompileTime>;

    // Prechecks
    la_runtime_assert(points.rows() >= 2, "There must be at least two points");

    // Compute covariance
    MatrixXS covariance;
    RowVectorXS center;

    if (should_shift_centeroid) {
        center = points.colwise().mean();
    } else {
        center = RowVectorXS::Zero(points.cols());
    } // end of should_shift_centroid

    if (should_normalize) {
        // We may instead divide by points.rows()-1 which applies the Bessel's correction.
        // https://en.wikipedia.org/wiki/Bessel%27s_correction
        covariance =
            ((points.rowwise() - center).transpose() * (points.rowwise() - center)) / points.rows();
    } else {
        covariance = ((points.rowwise() - center).transpose() * (points.rowwise() - center));
    } // end of should_normalize

    // SelfAdjointEigenSolver is guaranteed to return the eigenvalues sorted
    // in increasing order. Note that this is not true for the general EigenValueSolver.
    Eigen::SelfAdjointEigenSolver<MatrixXS> eigs(covariance);
    // Unless something is going wrong, the covariance matrix should
    // be well behaved. Let's double check.
    la_runtime_assert(eigs.info() == Eigen::Success, "Eigen decomposition failed");

    ComputePointcloudPCAOutput<Scalar> output;
    output.center = center.transpose();
    output.components = eigs.eigenvectors(); // Columns are eigenvectors
    output.weights = eigs.eigenvalues();

    // make sure components follow the right hand rule
    if (output.components.determinant() < 0.) {
        output.components.col(0) *= -1;
    }

    return output;
}

} // namespace legacy
} // namespace lagrange
