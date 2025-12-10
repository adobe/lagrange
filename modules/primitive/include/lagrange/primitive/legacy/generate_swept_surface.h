/*
 * Copyright 2021 Adobe. All rights reserved.
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

#include <lagrange/Logger.h>
#include <lagrange/Mesh.h>
#include <lagrange/common.h>
#include <lagrange/compute_normal.h>
#include <lagrange/compute_triangle_normal.h>
#include <lagrange/create_mesh.h>
#include <lagrange/legacy/inline.h>
#include <lagrange/primitive/SweepPath.h>
#include <lagrange/primitive/generation_utils.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/safe_cast.h>

#include <Eigen/Geometry>

#include <lagrange/utils/warnoff.h>
#include <lagrange/utils/warnon.h>
#include <tbb/parallel_for.h>

#include <lagrange/internal/constants.h>
#include <limits>
#include <vector>

namespace lagrange {
namespace primitive {
LAGRANGE_LEGACY_INLINE
namespace legacy {

namespace internal {

template <typename Derived>
bool is_path_closed(const Eigen::MatrixBase<Derived>& path)
{
    if (path.rows() <= 2) return false;

    using Scalar = typename Derived::Scalar;
    constexpr Scalar TOL = std::numeric_limits<Scalar>::epsilon() * 10;
    return (path.row(0) - path.row(path.rows() - 1)).squaredNorm() < TOL;
}

/**
 * Compute profile break points such that each piece is less than `max_len` long and contains no
 * sharp turns.
 *
 * @param[in]  profile        An Eigen matrix representing profile samples.
 * @param[in]  arc_lengths    Arc lengths associated with each sample.
 * @param[in]  turning_angles Turning angles associated with each sample.
 * @param[in]  max_len        Max allowed arc length of a profile piece.
 * @param[out] breaks         A boolean array indicating whether profile should
 *                            break at the corresponding sample.
 *
 * @returns  num_pieces  The number of resulting profile pieces.
 */
template <typename Derived>
int compute_profile_breaks(
    const Eigen::MatrixBase<Derived>& profile,
    const std::vector<ScalarOf<Derived>>& arc_lengths,
    const std::vector<ScalarOf<Derived>>& turning_angles,
    typename Derived::Scalar max_len,
    std::vector<bool>& breaks)
{
    using Scalar = typename Derived::Scalar;
    using Index = int;
    const Index N = static_cast<Index>(profile.rows());
    la_runtime_assert(N > 1, "Invalid profile with less than 2 points.");

    breaks.resize(N, false);

    const Index num_pieces = std::max<Index>(
        (max_len > 0 ? static_cast<Index>(std::ceil(arc_lengths[N - 1] / max_len)) : 1),
        1);
    const Scalar ave_len = arc_lengths[N - 1] / num_pieces;

    constexpr Scalar EPSILON = std::numeric_limits<Scalar>::epsilon() * 100;
    Index strip_index = 0;
    Scalar prev_arc_length = 0;
    for (Index i = 1; i < N - 1; i++) {
        if (std::abs(turning_angles[i]) > lagrange::internal::pi / 4 ||
            arc_lengths[i] - prev_arc_length > ave_len + EPSILON) {
            breaks[i] = true;
            prev_arc_length = arc_lengths[i];
            strip_index++;
        }
    }

    return strip_index + 1;
}

/**
 * Compute the turning angle of a profile curve.
 */
template <typename Derived>
std::vector<typename Derived::Scalar> compute_turning_angles(
    const Eigen::PlainObjectBase<Derived>& profile)
{
    using Scalar = typename Derived::Scalar;

    const size_t N = safe_cast<size_t>(profile.rows());
    const bool profile_closed = is_path_closed(profile);
    std::vector<Scalar> profile_turning_angles(N, 0);

    Eigen::Matrix<Scalar, 1, 3> v0, v1;
    for (size_t i = 1; i < N - 1; i++) {
        v0 = profile.row(i) - profile.row(i - 1);
        v1 = profile.row(i + 1) - profile.row(i);
        profile_turning_angles[i] = std::atan2(v1.cross(v0).norm(), v1.dot(v0));
    }
    if (profile_closed) {
        v0 = profile.row(N - 1) - profile.row(N - 2);
        v1 = profile.row(1) - profile.row(0);
        Scalar angle = std::atan2(v1.cross(v0).norm(), v1.dot(v0));
        profile_turning_angles[0] = angle;
        profile_turning_angles[N - 1] = angle;
    }

    return profile_turning_angles;
}

/**
 * Generate UV for swept surface using tensor product of arc lengths of
 * profile and sweep path.
 *
 * @param[in]  mesh       Mesh representing swept surface.
 * @param[in]  profile    Profile of the swept shape.
 * @param[in]  sweep_path Sweep path.
 * @param[in]  max_strip_len Max profile strip len.  See note below.
 * @param[in]  profile_turning_angles  The turning angles of the input profile curve.
 *
 * The UV coordinated will be asigned in the `mesh` object.
 *
 * \note
 * `max_strip_len` is used to break UV into multiple pieces based on the profile
 * arc length, where no profile piece will have arc lengths larger than
 * `max_strip_len`.  Set `max_strip_len` to 0 to indicate no break is necessary.
 */
template <typename MeshType>
void generate_uv(
    MeshType& mesh,
    const VertexArrayOf<MeshType>& profile,
    const std::vector<Eigen::Transform<ScalarOf<MeshType>, 3, Eigen::AffineCompact>>& transforms,
    ScalarOf<MeshType> max_strip_len,
    const std::vector<ScalarOf<MeshType>>& profile_turning_angles)
{
    using Scalar = ScalarOf<MeshType>;
    using Index = IndexOf<MeshType>;
    using UVArray = typename MeshType::UVArray;
    using UVIndices = typename MeshType::UVIndices;

    const Index N = safe_cast<Index>(profile.rows());
    const Index M = safe_cast<Index>(transforms.size());

    // Cumulative sum of arc lengths for profile and sweep path.
    std::vector<Scalar> profile_arc_lengths(N);
    profile_arc_lengths[0] = 0;
    for (Index i = 1; i < N; i++) {
        profile_arc_lengths[i] =
            profile_arc_lengths[i - 1] + (profile.row(i) - profile.row(i - 1)).norm();
    }

    std::vector<Scalar> sweep_path_arc_lengths(M);
    sweep_path_arc_lengths[0] = 0;
    {
        Eigen::Matrix<Scalar, 1, 3> c = profile.colwise().mean();
        for (Index i = 1; i < M; i++) {
            sweep_path_arc_lengths[i] =
                sweep_path_arc_lengths[i - 1] +
                (transforms[i] * c.transpose() - transforms[i - 1] * c.transpose()).norm();
        }
    }

    const Index num_quads = (N - 1) * (M - 1);
    std::vector<bool> breaks;
    const Index num_strips = compute_profile_breaks(
        profile,
        profile_arc_lengths,
        profile_turning_angles,
        max_strip_len,
        breaks);
    const Index L = N + num_strips - 1;
    const Index num_uvs = L * M + num_quads;
    UVArray uvs(num_uvs, 2);
    UVIndices uv_indices(mesh.get_num_facets(), 3);

    for (Index i = 0; i < M; i++) {
        Index strip_index = 0;
        for (Index j = 0; j < N; j++) {
            uvs.row(i * L + j + strip_index) << profile_arc_lengths[j], sweep_path_arc_lengths[i];

            if (i != 0 && j != 0) {
                Index id = (i - 1) * (N - 1) + j - 1;
                Index v0 = (i - 1) * L + (j - 1) + strip_index;
                Index v1 = (i - 1) * L + (j - 1) + strip_index + 1;
                Index v2 = i * L + (j - 1) + strip_index;
                Index v3 = i * L + (j - 1) + strip_index + 1;
                Index c = L * M + id;

                uvs.row(c) = (uvs.row(v0) + uvs.row(v1) + uvs.row(v2) + uvs.row(v3)) / 4;
                uv_indices.row(id * 4) << c, v0, v1;
                uv_indices.row(id * 4 + 1) << c, v1, v3;
                uv_indices.row(id * 4 + 2) << c, v3, v2;
                uv_indices.row(id * 4 + 3) << c, v2, v0;
            }

            if (breaks[j]) {
                // End a strip
                strip_index++;
                uvs.row(i * L + j + strip_index) << profile_arc_lengths[j],
                    sweep_path_arc_lengths[i];
            }
        }
    }
    assert(uv_indices.maxCoeff() < safe_cast<Index>(uvs.rows()));
    mesh.initialize_uv(uvs, uv_indices);
}

/**
 * Compute normal field of the swept surface.
 *
 * @param[in] mesh  The input mesh object.
 * @param[in] N     The profile curve size (including the duplicate end point if closed).
 * @param[in] angle_threshold  The angle above which edges are considered as sharp.
 *                             Note that the "angle" is measured differently depending on the
 *                             facets involved.
 * @param[in] profile_turning_angles  The turning angles of the input profile curve.
 *
 * This method will add the indexed normal attribute to the input mesh object.
 *
 * @note For 2 adjacent facets that belongs to the same quad row (), their common edge is
 * considered sharp if the profile turning angle is greater than angle_threshold.  For 2
 * adjacent facets belonging to the same quad column, their common edge is considered shart if the
 * dihedral angle is greater than angle_threshold.
 */
template <typename MeshType>
void generate_normal(
    MeshType& mesh,
    IndexOf<MeshType> N,
    ScalarOf<MeshType> angle_threshold,
    const std::vector<ScalarOf<MeshType>>& profile_turning_angles)
{
    using Scalar = ScalarOf<MeshType>;
    using Index = IndexOf<MeshType>;

    // Profile corners (with turning angle > pi/4) will introduce a normal
    // discontinuity in the swept surface.
    if (!mesh.has_facet_attribute("normal")) {
        compute_triangle_normal(mesh);
    }
    const auto& facet_normals = mesh.get_facet_attribute("normal");
    const Scalar cos_threshold = std::cos(angle_threshold);

    compute_normal(mesh, [&](Index f0, Index f1) -> bool {
        Index quad0 = f0 / 4;
        Index quad1 = f1 / 4;

        Index row0 = quad0 / (N - 1);
        Index row1 = quad1 / (N - 1);
        Index col0 = quad0 % (N - 1);
        Index col1 = quad1 % (N - 1);
        if (row0 != row1 || quad0 == quad1) {
            // Angle between n0 and n1 must be less than lagrange::internal::pi/4.
            return facet_normals.row(f0).dot(facet_normals.row(f1)) > cos_threshold;
        } else {
            if (col0 + 1 == col1 || (col0 == N - 2 && col1 == 0)) {
                return profile_turning_angles[col1] <= angle_threshold;
            } else if (col1 + 1 == col0 || (col1 == N - 2 && col0 == 0)) {
                return profile_turning_angles[col0] <= angle_threshold;
            } else {
                logger().debug("f0: {} quad0: {} row0: {} col0: {}", f0, quad0, row0, col0);
                logger().debug("f1: {} quad1: {} row1: {} col1: {}", f1, quad1, row1, col1);
                throw Error(fmt::format("Facet {} and {} are not adjacent!", f0, f1));
            }
        }
    });
}

template <typename VertexArray>
VertexArray compute_offset_directions(const VertexArray& profile)
{
    using Scalar = typename VertexArray::Scalar;
    using Index = Eigen::Index;

    bool closed = is_path_closed(profile);
    const auto N = profile.rows();
    assert(N >= 2);
    VertexArray dirs(N, 3);

    for (auto i : range(N)) {
        Index v_curr = i;
        Index v_next = closed ? (i + 1) % (N - 1) : std::min<Index>(i + 1, N - 1);
        Index v_prev = closed ? (i + N - 2) % (N - 1) : std::max<Index>(1, i) - 1;
        Eigen::Matrix<Scalar, 3, 1> n0;
        Eigen::Matrix<Scalar, 3, 1> n1;
        n0 = profile.row(v_curr) - profile.row(v_prev);
        n1 = profile.row(v_next) - profile.row(v_curr);
        std::swap(n0[0], n0[1]);
        std::swap(n1[0], n1[1]);
        n0[1] *= -1;
        n1[1] *= -1;

        if (i == 0 && !closed) {
            dirs.row(i) = n1.normalized();
        } else if (i == N - 1 && !closed) {
            dirs.row(i) = n0.normalized();
        } else {
            n0.normalize();
            n1.normalize();
            Scalar l = 1 / std::sqrt(1 + n0.dot(n1) / 2 + (Scalar)1e-6);
            // Sharp angle causes numerical instability.
            // Thus, we limit the dihedral angle to be at minimum 10 degrees.
            // This translates to l can be at most 11.4737132467.
            l = std::max((Scalar)11.4737132467, l);
            dirs.row(i) = (n0 + n1).normalized() * l;
        }
    }

    if (!dirs.array().isFinite().all()) {
        // Profile contains degenerate edges.
        lagrange::logger().warn("Sweep profile contains degenerate edges.");
    }
    return dirs;
}

} // namespace internal

/**
 * Generate swepth surface.
 *
 * @param[in]  profile       A simply connected curve serving as sweep profile.
 *                           Profile must be embedded in the XY plane.
 * @param[in]  transforms    A sequence of sampled transforms of the profile
 *                           along a sweep path.
 * @param[in]  offsets       A sequence of sampled normal offset amounts.  If
 *                           empty, no offset is applied.  If not empty, it must
 *                           be of the same size as `transforms`.
 * @param[in]  max_strip_len Max profile arc length in generated UV charts.
 *                           `max_strip_len` <= 0 will generate single UV chart.
 * @param[in]  path_closed   Whether the extrusion path (represented by the set
 *                           of transforms) is closed.
 *
 * @tparam     MeshType   Mesh type.
 *
 * @return     A mesh representing the swept surface.
 *
 * Note that this is the most generic version of `generate_swept_surface`.  The `transforms` vector
 * can take into account rotation, translation, twisting and scaling.  The swept
 * path defined by `transforms` is closed iff the first and
 * last transforms are the same.
 *
 * `profile` can be either open or closed.  A curve is closed if its first point equals its last
 * point. The location of `profile` is irrelavant.  It is not required to be centered at the origin.
 */
template <typename MeshType>
std::unique_ptr<MeshType> generate_swept_surface(
    const VertexArrayOf<MeshType>& profile,
    const std::vector<Eigen::Transform<ScalarOf<MeshType>, 3, Eigen::AffineCompact>>& transforms,
    const std::vector<ScalarOf<MeshType>>& offsets,
    ScalarOf<MeshType> max_strip_len = 0,
    bool path_closed = false)
{
    using Scalar = typename MeshType::Scalar;
    using Index = typename MeshType::Index;
    using VertexArray = typename MeshType::VertexArray;
    using FacetArray = typename MeshType::FacetArray;
    using AttributeArray = typename MeshType::AttributeArray;
    using IndexArray = typename MeshType::IndexArray;

    const Index N = safe_cast<Index>(profile.rows());
    const Index M = safe_cast<Index>(transforms.size());
    logger().debug("N: {}  M: {}", N, M);
    la_runtime_assert(N > 1, "Invalid sweep profile!");
    la_runtime_assert(M > 1, "Invalid sweep path transforms!");
    const bool profile_closed = internal::is_path_closed(profile);
    const Index n = profile_closed ? (N - 1) : N;
    const Index m = path_closed ? (M - 1) : M;

    const Index num_quads = (N - 1) * (M - 1);
    const Index num_vertices = n * m + num_quads;
    const Index num_facets = 4 * num_quads;
    VertexArray vertices(num_vertices, 3);
    FacetArray facets(num_facets, 3);

    // Process offsets.
    std::function<VertexArray(Index)> offset_profile;
    VertexArray offset_dirs;
    if (!offsets.empty()) {
        la_runtime_assert(
            static_cast<Index>(offsets.size()) == M,
            "Transforms and offsets must be sampled consistently");
        offset_dirs = internal::compute_offset_directions(profile);
        offset_profile = [&](Index i) -> VertexArray {
            const Scalar offset = offsets[i];
            return profile.topRows(n) + offset_dirs.topRows(n) * offset;
        };
    } else {
        offset_profile = [&](Index) -> VertexArray { return profile.topRows(n); };
    }

    // Compute vertex positions on transformed profiles.
    // Vertices on facet centers will be initialized later when generating facets.
    for (Index i = 0; i < m; i++) {
        const auto& T = transforms[i];
        vertices.block(i * n, 0, n, 3).transpose() = T * offset_profile(i).transpose();
    }

    // Compute triangle connectivity.
    for (Index i = 0; i < M - 1; i++) {
        for (Index j = 0; j < N - 1; j++) {
            const Index id = i * (N - 1) + j;
            const Index v0 = i * n + j;
            const Index v1 = i * n + (j + 1) % n;
            const Index v2 = ((i + 1) % m) * n + j;
            const Index v3 = ((i + 1) % m) * n + (j + 1) % n;
            const Index c = n * m + id;

            // Initialize vertex representing quad center.
            vertices.row(c) =
                (vertices.row(v0) + vertices.row(v1) + vertices.row(v2) + vertices.row(v3)) / 4;

            facets.row(id * 4) << c, v0, v1;
            facets.row(id * 4 + 1) << c, v1, v3;
            facets.row(id * 4 + 2) << c, v3, v2;
            facets.row(id * 4 + 3) << c, v2, v0;
        }
    }

    // Initialize arc lengths for profile and extrusion path.
    std::vector<Scalar> profile_arc_lengths(N, 0), path_arc_lengths(M, 0);
    for (auto i : range(N - 1)) {
        profile_arc_lengths[i + 1] = (profile.row(i + 1) - profile.row(i)).norm();
    }
    std::partial_sum(
        profile_arc_lengths.begin(),
        profile_arc_lengths.end(),
        profile_arc_lengths.begin());
    if (profile_arc_lengths.back() > 0) {
        for (auto& l : profile_arc_lengths) {
            l /= profile_arc_lengths.back();
        }
    }
    path_arc_lengths[0] = 0;
    {
        Eigen::Matrix<Scalar, 1, 3> c = profile.colwise().mean();
        for (Index i = 1; i < M; i++) {
            path_arc_lengths[i] =
                path_arc_lengths[i - 1] +
                (transforms[i] * c.transpose() - transforms[i - 1] * c.transpose()).norm();
        }
    }
    if (path_arc_lengths.back() > 0) {
        for (auto& l : path_arc_lengths) {
            l /= path_arc_lengths.back();
        }
    }

    // Compute latitudes and longitudes
    AttributeArray latitude(N * M + num_quads, 1); // along transformed profiles.
    AttributeArray longitude(N * M + num_quads, 1); // along sweep path.
    IndexArray feature_indices(num_facets, 3);

    for (Index i = 0; i < M; i++) {
        for (Index j = 0; j < N; j++) {
            latitude(i * N + j) = path_arc_lengths[i];
            longitude(i * N + j) = profile_arc_lengths[j];

            if (i < M - 1 && j < N - 1) {
                const Index offset = i * (N - 1) + j;
                latitude(M * N + offset) = (path_arc_lengths[i + 1] + path_arc_lengths[i]) / 2;
                longitude(M * N + offset) =
                    (profile_arc_lengths[j + 1] + profile_arc_lengths[j]) / 2;

                const Index v0 = i * N + j;
                const Index v1 = i * N + j + 1;
                const Index v2 = (i + 1) * N + j;
                const Index v3 = (i + 1) * N + j + 1;
                const Index c = M * N + offset;

                feature_indices.row(offset * 4) << c, v0, v1;
                feature_indices.row(offset * 4 + 1) << c, v1, v3;
                feature_indices.row(offset * 4 + 2) << c, v3, v2;
                feature_indices.row(offset * 4 + 3) << c, v2, v0;
            }
        }
    }

    auto mesh = create_mesh(std::move(vertices), std::move(facets));

    std::vector<Scalar> profile_turning_angles = internal::compute_turning_angles(profile);
    internal::generate_uv(*mesh, profile, transforms, max_strip_len, profile_turning_angles);
    internal::generate_normal(*mesh, N, lagrange::internal::pi / 4, profile_turning_angles);

    mesh->add_indexed_attribute("latitude");
    mesh->set_indexed_attribute("latitude", latitude, feature_indices);
    mesh->add_indexed_attribute("longitude");
    mesh->set_indexed_attribute("longitude", longitude, feature_indices);

    // Set uniform semantic label
    lagrange::set_uniform_semantic_label(*mesh, PrimitiveSemanticLabel::SIDE);

    return mesh;
}


/**
 * Generate swepth surface.
 *
 * @param[in]  profile       A simply connected curve serving as sweep profile.
 * @param[in]  sweep_path    A simply connected curve representing sweep path.
 * @param[in]  max_strip_len Max profile arc length in generated UV charts.
 *                           `max_strip_len` <= 0 will generate single UV chart.
 *
 * @tparam     MeshType   Mesh type.
 *
 * @return     A mesh representing the swept surface.
 *
 * Note that both `profile` and `sweep_path` can be either open or closed.  A
 * curve is closed if its first point equals its last point.
 *
 * The positions of `profile` and `sweep_path` are irrelavant.  Neither is
 * required to be centered at the origin.
 */
template <typename MeshType>
std::unique_ptr<MeshType> generate_swept_surface(
    const VertexArrayOf<MeshType>& profile,
    const VertexArrayOf<MeshType>& sweep_path,
    ScalarOf<MeshType> max_strip_len = 0)
{
    typename MeshType::VertexType profile_center =
        (profile.colwise().minCoeff() + profile.colwise().maxCoeff()) / 2;

    PolylineSweepPath<VertexArrayOf<MeshType>> path(sweep_path);
    path.set_pivot(profile_center);
    path.initialize();
    const auto& transforms = path.get_transforms();
    return generate_swept_surface<MeshType>(
        profile,
        transforms,
        {},
        max_strip_len,
        path.is_closed());
}

/**
 * Generate swept surface.
 *
 * @param[in]  profile     A simplly connected curve serving as sweep profile.
 * @param[in]  sweep_path  A sweep path object.
 * @param[in]  max_strip_len Max profile arc length in generated UV charts.
 *                           `max_strip_len` <= 0 will generate single UV chart.
 *
 * @tparam     MeshType   Mesh type.
 *
 * @return     A mesh representing the swept surface.
 *
 * Note that both `profile` and `sweep_path` can be either open or closed.  A
 * curve is closed if its first point equals its last point.
 */
template <typename MeshType>
std::unique_ptr<MeshType> generate_swept_surface(
    const VertexArrayOf<MeshType>& profile,
    const SweepPath<ScalarOf<MeshType>>& sweep_path,
    ScalarOf<MeshType> max_strip_len = 0)
{
    la_runtime_assert(
        sweep_path.get_num_samples() >= 2,
        "Please make sure sweep_path obj is sufficiently sampled.");
    return generate_swept_surface<MeshType>(
        profile,
        sweep_path.get_transforms(),
        sweep_path.get_offsets(),
        max_strip_len,
        sweep_path.is_closed());
}

template <typename VertexArray>
std::vector<VertexArray> generate_swept_surface_latitude(
    const VertexArray& profile,
    const std::vector<Eigen::Transform<ScalarOf<VertexArray>, 3, Eigen::AffineCompact>>& transforms,
    const std::vector<ScalarOf<VertexArray>>& offsets = {})
{
    using Scalar = ScalarOf<VertexArray>;
    std::function<VertexArray(size_t)> offset_profile;
    VertexArray offset_dirs;
    if (!offsets.empty()) {
        la_runtime_assert(
            offsets.size() == transforms.size(),
            "Transforms and offsets must be sampled consistently");
        offset_dirs = internal::compute_offset_directions(profile);
        offset_profile = [&](size_t i) -> VertexArray {
            const Scalar offset = offsets[i];
            return profile + offset_dirs * offset;
        };
    } else {
        offset_profile = [&](size_t) -> VertexArray { return profile; };
    }

    const size_t num_transforms = transforms.size();
    std::vector<VertexArray> latitudes(num_transforms);

    tbb::parallel_for(size_t(0), num_transforms, [&](size_t i) {
        const auto& T = transforms[i];
        latitudes[i] = (T * offset_profile(i).transpose()).transpose();
    });
    return latitudes;
}

template <typename VertexArray>
std::vector<VertexArray> generate_swept_surface_latitude(
    const VertexArray& profile,
    SweepPath<ScalarOf<VertexArray>>& sweep_path)
{
    return generate_swept_surface_latitude(
        profile,
        sweep_path.get_transforms(),
        sweep_path.get_offsets());
}

template <typename VertexArray>
std::vector<VertexArray> generate_swept_surface_longitude(
    const VertexArray& profile,
    const std::vector<Eigen::Transform<ScalarOf<VertexArray>, 3, Eigen::AffineCompact>>& transforms,
    const std::vector<ScalarOf<VertexArray>>& offsets = {})
{
    using Scalar = ScalarOf<VertexArray>;
    using Index = size_t;
    using VertexType = Eigen::Matrix<Scalar, 1, 3>;

    std::function<VertexType(Index, Index)> offset_profile;
    VertexArray offset_dirs;
    if (!offsets.empty()) {
        la_runtime_assert(
            offsets.size() == transforms.size(),
            "Transforms and offsets must be sampled consistently");
        offset_dirs = internal::compute_offset_directions(profile);
        offset_profile = [&](Index i, Index j) -> VertexType {
            const Scalar offset = offsets[i];
            return profile.row(j) + offset_dirs.row(j) * offset;
        };
    } else {
        offset_profile = [&](Index, Index j) -> VertexType { return profile.row(j); };
    }

    Index num_transforms = transforms.size();
    Index profile_size = static_cast<Index>(profile.rows());
    la_runtime_assert(profile_size >= 2, "Invalid profile!");

    if ((profile.row(0) - profile.row(profile_size - 1)).norm() == 0) {
        // Profile is closed.
        profile_size -= 1;
    }

    std::vector<VertexArray> longitudes(profile_size);
    for (auto i : range(profile_size)) {
        longitudes[i].resize(num_transforms, 3);
        for (auto j : range(num_transforms)) {
            longitudes[i].row(j) = (transforms[j] * offset_profile(j, i).transpose()).transpose();
        }
    }

    return longitudes;
}

template <typename VertexArray>
std::vector<VertexArray> generate_swept_surface_longitude(
    const VertexArray& profile,
    SweepPath<ScalarOf<VertexArray>>& sweep_path)
{
    return generate_swept_surface_longitude(
        profile,
        sweep_path.get_transforms(),
        sweep_path.get_offsets());
}


} // namespace legacy
} // namespace primitive
} // namespace lagrange
