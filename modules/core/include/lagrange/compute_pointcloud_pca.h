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

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
    #include <lagrange/legacy/compute_pointcloud_pca.h>
#endif

#include <lagrange/utils/span.h>

namespace lagrange {

struct ComputePointcloudPCAOptions
{
    /**
     * when true : covariance = (P - centroid) ^ T(P - centroid)
     * when false : covariance = (P) ^ T(P)
     */
    bool shift_centroid = false;

    /**
     * should we divide the result by number of points?
     */
    bool normalize = false;
};

template <typename Scalar>
struct PointcloudPCAOutput
{
    /// The point around which the covariance matrix is evaluated.
    std::array<Scalar, 3> center;

    /// The 3 components, sorted by weight magnitudes.
    std::array<std::array<Scalar, 3>, 3> eigenvectors;

    /// Each entry is a weight for the corresponding principal component
    std::array<Scalar, 3> eigenvalues;
};

/**
 * Finds the principal components for a pointcloud
 *
 * Assumes that the points are supplied in a matrix where each
 * ``row'' is a point.
 *
 * This is closely related to the inertia tensor, principal directions
 * and principal moments. But it is not exactly the same.
 *
 * COVARIANCE_MATRIX (tensor) = (P-eC)^T (P-eC)
 * Where C is the centroid, and e is column vector of ones.
 * eigenvalues and eigenvectors of this matrix would be
 * the principal weights and components.
 *
 * MOMENT OF INERTIA (tensor) = trace(P^T P)I - P^T P
 * eigenvalues and eigenvectors of this matrix would be
 * the principal moments and directions.
 *
 * @param[in]  points               The input points.
 * @param[in]  options              Options struct, see above.
 *
 * @return PointCloudPCAOutput      Contains center, eigenvectors (components) and eigenvalues
 * (weights). See above for more details.
 */
template <typename Scalar>
PointcloudPCAOutput<Scalar> compute_pointcloud_pca(
    span<const Scalar> points,
    ComputePointcloudPCAOptions options = {});

} // namespace lagrange
