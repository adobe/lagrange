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

#include <lagrange/IndexedAttribute.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/compute_facet_normal.h>
#include <lagrange/compute_normal.h>
#include <lagrange/primitive/api.h>
#include <lagrange/primitive/generate_swept_surface.h>
#include <lagrange/triangulate_polygonal_facets.h>
#include <lagrange/utils/assert.h>
#include <lagrange/views.h>

#include <Eigen/Core>
#include <Eigen/Geometry>

namespace lagrange::primitive {

namespace {

///
/// Check if a path is closed.
///
/// A path is considered closed if its first point equals its last point.
///
/// @param[in]  path  The path to check.
/// @return     `true` if the path is closed, `false` otherwise.
///
template <typename Derived>
bool is_path_closed(
    const Eigen::MatrixBase<Derived>& path,
    typename Derived::Scalar sq_tol = std::numeric_limits<typename Derived::Scalar>::epsilon() * 10)
{
    if (path.rows() <= 2) return false;

    return (path.row(0) - path.row(path.rows() - 1)).squaredNorm() < sq_tol;
}

///
/// Compute the arc lengths of a profile curve.
///
/// @param[in]  profile_data  A span of profile data, where each point is represented by two
///                           consecutive values (x, y).
/// @param[in]  normalize     Whether to normalize the arc lengths so that the total length is 1.
///
/// @return     A vector of arc lengths, where the i-th element corresponds to the cumulative
///             (normalized) arc length up to the i-th point in the profile.
///
template <typename Scalar>
std::vector<Scalar> compute_arc_lengths(span<const Scalar> profile_data, bool normalize = false)
{
    using PointArray = Eigen::Matrix<Scalar, Eigen::Dynamic, 2, Eigen::RowMajor>;
    using PointMap = Eigen::Map<const PointArray>;
    using Index = Eigen::Index;

    PointMap profile(profile_data.data(), profile_data.size() / 2, 2);
    const Index N = safe_cast<Index>(profile.rows());

    std::vector<Scalar> profile_arc_lengths(N);
    profile_arc_lengths[0] = 0;
    for (Index i = 1; i < N; i++) {
        profile_arc_lengths[i] =
            profile_arc_lengths[i - 1] + (profile.row(i) - profile.row(i - 1)).norm();
    }

    if (normalize) {
        Scalar total_length = profile_arc_lengths[N - 1];
        if (total_length > 0) {
            for (Index i = 0; i < N; i++) {
                profile_arc_lengths[i] /= total_length;
            }
        } else {
            // If the total length is zero, set all arc lengths to zero.
            std::fill(profile_arc_lengths.begin(), profile_arc_lengths.end(), 0);
        }
    }

    return profile_arc_lengths;
}

///
/// Compute the arc lengths of a sweep path.
///
/// @param[in]  transforms  The sequence of transforms defining the sweep path.
/// @param[in]  pivot       The pivot point around which the transforms are applied.
/// @param[in]  normalize    Whether to normalize the arc lengths so that the total length is 1.
///
/// @return     A vector of arc lengths, where the i-th element corresponds to the cumulative
///             (normalized) arc length up to the i-th transform in the sweep path.
///
template <typename Scalar, typename Transform>
std::vector<Scalar> compute_sweep_path_arc_lengths(
    const std::vector<Transform>& transforms,
    const Eigen::Matrix<Scalar, 1, 3>& pivot,
    bool normalize = false)
{
    using Point = Eigen::Matrix<Scalar, 1, 3>;
    using Index = Eigen::Index;

    const Index M = safe_cast<Index>(transforms.size());
    const auto& c = pivot;

    std::vector<Scalar> sweep_path_arc_lengths(M);
    Point prev_p = transforms[0] * c.transpose();
    Point curr_p;

    sweep_path_arc_lengths[0] = 0;
    for (Index i = 1; i < M; i++) {
        curr_p = transforms[i] * c.transpose();
        sweep_path_arc_lengths[i] = sweep_path_arc_lengths[i - 1] + (curr_p - prev_p).norm();
        prev_p = curr_p;
    }

    if (normalize) {
        Scalar total_length = sweep_path_arc_lengths[M - 1];
        if (total_length > 0) {
            for (Index i = 0; i < M; i++) {
                sweep_path_arc_lengths[i] /= total_length;
            }
        } else {
            // If the total length is zero, set all arc lengths to zero.
            std::fill(sweep_path_arc_lengths.begin(), sweep_path_arc_lengths.end(), 0);
        }
    }

    return sweep_path_arc_lengths;
}


///
/// Compute the turning angles of a profile curve.
///
/// @note This function assumes the profile is a simple 2D curve embedded in the XY plane.
///
/// @param[in]  profile_data  A span of profile data, where each point is represented by two
///                           consecutive values (x, y).
///
/// @return     A vector of turning angles, where the i-th element corresponds to the turning angle
///             at the i-th point in the profile.
///
template <typename Scalar>
std::vector<Scalar> compute_turning_angles(span<const Scalar> profile_data)
{
    using PointArray = Eigen::Matrix<Scalar, Eigen::Dynamic, 2, Eigen::RowMajor>;
    using PointMap = Eigen::Map<const PointArray>;

    PointMap profile(profile_data.data(), profile_data.size() / 2, 2);

    const size_t N = safe_cast<size_t>(profile.rows());
    const bool profile_closed = is_path_closed(profile);
    std::vector<Scalar> profile_turning_angles(N, 0);

    Eigen::Matrix<Scalar, 1, 2> v0, v1;
    for (size_t i = 1; i < N - 1; i++) {
        v0 = profile.row(i) - profile.row(i - 1);
        v1 = profile.row(i + 1) - profile.row(i);
        profile_turning_angles[i] = std::atan2(std::abs(v0[0] * v1[1] - v0[1] * v1[0]), v1.dot(v0));
    }
    if (profile_closed) {
        v0 = profile.row(N - 1) - profile.row(N - 2);
        v1 = profile.row(1) - profile.row(0);
        Scalar angle = std::atan2(std::abs(v0[0] * v1[1] - v0[1] * v1[0]), v1.dot(v0));
        profile_turning_angles[0] = angle;
        profile_turning_angles[N - 1] = angle;
    }

    return profile_turning_angles;
}

///
/// Compute profile break points such that each piece is less than `max_len` long and contains no
/// sharp turns.
///
/// @param[in]  arc_lengths    Profile arc lengths associated with each sample.
/// @param[in]  turning_angles Profile turning angles associated with each sample.
/// @param[in]  max_angle      Maximum allowed turning angle (in radians) for a profile piece.
/// @param[in]  max_len        Max allowed arc length of a profile piece.
/// @param[out] breaks         A boolean array indicating whether profile should
///                            break at the corresponding sample.
///
/// @returns  num_pieces  The number of resulting profile pieces.
///
template <typename Scalar, typename Index>
Index compute_profile_breaks(
    span<const Scalar> arc_lengths,
    span<const Scalar> turning_angles,
    float max_angle,
    float max_len,
    std::vector<bool>& breaks)
{
    const Index N = static_cast<Index>(arc_lengths.size());
    la_debug_assert(N > 1, "Invalid profile with less than 2 points.");
    la_debug_assert(
        N == static_cast<Index>(turning_angles.size()),
        "Arc lengths and turning angles must have the same size.");

    breaks.resize(N, false);

    const Index num_pieces = std::max<Index>(
        (max_len > 0 ? static_cast<Index>(std::ceil(arc_lengths[N - 1] / max_len)) : 1),
        1);
    const Scalar ave_len = arc_lengths[N - 1] / num_pieces;

    constexpr Scalar EPSILON = std::numeric_limits<Scalar>::epsilon() * 100;
    Index strip_index = 0;
    Scalar prev_arc_length = 0;
    for (Index i = 1; i < N - 1; i++) {
        if (std::abs(turning_angles[i]) > max_angle ||
            arc_lengths[i] - prev_arc_length > ave_len + EPSILON) {
            breaks[i] = true;
            prev_arc_length = arc_lengths[i];
            strip_index++;
        }
    }

    return strip_index + 1;
}

///
/// Compute offset directions for each vertex in the profile.
///
/// @note This function assumes the profile is a simple 2D curve embedded in the XY plane.
///
/// @param[in]  profile  The profile vertices.
/// @return     A matrix of offset directions, where each row corresponds to a vertex in the profile.
///
template <typename VertexArray>
auto compute_offset_directions(const VertexArray& profile, bool profile_closed = false)
    -> Eigen::Matrix<typename VertexArray::Scalar, Eigen::Dynamic, 2, Eigen::RowMajor>
{
    using Scalar = typename VertexArray::Scalar;
    using Index = Eigen::Index;

    const auto N = profile.rows();
    la_debug_assert(N >= 2);
    Eigen::Matrix<Scalar, Eigen::Dynamic, 2, Eigen::RowMajor> dirs(N, 2);

    for (auto i : range(N)) {
        Index v_curr = i;
        Index v_next = profile_closed ? (i + 1) % (N - 1) : std::min<Index>(i + 1, N - 1);
        Index v_prev = profile_closed ? (i + N - 2) % (N - 1) : std::max<Index>(1, i) - 1;
        Eigen::Matrix<Scalar, 2, 1> n0;
        Eigen::Matrix<Scalar, 2, 1> n1;
        n0 = profile.row(v_curr) - profile.row(v_prev);
        n1 = profile.row(v_next) - profile.row(v_curr);
        std::swap(n0[0], n0[1]);
        std::swap(n1[0], n1[1]);
        n0[1] *= -1;
        n1[1] *= -1;

        if (i == 0 && !profile_closed) {
            dirs.row(i) = n1.normalized();
        } else if (i == N - 1 && !profile_closed) {
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

///
/// Generate UV coordinates for a swept surface mesh.
///
/// @param[in,out]  mesh                    The surface mesh to which UV coordinates will be added.
/// @param[in]      uv_attribute_name       The name of the UV attribute to create.
/// @param[in]      profile_arc_lengths     Arc lengths associated with each sample in the profile.
/// @param[in]      sweep_path_arc_lengths  Arc lengths associated with each sample in the sweep
///                                         path.
/// @param[in]      profile_turning_angles  Turning angles associated with each sample in the
///                                         profile.
/// @param[in]      use_full_uv_domain      Whether to use the full UV domain [0, 1] x [0, 1].
/// @param[in]      use_u_as_profile_length If `true`, the U coordinate will represent the profile
///                                         length; otherwise, the V coordinate will represent the
///                                         profile length.
/// @param[in]      profile_angle_threshold Maximum allowed angle (in radians) between consecutive
///                                         profile segments for it to be considered as smooth.
/// @param[in]      max_profile_length        The maximum length of a profile arc in generated UV
///                                         charts.
///
/// @return     The ID of the created UV attribute.
///
template <typename Scalar, typename Index>
AttributeId generate_uv(
    SurfaceMesh<Scalar, Index>& mesh,
    const std::string_view uv_attribute_name,
    span<const Scalar> profile_arc_lengths,
    span<const Scalar> sweep_path_arc_lengths,
    span<const Scalar> profile_turning_angles,
    bool use_full_uv_domain,
    bool use_u_as_profile_length,
    float profile_angle_threshold,
    float max_profile_length)
{
    using UVArray = Eigen::Matrix<Scalar, Eigen::Dynamic, 2, Eigen::RowMajor>;
    using UVIndices = Eigen::Matrix<Index, Eigen::Dynamic, 4, Eigen::RowMajor>;

    const Index N = safe_cast<Index>(profile_arc_lengths.size());
    const Index M = safe_cast<Index>(sweep_path_arc_lengths.size());
    la_debug_assert(
        mesh.get_num_facets() == (N - 1) * (M - 1),
        "Number of facets in the mesh does not match the expected number of quads.");

    std::vector<bool> breaks(N, false);
    Index num_strips = 1;
    if (!use_full_uv_domain) {
        num_strips = compute_profile_breaks<Scalar, Index>(
            profile_arc_lengths,
            profile_turning_angles,
            profile_angle_threshold,
            max_profile_length,
            breaks);
    }

    const Index L = N + num_strips - 1;
    const Index num_uvs = L * M;
    UVArray uvs(num_uvs, 2);
    UVIndices uv_indices(mesh.get_num_facets(), 4);

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

                uv_indices.row(id) << v0, v1, v3, v2;
            }

            if (breaks[j]) {
                // End a strip
                strip_index++;
                uvs.row(i * L + j + strip_index) << profile_arc_lengths[j],
                    sweep_path_arc_lengths[i];
            }
        }
    }

    if (use_full_uv_domain) {
        // Normalize UV coordinates to [0, 1] range.
        if (profile_arc_lengths[N - 1] > 1e-6) {
            uvs.col(0) /= profile_arc_lengths[N - 1];
        }
        if (sweep_path_arc_lengths[M - 1] > 1e-6) {
            uvs.col(1) /= sweep_path_arc_lengths[M - 1];
        }
    }

    if (!use_u_as_profile_length) {
        // Swap U and V coordinates if the profile length is not used as U.
        uvs.col(0).swap(uvs.col(1));
        Scalar max_u = uvs.col(0).maxCoeff();
        uvs.col(0) = max_u - uvs.col(0).array();
    }

    auto uv_attr_id = mesh.template create_attribute<Scalar>(
        uv_attribute_name,
        AttributeElement::Indexed,
        2,
        AttributeUsage::UV,
        {uvs.data(), static_cast<size_t>(num_uvs * 2)},
        {uv_indices.data(), static_cast<size_t>(uv_indices.size())});
    return uv_attr_id;
}

template <typename Scalar, typename Index>
AttributeId generate_normal(
    SurfaceMesh<Scalar, Index>& mesh,
    const std::string_view normal_attribute_name,
    span<const Scalar> profile_turning_angles,
    float profile_angle_threshold,
    float normal_angle_threshold,
    float epsilon)
{
    FacetNormalOptions facet_normal_options;
    facet_normal_options.output_attribute_name = "@swept_surface_facet_normal";
    auto facet_normal_attr_id = compute_facet_normal(mesh, facet_normal_options);
    auto facet_normals = attribute_matrix_view<Scalar>(mesh, facet_normal_attr_id);

    Index N = static_cast<Index>(profile_turning_angles.size());
    const Scalar normal_angle_threshold_cos = static_cast<Scalar>(std::cos(normal_angle_threshold));

    NormalOptions normal_options;
    normal_options.output_attribute_name = normal_attribute_name;
    normal_options.facet_normal_attribute_name = facet_normal_options.output_attribute_name;
    normal_options.weight_type = NormalWeightingType::Uniform;
    normal_options.distance_tolerance = epsilon;
    auto normal_attr_id = compute_normal<Scalar, Index>(
        mesh,
        [&](Index f0, Index f1) -> bool {
            Index row0 = f0 / (N - 1);
            Index row1 = f1 / (N - 1);
            Index col0 = f0 % (N - 1);
            Index col1 = f1 % (N - 1);
            if (row0 != row1) {
                // Use facet normals to determine sharp features.
                return facet_normals.row(f0).dot(facet_normals.row(f1)) >
                       normal_angle_threshold_cos;
            } else {
                // Use profile turning angle to determine sharp features.
                if (col0 + 1 == col1 || (col0 == N - 2 && col1 == 0)) {
                    return std::abs(profile_turning_angles[col1]) <=
                           static_cast<Scalar>(profile_angle_threshold);
                } else if (col1 + 1 == col0 || (col1 == N - 2 && col0 == 0)) {
                    return std::abs(profile_turning_angles[col0]) <=
                           static_cast<Scalar>(profile_angle_threshold);
                } else {
                    logger().debug("f0: {} row0: {} col0: {}", f0, row0, col0);
                    logger().debug("f1: {} row1: {} col1: {}", f1, row1, col1);
                    throw Error(fmt::format("Facet {} and {} are not adjacent!", f0, f1));
                }
            }
        },
        {},
        std::move(normal_options));

    // Only keeps one set of normal attribute to avoid confusion in downstream processing.
    mesh.delete_attribute(facet_normal_options.output_attribute_name);

    return normal_attr_id;
}

template <typename Scalar, typename Index>
AttributeId generate_longitude(
    SurfaceMesh<Scalar, Index>& mesh,
    const std::string_view longitude_attribute_name,
    span<const Scalar> profile_arc_lengths,
    span<const Scalar> sweep_path_arc_lengths)
{
    constexpr Scalar eps = std::numeric_limits<Scalar>::epsilon() * 100;
    const Index N = safe_cast<Index>(profile_arc_lengths.size());
    const Index M = safe_cast<Index>(sweep_path_arc_lengths.size());

    Scalar total_profile_arc_length = profile_arc_lengths[N - 1];

    if (total_profile_arc_length < eps) total_profile_arc_length = 1;

    auto longitude_attr_id = mesh.template create_attribute<Scalar>(
        longitude_attribute_name,
        AttributeElement::Indexed,
        1,
        AttributeUsage::Scalar);

    auto& longitude_attr = mesh.template ref_indexed_attribute<Scalar>(longitude_attr_id);

    auto& longitude_values = longitude_attr.values();
    auto& longitude_indices = longitude_attr.indices();

    longitude_values.resize_elements(N);

    for (Index j = 0; j < N; j++) {
        longitude_values.ref(j) = profile_arc_lengths[j] / total_profile_arc_length;
    }

    for (Index i = 0; i < M - 1; i++) {
        for (Index j = 0; j < N - 1; j++) {
            const Index id = i * (N - 1) + j;

            longitude_indices.ref(id * 4) = j;
            longitude_indices.ref(id * 4 + 1) = j + 1;
            longitude_indices.ref(id * 4 + 2) = j + 1;
            longitude_indices.ref(id * 4 + 3) = j;
        }
    }

    return longitude_attr_id;
}

template <typename Scalar, typename Index>
AttributeId generate_latitude(
    SurfaceMesh<Scalar, Index>& mesh,
    const std::string_view latitude_attribute_name,
    span<const Scalar> profile_arc_lengths,
    span<const Scalar> sweep_path_arc_lengths)
{
    constexpr Scalar eps = std::numeric_limits<Scalar>::epsilon() * 100;
    const Index N = safe_cast<Index>(profile_arc_lengths.size());
    const Index M = safe_cast<Index>(sweep_path_arc_lengths.size());

    Scalar total_sweep_path_arc_length = sweep_path_arc_lengths[M - 1];

    if (total_sweep_path_arc_length < eps) total_sweep_path_arc_length = 1;

    auto latitude_attr_id = mesh.template create_attribute<Scalar>(
        latitude_attribute_name,
        AttributeElement::Indexed,
        1,
        AttributeUsage::Scalar);

    auto& latitude_attr = mesh.template ref_indexed_attribute<Scalar>(latitude_attr_id);

    auto& latitude_values = latitude_attr.values();
    auto& latitude_indices = latitude_attr.indices();

    latitude_values.resize_elements(M);

    for (Index i = 0; i < M; i++) {
        latitude_values.ref(i) = sweep_path_arc_lengths[i] / total_sweep_path_arc_length;
    }

    for (Index i = 0; i < M - 1; i++) {
        for (Index j = 0; j < N - 1; j++) {
            const Index id = i * (N - 1) + j;

            latitude_indices.ref(id * 4) = i;
            latitude_indices.ref(id * 4 + 1) = i;
            latitude_indices.ref(id * 4 + 2) = i + 1;
            latitude_indices.ref(id * 4 + 3) = i + 1;
        }
    }

    return latitude_attr_id;
}

} // namespace

template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> generate_swept_surface(
    span<const Scalar> profile_data,
    const SweepOptions<Scalar>& sweep_setting,
    const SweptSurfaceOptions& options)
{
    using PointArray2D = Eigen::Matrix<Scalar, Eigen::Dynamic, 2, Eigen::RowMajor>;
    using PointMap = Eigen::Map<const PointArray2D>;

    la_runtime_assert(
        profile_data.size() % 2 == 0,
        "Profile data must have even number of elements.");
    PointMap profile(profile_data.data(), profile_data.size() / 2, 2);
    const auto& transforms = sweep_setting.sample_transforms();
    const auto& offsets = sweep_setting.sample_offsets();
    auto pivot = sweep_setting.get_pivot();

    const Index N = safe_cast<Index>(profile.rows());
    const Index M = safe_cast<Index>(transforms.size());
    la_runtime_assert(N >= 2, "Profile must have at least 2 points.");
    la_runtime_assert(M >= 2, "Sweep path must have at least 2 transforms.");

    const bool profile_closed = is_path_closed(profile);
    const bool path_closed = sweep_setting.is_closed();

    const Index n = profile_closed ? (N - 1) : N;
    const Index m = path_closed ? (M - 1) : M;

    const Index num_quads = (N - 1) * (M - 1);
    const Index num_vertices = n * m;

    SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertices(num_vertices);
    mesh.add_quads(num_quads);
    auto vertices = vertex_ref(mesh);
    auto facets = facet_ref(mesh);


    // Initialize vertices.
    vertices.col(2).setZero(); // Set Z coordinate to zero for all vertices.
    if (offsets.empty()) {
        for (Index i = 0; i < m; i++) {
            vertices.block(i * n, 0, n, 2) = profile.topRows(n);
        }
    } else {
        auto offset_dirs = compute_offset_directions(profile);
        for (Index i = 0; i < m; i++) {
            vertices.block(i * n, 0, n, 2) =
                profile.topRows(n) + offset_dirs.topRows(n) * offsets[i];
        }
    }

    for (Index i = 0; i < m; i++) {
        const auto& T = transforms[i];
        vertices.middleRows(i * n, n).template leftCols<3>().transpose() =
            T * vertices.middleRows(i * n, n).template leftCols<3>().transpose();
    }

    // Compute quad facets.
    for (Index i = 0; i < M - 1; i++) {
        for (Index j = 0; j < N - 1; j++) {
            const Index id = i * (N - 1) + j;
            const Index v0 = i * n + j;
            const Index v1 = i * n + (j + 1) % n;
            const Index v2 = ((i + 1) % m) * n + j;
            const Index v3 = ((i + 1) % m) * n + (j + 1) % n;

            facets.row(id) << v0, v1, v3, v2;
        }
    }

    // Create UV and normal attributes.
    auto profile_arc_length = compute_arc_lengths(profile_data, false);
    auto sweep_path_arc_length = compute_sweep_path_arc_lengths(transforms, pivot, false);
    auto profile_turning_angles = compute_turning_angles(profile_data);

    if (options.uv_attribute_name != "") {
        generate_uv(
            mesh,
            options.uv_attribute_name,
            {profile_arc_length.data(), profile_arc_length.size()},
            {sweep_path_arc_length.data(), sweep_path_arc_length.size()},
            {profile_turning_angles.data(), profile_turning_angles.size()},
            options.fixed_uv,
            options.use_u_as_profile_length,
            options.profile_angle_threshold,
            options.max_profile_length);
    }

    if (options.normal_attribute_name != "") {
        generate_normal(
            mesh,
            options.normal_attribute_name,
            {profile_turning_angles.data(), profile_turning_angles.size()},
            options.profile_angle_threshold,
            options.angle_threshold,
            options.epsilon);
    }

    if (options.longitude_attribute_name != "") {
        generate_longitude(
            mesh,
            options.longitude_attribute_name,
            {profile_arc_length.data(), profile_arc_length.size()},
            {sweep_path_arc_length.data(), sweep_path_arc_length.size()});
    }

    if (options.latitude_attribute_name != "") {
        generate_latitude(
            mesh,
            options.latitude_attribute_name,
            {profile_arc_length.data(), profile_arc_length.size()},
            {sweep_path_arc_length.data(), sweep_path_arc_length.size()});
    }

    if (options.triangulate) {
        TriangulationOptions triangulation_opts;
        triangulation_opts.scheme = TriangulationOptions::Scheme::CentroidFan;
        mesh.clear_edges();
        triangulate_polygonal_facets(mesh, triangulation_opts);
    }

    return mesh;
}

#define LA_X_generate_swept_surface(_, Scalar, Index)                                           \
    template LA_PRIMITIVE_API SurfaceMesh<Scalar, Index> generate_swept_surface<Scalar, Index>( \
        span<const Scalar>,                                                                     \
        const SweepOptions<Scalar>&,                                                            \
        const SweptSurfaceOptions&);

LA_SURFACE_MESH_X(generate_swept_surface, 0)

} // namespace lagrange::primitive
