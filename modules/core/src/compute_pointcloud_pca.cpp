/*
 * Copyright 2024 Adobe. All rights reserved.
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
#include <lagrange/compute_pointcloud_pca.h>
#include <lagrange/utils/assert.h>

#include <Eigen/Core>
#include <Eigen/Eigenvalues>

namespace lagrange {

template <typename Scalar>
PointcloudPCAOutput<Scalar> compute_pointcloud_pca(
    span<const Scalar> points,
    ComputePointcloudPCAOptions options)
{
    // Prechecks
    constexpr uint32_t dimension = 3;
    la_runtime_assert(points.size() % dimension == 0);

    const size_t num_points = points.size() / dimension;
    la_runtime_assert(num_points >= 2, "There must be at least two points");

    using MatrixXS = Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;
    using RowVector3S = Eigen::Matrix<Scalar, 1, dimension>;

    Eigen::Map<const MatrixXS> vertices(points.data(), num_points, dimension);

    MatrixXS covariance;
    RowVector3S center;

    if (options.shift_centroid) {
        center = vertices.colwise().mean();
    } else {
        center = RowVector3S::Zero();
    }

    if (options.normalize) {
        // We may instead divide by points.rows()-1 which applies the Bessel's correction.
        // https://en.wikipedia.org/wiki/Bessel%27s_correction
        covariance = ((vertices.rowwise() - center).transpose() * (vertices.rowwise() - center)) /
                     vertices.rows();
    } else {
        covariance = ((vertices.rowwise() - center).transpose() * (vertices.rowwise() - center));
    }

    // SelfAdjointEigenSolver is guaranteed to return the eigenvalues sorted
    // in increasing order. Note that this is not true for the general EigenValueSolver.
    Eigen::SelfAdjointEigenSolver<MatrixXS> eigs(covariance);
    // Unless something is going wrong, the covariance matrix should
    // be well behaved. Let's double check.
    la_runtime_assert(eigs.info() == Eigen::Success, "Eigen decomposition failed");

    // make sure components follow the right hand rule
    auto eigenvectors = eigs.eigenvectors();
    auto eigenvalues = eigs.eigenvalues();
    if (eigenvectors.determinant() < 0.) {
        eigenvectors.col(0) *= -1;
    }

    PointcloudPCAOutput<Scalar> output = PointcloudPCAOutput<Scalar>();
    for (size_t i = 0; i < dimension; ++i) {
        output.center[i] = center(i);
        for (size_t j = 0; j < dimension; ++j) {
            output.eigenvectors[i][j] = eigenvectors(i, j);
        }
        output.eigenvalues[i] = eigenvalues(i);
    }

    return output;
}

#define LA_X_compute_pointcloud_pca(_, Scalar)                                       \
    template LA_CORE_API PointcloudPCAOutput<Scalar> compute_pointcloud_pca<Scalar>( \
        span<const Scalar>,                                                          \
        ComputePointcloudPCAOptions);
LA_SURFACE_MESH_SCALAR_X(compute_pointcloud_pca, 0)

} // namespace lagrange
