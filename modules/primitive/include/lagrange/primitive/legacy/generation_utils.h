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

#include <vector>

#include <lagrange/Mesh.h>
#include <lagrange/common.h>
#include <lagrange/create_mesh.h>
#include <lagrange/internal/constants.h>
#include <lagrange/legacy/inline.h>
#include <lagrange/mesh_cleanup/remove_duplicate_vertices.h>
#include <lagrange/utils/assert.h>

namespace lagrange {
LAGRANGE_LEGACY_INLINE
namespace legacy {
// TODO: This whole file should be in the primitive namespace.

enum class PrimitiveSemanticLabel : std::uint8_t { SIDE = 0, TOP = 1, BOTTOM = 2, UNKNOWN };

template <typename Scalar>
Scalar compute_sweep_angle(const Scalar begin_angle, Scalar end_angle)
{
    while (end_angle < begin_angle) {
        end_angle += (Scalar)(2 * lagrange::internal::pi);
    }
    return end_angle - begin_angle;
}

template <typename VertexArray, typename FacetArray>
struct SubdividedMeshData
{
    // Output vertices after subdivision
    VertexArray vertices;
    // Output faces after subdivision
    FacetArray triangles;
    // Ordered list of segment indices per corner
    std::vector<std::vector<typename FacetArray::Scalar>> segment_indices;
};

template <typename VertexArray, typename Index>
struct GeometricProfile
{
    GeometricProfile(VertexArray _samples, Index _num_samples)
        : samples(_samples)
        , num_samples(_num_samples)
    {}

    // Vertices for this profile
    VertexArray samples;
    // Number of vertices for a single vertical repeat
    Index num_samples;

    // Return number of spans for this profile
    Index spans() const { return num_samples > 0 ? num_samples - 1 : 0; }
};

/**
 * Project point to the surface of the sphere with the given radius
 * centered at the specified center.
 */
template <typename VertexType, typename Scalar>
VertexType project_to_sphere(const VertexType& center, const VertexType& point, const Scalar radius)
{
    const VertexType PC = point - center;
    auto length = PC.norm();
    // Point coincides with the sphere center. Don't project.
    if (length == 0.0) {
        return center;
    }
    auto scale = radius / length;
    return scale * PC + center;
}

/**
 * Project point to the surface of the sphere with the given radius
 * centered at the specified center while preserving the segment size
 */
template <typename VertexType, typename Scalar, typename Index>
VertexType project_to_sphere(
    const VertexType& center,
    const Scalar radius,
    const Index row,
    const Index col,
    const Index size)
{
    Scalar theta = row < size ? (col * lagrange::internal::pi) / (2 * (size - row)) : 0.0;
    Scalar phi = ((size - row) * lagrange::internal::pi) / (2 * size);

    auto dx = std::copysign(radius * std::cos(theta) * std::sin(phi), center.x());
    auto dy = std::copysign(radius * std::sin(theta) * std::sin(phi), center.y());
    auto dz = std::copysign(radius * std::cos(phi), center.z());
    return center + VertexType(dx, dy, dz);
}

/**
 * Range normalize data to a unit box i.e betweeen [0,1]
 */
template <typename VertexArray>
void normalize_to_unit_box(VertexArray& input_vertices)
{
    const auto xmax = input_vertices.col(0).maxCoeff();
    const auto xmin = input_vertices.col(0).minCoeff();
    const auto ymax = input_vertices.col(1).maxCoeff();
    const auto ymin = input_vertices.col(1).minCoeff();
    const auto max_diff = std::max(
        {xmax - xmin, ymax - ymin, std::numeric_limits<typename VertexArray::Scalar>::epsilon()});

    for (auto idx : range(input_vertices.rows())) {
        auto vertex = input_vertices.row(idx);
        auto x = (vertex[0] - xmin) / max_diff;
        auto y = (vertex[1] - ymin) / max_diff;
        input_vertices.row(idx) << x, y;
    }
}

/**
 * Split edge indexed by v1 and v2 into `num_segments` pieces.  The inserted
 * points and newly generated segments are returned.
 */
template <typename VertexArray, typename Index>
std::tuple<VertexArray, std::vector<Index>> divide_line_into_segments(
    const VertexArray& vertices,
    const Index v1,
    const Index v2,
    const Index num_segments)
{
    using Scalar = typename VertexArray::Scalar;
    using IndexList = std::vector<Index>;

    const Index num_vertices = safe_cast<Index>(vertices.rows());
    IndexList output_indices(num_segments + 1);
    VertexArray output_vertices(num_vertices + num_segments - 1, vertices.cols());

    output_vertices.topRows(num_vertices) = vertices;
    output_indices[0] = v1;

    for (auto i : range(num_segments - 1)) {
        const Scalar ratio = safe_cast<Scalar>(i + 1) / num_segments;
        output_vertices.row(num_vertices + i)
            << vertices.row(v2) * ratio + vertices.row(v1) * (1.0 - ratio);
        output_indices[i + 1] = num_vertices + i;
    }

    output_indices[num_segments] = v2;
    return std::make_tuple(output_vertices, output_indices);
}

/**
 * Returns a lambda function to generates a partial torus with input parameters
 * starting at start angle such that the angle subtended by each span=t is slice.
 */

template <typename Scalar, typename Point>
std::function<Point(Scalar)> partial_torus_generator(
    Scalar major_radius,
    Scalar minor_radius,
    Eigen::Matrix<Scalar, 3, 1> center,
    Scalar start_slice_angle,
    Scalar slice_angle)
{
    return [=](Scalar t) -> Point {
        Point vert;
        Scalar theta = t * slice_angle + start_slice_angle;
        vert << major_radius + minor_radius * std::cos(theta) + center.x(),
            minor_radius * std::sin(theta) + center.y(), center.z();
        return vert;
    };
}


/*
Generate samples from a single vertex generator
*/
template <typename MeshType, typename Point>
const GeometricProfile<typename MeshType::VertexArray, typename MeshType::Index> generate_profile(
    std::function<Point(typename MeshType::Scalar)> generate_vertex,
    typename MeshType::Index spans,
    bool reverse_profile = false)
{
    using VertexArray = typename MeshType::VertexArray;
    using Scalar = typename MeshType::Scalar;
    using Index = typename MeshType::Index;
    VertexArray samples(spans + 1, 3);
    for (auto sample_idx : range(spans + 1)) {
        Scalar t = (Scalar)sample_idx / (Scalar)spans;
        if (reverse_profile) {
            samples.row(sample_idx) = generate_vertex(1 - t);
        } else {
            samples.row(sample_idx) = generate_vertex(t);
        }
    }
    return GeometricProfile<VertexArray, Index>(samples, spans + 1);
}

/*
Combine different geometric profiles to generate one geometric profile
*/
template <typename VertexArray, typename Index>
const GeometricProfile<VertexArray, Index> combine_geometric_profiles(
    std::vector<GeometricProfile<VertexArray, Index>>& profiles)
{
    Index total_samples = 0, rows = 0, start_idx = 0;
    la_runtime_assert(profiles.size() > 0, "No geometric profiles found, 0 samples generated.");

    for (const auto& profile : profiles) total_samples += profile.num_samples;

    VertexArray samples(total_samples, 3);
    // Concatenate profiles
    for (auto i : range(profiles.size())) {
        auto profile_sample = profiles[i].samples;
        assert(profile_sample.cols() == 3);

        // Stitch profiles at merge points
        if (rows && profile_sample.row(0).isApprox(samples.row(rows - 1))) {
            ++start_idx;
        }
        Index rows_to_copy = (Index)(profile_sample.rows() - start_idx);
        samples.block(rows, 0, rows_to_copy, 3) = profile_sample.bottomRows(rows_to_copy);
        rows += rows_to_copy;
        start_idx = 0;
    }

    return GeometricProfile<VertexArray, Index>(samples.topRows(rows), rows);
}

/*
  Rotate a geometric profile
*/
template <typename VertexArray, typename Index, typename Scalar>
const GeometricProfile<VertexArray, Index> rotate_geometric_profile(
    GeometricProfile<VertexArray, Index>& profile,
    Scalar theta)
{
    using Vector3S = Eigen::Matrix<Scalar, 3, 1>;

    Index num_samples = profile.num_samples;
    VertexArray samples(num_samples, 3);

    auto rotation = Eigen::AngleAxis<Scalar>(theta, Vector3S::UnitY());

    for (auto i : range(num_samples)) {
        auto sample = profile.samples.row(i);
        samples.row(i) = rotation * Vector3S(sample[0], sample[1], sample[2]);
    }

    return GeometricProfile<VertexArray, Index>(samples, num_samples);
}

/**
 * Fan triangulate a profile
 */
template <typename MeshType>
std::unique_ptr<MeshType> fan_triangulate_profile(
    const GeometricProfile<typename MeshType::VertexArray, typename MeshType::Index>& profile,
    Eigen::Matrix<typename MeshType::Scalar, 3, 1> center =
        Eigen::Matrix<typename MeshType::Scalar, 3, 1>(0.0, 0.0, 0.0),
    bool flip_normals = false)
{
    using VertexArray = typename MeshType::VertexArray;
    using FacetArray = typename MeshType::FacetArray;
    using Index = typename MeshType::Index;

    la_runtime_assert(profile.num_samples >= 2);

    Index vertex_count = profile.num_samples + 1; // Add origin as center vertex
    Index triangle_count = profile.spans();
    VertexArray vertices(vertex_count, 3);

    vertices.row(0) = center;
    vertices.block(1, 0, profile.num_samples, 3) = profile.samples;

    FacetArray facets(triangle_count, 3);
    for (Index triangle_id = 0; triangle_id < triangle_count; triangle_id++) {
        Index v0 = 0;
        Index v1 = 1 + triangle_id; // connect new ring verts with center
        Index v2 = 2 + triangle_id;

        if (flip_normals) {
            facets.row(triangle_id) << v0, v1, v2;
        } else {
            facets.row(triangle_id) << v0, v2, v1;
        }
    }

    return lagrange::create_mesh(vertices, facets);
}

/**
 *Connect profiles with triangulated facets
 */
template <typename MeshType>
std::unique_ptr<MeshType> connect_geometric_profiles_with_facets(
    std::vector<GeometricProfile<typename MeshType::VertexArray, typename MeshType::Index>>&
        profiles)
{
    using VertexArray = typename MeshType::VertexArray;
    using FacetArray = typename MeshType::FacetArray;
    using Index = typename MeshType::Index;

    Index num_profiles = safe_cast<Index>(profiles.size());
    la_runtime_assert(num_profiles > 1, "Need more than 1 profile to connect.");
    Index num_samples = profiles[0].num_samples;
    Index spans = profiles[0].spans();
    Index vertex_count = num_profiles * num_samples;
    Index triangle_count = (num_profiles - 1) * spans * 2;
    ;
    VertexArray vertices(vertex_count, 3);
    Index rows = 0;

    for (auto i : range(num_profiles)) {
        la_runtime_assert(profiles[i].num_samples == num_samples);
        vertices.block(rows, 0, num_samples, 3) = profiles[i].samples;
        rows += num_samples;
    }

    la_runtime_assert(rows == vertex_count);

    auto get_index = [&spans](Index x, Index y) -> Index { return x * (spans + 1) + y; };

    // Connect vertices, making triangle strips
    FacetArray facets(triangle_count, 3);
    Index triangle_id = 0;

    for (auto p : range(num_profiles - 1)) {
        for (auto span : range(spans)) {
            auto q0 = get_index(p, span);
            auto q1 = get_index(p + 1, span);
            auto q2 = get_index(p + 1, span + 1);
            auto q3 = get_index(p, span + 1);

            assert(q0 <= vertex_count - 1);
            assert(q1 <= vertex_count - 1);
            assert(q2 <= vertex_count - 1);
            assert(q3 <= vertex_count - 1);

            // Triangles might be degenerate if points are too close; check?
            facets.row(triangle_id) << q0, q1, q2;
            facets.row(triangle_id + 1) << q0, q2, q3;
            triangle_id += 2;
        }
    }
    la_runtime_assert(triangle_id == triangle_count);
    return lagrange::create_mesh(vertices, facets);
}

/**
 * Return a mesh by sweeping the profile across the sweep angle 'sections' number of times.
 * radius_top: Radius swept at the top
 * radius_bottom: Radius swept at the bottom
 */
template <typename MeshType>
std::unique_ptr<MeshType> sweep(
    const GeometricProfile<typename MeshType::VertexArray, typename MeshType::Index>& profile,
    typename MeshType::Index sections,
    typename MeshType::Scalar radius_top,
    typename MeshType::Scalar radius_bottom,
    typename MeshType::Scalar bevel_top = 0.0,
    typename MeshType::Scalar bevel_bottom = 0.0,
    typename MeshType::Scalar top_slice = 0.0,
    typename MeshType::Scalar base_slice = 0.0,
    typename MeshType::Scalar start_angle = 0.0,
    typename MeshType::Scalar sweep_angle = 2 * lagrange::internal::pi)
{
    using AttributeArray = typename MeshType::AttributeArray;
    using VertexArray = typename MeshType::VertexArray;
    using FacetArray = typename MeshType::FacetArray;
    using Scalar = typename MeshType::Scalar;
    using Index = typename MeshType::Index;
    using Vector3S = Eigen::Matrix<Scalar, 3, 1>;

    const Index section_count = sections + 1; // @TODO: Causes boundary loop, else UV gap
    const Index spans = profile.spans();
    const Index sample_count = profile.num_samples;
    const Index vertex_count = section_count * sample_count;
    const Index triangle_count = sections * spans * 2;
    VertexArray samples = profile.samples;

    // Generate vertices for each section
    VertexArray vertices(vertex_count, 3);
    AttributeArray uvs(vertex_count, 2);
    Index vertex_index = 0;

    // L(t) is the arclength function
    // s.t L(0.0) = 0 and
    // L(sample_count) = L_b(bottom curve) + L_mid(straight profile) + L_t(top curve)
    std::vector<Scalar> L(sample_count);
    auto L_b = bevel_bottom * base_slice;
    auto L_t = bevel_top * top_slice;

    // Initialize arc lengths with default parameters
    L[0] = 0.0;
    for (auto t : range<Index>(1, sample_count)) {
        Scalar segment_length = (samples.row(t) - samples.row(t - 1)).norm();
        L[t] = L[t - 1] + segment_length;
    }

    // UV calculations when radii are different
    //[todo]: high tolerance until normalization can maintain aspect ratio
    constexpr Scalar uv_tol = safe_cast<Scalar>(1.5);

    auto height = samples.col(1).maxCoeff() - samples.col(1).minCoeff();
    auto angle = std::atan2(height, fabs(radius_bottom - radius_top));
    auto phi = radius_bottom > radius_top ? angle : (0.5 * lagrange::internal::pi - angle);
    auto psi = radius_bottom > radius_top ? 0.5 * lagrange::internal::pi - phi : phi;
    auto tan_half_angle = std::tan(0.5 * phi);

    // R is the length from apex of cone of radius r (radius top or bottom)
    // to the end of the curve.
    Scalar R(0.0);
    if (radius_bottom > radius_top) {
        /* Profile to sweep:
           ¯¯৲Lt
              \
               \
           _____）Lb
        */
        auto cot_half_angle = tan_half_angle > 0 ? 1 / tan_half_angle : 0.0;
        auto H = height * radius_bottom / (radius_bottom - radius_top);
        auto hyp = sqrt(pow(radius_bottom, 2) + pow(H, 2));
        R = (Scalar)(hyp - bevel_bottom * cot_half_angle + L_b);
    } else if (radius_bottom < radius_top) {
        /* Profile to sweep:
                ¯¯৲Lt
                  /
                 /
           _____⎠Lb
           */
        auto H = height * radius_top / (radius_top - radius_bottom);
        auto hyp = sqrt(pow(radius_top, 2) + pow(H, 2));
        R = (Scalar)(hyp - bevel_top * tan_half_angle + L_t);
    }

    for (auto section : range(section_count)) {
        Scalar theta = (Scalar)section / (Scalar)sections * sweep_angle + start_angle;
        auto rotation = Eigen::AngleAxis<Scalar>(theta, Vector3S::UnitY());
        for (auto t : range(sample_count)) {
            // Transform the source vertex, to sweep it to new position/orientation
            auto sample = samples.row(t);
            vertices.row(vertex_index) = rotation * Vector3S(sample[0], sample[1], sample[2]);

            // Set uvs per vertex
            if (fabs(radius_bottom - radius_top) < uv_tol) {
                /* Profile to sweep:
                   ¯¯¯⎞Lt
                      |
                      |
                   ___⎠Lb
                */
                auto ratio = (sweep_angle * radius_bottom) / sections;
                // Map 1:1 with the uv grid
                uvs(vertex_index, 0) = ratio * section;
                uvs(vertex_index, 1) = L[t];
            } else {
                Scalar angle2 = (Scalar)(theta * std::sin(psi)); // Polar angle after unwrapping
                auto multiplier =
                    radius_bottom > radius_top ? R - L[t] : L[t] + (R - L[sample_count - 1]);
                uvs(vertex_index, 0) = multiplier * std::cos(angle2);
                uvs(vertex_index, 1) = multiplier * std::sin(angle2);
            }
            vertex_index++;
        }
    }

    auto get_index = [&spans](Index x, Index y) -> Index { return x * (spans + 1) + y; };

    // Connect vertices, making triangle strips
    FacetArray facets(triangle_count, 3);
    Index triangle_id = 0;

    for (auto section : range(sections)) {
        // For full sweep, the next section for the penultimate vertex is the 0th section.
        Index next_section = (section + 1) % section_count;
        for (auto span : range(spans)) {
            auto q0 = get_index(section, span);
            auto q1 = get_index(next_section, span);
            auto q2 = get_index(next_section, span + 1);
            auto q3 = get_index(section, span + 1);

            assert(q0 < vertex_index);
            assert(q1 < vertex_index);
            assert(q2 < vertex_index);
            assert(q3 < vertex_index);

            facets.row(triangle_id) << q0, q1, q2;
            facets.row(triangle_id + 1) << q0, q2, q3;
            triangle_id += 2;
        }
    }
    auto mesh = lagrange::create_mesh(vertices, facets);
    const auto xmin = uvs.col(0).minCoeff();
    const auto ymin = uvs.col(1).minCoeff();

    // Recenter uv coordinates to begin from (0,0)
    if (xmin < 0.0) {
        uvs.col(0).array() += -1.0f * xmin;
    }
    if (ymin < 0.0) {
        uvs.col(1).array() += -1.0f * ymin;
    }
    mesh->initialize_uv(uvs, facets);

    return mesh;
}

/**
 * Makes a fan-based disk, of #sections radial segments. by default, disk is oriented around origin,
 * single sided, with y-up face
 */
template <typename MeshType>
std::unique_ptr<MeshType> generate_disk(
    typename MeshType::Scalar radius,
    typename MeshType::Index sections,
    typename MeshType::Scalar start_angle = 0.0,
    typename MeshType::Scalar sweep_angle = 2 * lagrange::internal::pi,
    Eigen::Matrix<typename MeshType::Scalar, 3, 1> center =
        Eigen::Matrix<typename MeshType::Scalar, 3, 1>(0.0, 0.0, 0.0),
    bool flip_normals = true)
{
    using VertexArray = typename MeshType::VertexArray;
    using FacetArray = typename MeshType::FacetArray;
    using Scalar = typename MeshType::Scalar;
    using Index = typename MeshType::Index;
    using UVArray = typename MeshType::UVArray;
    using Vector3S = Eigen::Matrix<Scalar, 3, 1>;
    using Affine3S = Eigen::Transform<Scalar, 3, Eigen::Affine>;
    using Translation3S = Eigen::Translation<Scalar, 3>;

    const Index section_count = sweep_angle == 2 * lagrange::internal::pi ? sections : sections + 1;
    Index vertex_count = section_count + 1; // Add origin as center vertex
    Index triangle_count = sections;

    auto transform = Affine3S(Translation3S(center) * Eigen::Scaling(radius));
    Vector3S origin = transform * Vector3S::Zero();
    VertexArray vertices(vertex_count, 3);
    UVArray uvs(vertex_count, 2);
    Index vertex_index = 0;

    for (auto section : range(section_count)) {
        Scalar theta = (Scalar)section / (Scalar)sections * sweep_angle + start_angle;
        la_runtime_assert(vertex_index < vertex_count, "Index out of range");
        auto transformed_vertex = transform * Vector3S(std::cos(theta), 0, -std::sin(theta));
        vertices.row(vertex_index) = transformed_vertex;

        // Set uvs per vertex
        uvs(vertex_index, 0) = transformed_vertex[0];
        uvs(vertex_index, 1) = transformed_vertex[2];

        vertex_index++;
    }

    // create cap center point
    Index center_vertex_index = vertex_index;
    la_runtime_assert(center_vertex_index < vertex_count);
    vertices.row(center_vertex_index) = origin;

    // Add uv for origin
    uvs(center_vertex_index, 0) = origin[0];
    uvs(center_vertex_index, 1) = origin[2];

    la_runtime_assert(++vertex_index == vertex_count);

    FacetArray facets(triangle_count, 3);
    Index triangle_id = 0;
    for (auto section : range(sections)) {
        Index next_section = (section + 1) % section_count;
        la_runtime_assert(triangle_id < triangle_count);

        Index v0 = center_vertex_index;
        Index v1 = section; // connect new ring verts with center
        Index v2 = next_section;

        assert(v0 < vertex_index);
        assert(v1 < vertex_index);
        assert(v2 < vertex_index);

        if (flip_normals) {
            facets.row(triangle_id++) << v0, v2, v1;
        } else {
            facets.row(triangle_id++) << v0, v1, v2;
        }
    }
    la_runtime_assert(triangle_id == triangle_count);

    auto mesh = lagrange::create_mesh(vertices, facets);

    // Ensure UV has positive orientation.
    if (!flip_normals) {
        uvs.col(0) *= -1;
    }

    // Recenter uv coordinates to begin from (0,0)
    uvs.col(0).array() += radius;
    uvs.col(1).array() += radius;

    mesh->initialize_uv(uvs, facets);
    return mesh;
}

// ==========================================
//     Semantic labels
// ==========================================

template <typename MeshType>
void set_uniform_semantic_label(MeshType& mesh, const PrimitiveSemanticLabel label)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "this is not a mesh");
    using AttributeArray = typename MeshType::AttributeArray;
    using Scalar = typename MeshType::Scalar;
    AttributeArray semantic_label =
        AttributeArray::Constant(mesh.get_num_facets(), 1, safe_cast_enum<Scalar>(label));
    if (!mesh.has_facet_attribute("semantic_label")) {
        mesh.add_facet_attribute("semantic_label");
    }
    mesh.import_facet_attribute("semantic_label", semantic_label);
}

// ==========================================
//     Spherical Projection Per Corner UVs
// ==========================================

template <typename MeshType>
typename MeshType::UVArray compute_spherical_uv_mapping(
    const MeshType& mesh,
    const Eigen::Matrix<typename MeshType::Scalar, 3, 1>& center)
{
    using Index = typename MeshType::Index;
    using UVArray = typename MeshType::UVArray;
    using Scalar = typename MeshType::Scalar;

    const Index num_vertices = mesh.get_num_vertices();
    const auto& vertices = mesh.get_vertices();

    UVArray uvs_per_vertex(num_vertices, 2);

    for (auto i : range(num_vertices)) {
        const auto vertex = (vertices.row(i) - center.transpose()).eval();
        const Scalar theta = std::atan2(vertex[0], vertex[2]);
        const Scalar phi = lagrange::internal::pi - std::acos(vertex[1] / vertex.norm());
        uvs_per_vertex.row(i) << (theta + lagrange::internal::pi) / (2 * lagrange::internal::pi),
            phi / lagrange::internal::pi;
    }

    const Index num_facets = mesh.get_num_facets();
    const auto& facets = mesh.get_facets();
    const Index vertex_per_facet = mesh.get_vertex_per_facet();
    UVArray uvs(num_facets * vertex_per_facet, uvs_per_vertex.cols());

    // Assign UVs per face
    for (Index i = 0; i < num_facets; i++) {
        for (Index j = 0; j < vertex_per_facet; j++) {
            uvs.row(i * vertex_per_facet + j) = uvs_per_vertex.row(facets(i, j));
        }
    }

    // Correct Seams
    constexpr Scalar tol = 1e-6;
    constexpr Scalar UV_THRESH = 1.0 / 3.0 + tol;

    for (Index i = 0; i < num_facets; i++) {
        Index face_idx = i * vertex_per_facet;
        Scalar min_u = 2;
        Scalar max_u = -2;
        std::vector<bool> vertices_on_border(vertex_per_facet, false);
        Scalar sum_mid_u = 0;
        Index count = 0;
        bool left_side = true;
        for (auto j : range(vertex_per_facet)) {
            const Scalar u = uvs(face_idx + j, 0);
            vertices_on_border[j] = (u < tol) || (u > 1.0 - tol);
            if (!vertices_on_border[j]) {
                sum_mid_u += u;
                count++;
            }
            if (u < min_u) min_u = u;
            if (u > max_u) max_u = u;
        }
        left_side = (sum_mid_u / count) < 0.5;

        if (max_u > min_u + UV_THRESH) {
            for (auto j : range(vertex_per_facet)) {
                if (vertices_on_border[j]) {
                    auto& u = uvs(face_idx + j, 0);
                    if (left_side && u > 1.0 - tol) u = 0.0;
                    if (!left_side && u < tol) u = 1.0;
                }
            }
        }
    }

    // Correct Poles: v = 0 or v = 1
    if (vertex_per_facet == 3) {
        for (Index i = 0; i < num_facets; i++) {
            for (Index j = 0; j < vertex_per_facet; j++) {
                const Index face_idx = i * vertex_per_facet;
                const Scalar v = uvs(face_idx + j, 1);
                if (v < tol || v > 1.0 - tol) {
                    Index prev = j < 1 ? j + vertex_per_facet - 1 : j - 1;
                    Index next = (j + 1) % vertex_per_facet;

                    // Average of residual vertices
                    uvs(face_idx + j, 0) =
                        0.5 * (uvs(face_idx + prev, 0) + uvs(face_idx + next, 0));
                }
            }
        }
    } else if (vertex_per_facet == 4) {
        for (Index i = 0; i < num_facets; i++) {
            for (Index j = 0; j < vertex_per_facet; j++) {
                const Index face_idx = i * vertex_per_facet;
                const Scalar v = uvs(face_idx + j, 1);
                if (v < tol || v > 1.0 - tol) {
                    Index prev = j < 1 ? j + vertex_per_facet - 1 : j - 1;
                    Index next = (j + 1) % vertex_per_facet;
                    Index opposite = (j + 2) % vertex_per_facet;

                    // Theta_pole = t - (theta_opposite - t),
                    // where t = 0.5 * (theta_prev + theta_next)
                    const Scalar t = 0.5 * (uvs(face_idx + prev, 0) + uvs(face_idx + next, 0));
                    uvs(face_idx + j, 0) = t - (uvs(face_idx + opposite, 0) - t);
                    continue;
                }
            }
        }
    } else {
        throw std::runtime_error("Not supported");
    }
    return uvs;
}

} // namespace legacy
} // namespace lagrange
