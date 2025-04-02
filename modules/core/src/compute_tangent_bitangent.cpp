/*
 * Copyright 2022 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/compute_tangent_bitangent.h>

#include <lagrange/AttributeTypes.h>
#include <lagrange/IndexedAttribute.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/internal/find_attribute_utils.h>
#include <lagrange/utils/geometry3d.h>
#include <lagrange/views.h>

#include "internal/bucket_sort.h"

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <tbb/parallel_for.h>
#include <lagrange/utils/warnon.h>
// clang-format on

namespace lagrange {

namespace {

///
/// Computes tangent and bitangent of a triangle
///
/// @param[in]  positions  3x3 matrix of corner positions, one position per row.
/// @param[in]  uvs        3x2 matrix of corner uvs, one uv per row.
///
/// @tparam     Scalar     Scalar type.
///
/// @return     A triplet (t, bt, sign), where t/bt are the tangent/bitangent vectors, and sign is
///             +1 if the UV triangle is positively oriented wrt to the tangent frame, and -1
///             otherwise.
///
template <typename Scalar>
auto triangle_tangent_bitangent(
    const Eigen::Matrix<Scalar, 3, 3>& positions,
    const Eigen::Matrix<Scalar, 3, 2>& uvs)
{
    const auto edge0 = (positions.row(1) - positions.row(0)).eval();
    const auto edge1 = (positions.row(2) - positions.row(0)).eval();

    const auto edge_uv0 = (uvs.row(1) - uvs.row(0)).eval();
    const auto edge_uv1 = (uvs.row(2) - uvs.row(0)).eval();

    const Scalar uv_signed_area = (edge_uv0.x() * edge_uv1.y() - edge_uv0.y() * edge_uv1.x());
    const Scalar r = (uv_signed_area == Scalar(0) ? Scalar(1) : Scalar(1) / uv_signed_area);

    bool sign = (uv_signed_area > 0);

    // Tangent/bitangent vectors are computed using the first-order derivatives of the UV parameterization.
    // See Section 3.1 in the reference paper:
    //    Mikkelsen, Morten. "Simulation of wrinkled surfaces revisited." (2008).
    //    http://image.diku.dk/projects/media/morten.mikkelsen.08.pdf
    Eigen::RowVector3<Scalar> t =
        ((edge0 * edge_uv1.y() - edge1 * edge_uv0.y()) * r).stableNormalized();
    Eigen::RowVector3<Scalar> bt =
        ((edge1 * edge_uv0.x() - edge0 * edge_uv1.x()) * r).stableNormalized();

    return std::make_tuple(t, bt, sign);
}

///
/// Whether the uv triangle is flipped wrt to the 3D triangle
///
/// @param[in]  uvs     3x2 matrix of uvs, one uv per row.
///
/// @tparam     Scalar  Scalar type.
///
/// @return     True iff the uv triangle is positively oriented, False otherwise.
///
template <typename Scalar>
bool triangle_uv_orientation(const Eigen::Matrix<Scalar, 3, 2>& uvs)
{
    const auto edge_uv0 = (uvs.row(1) - uvs.row(0)).eval();
    const auto edge_uv1 = (uvs.row(2) - uvs.row(0)).eval();

    const Scalar uv_signed_area = (edge_uv0.x() * edge_uv1.y() - edge_uv0.y() * edge_uv1.x());

    return uv_signed_area > 0;
}

///
/// Computes tangent and bitangent of a quad. If the quad is a point or line in the uv space, result
/// is undefined. This function can handle two points of the quad being colocated in the uv space.
///
/// @param[in]  positions  4x3 matrix of corner positions, one position per row.
/// @param[in]  uvs        4x2 matrix of corner uvs, one uv per row.
///
/// @tparam     Scalar     Scalar type.
///
/// @return     A triplet (t, bt, sign), where t/bt are the tangent/bitangent vectors, and sign is
///             +1 if the UV triangle is positively oriented wrt to the tangent frame, and -1
///             otherwise.
///
template <typename Scalar>
auto quad_tangent_bitangent(
    const Eigen::Matrix<Scalar, 4, 3>& positions,
    const Eigen::Matrix<Scalar, 4, 2>& uvs)
{
    // Check whether there are colocated vertices in UV space
    Eigen::Vector3i indices = Eigen::Vector3i(0, 1, 2);
    if ((uvs.row(0) - uvs.row(1)).isZero()) {
        indices = Eigen::Vector3i(1, 2, 3);
    } else if ((uvs.row(1) - uvs.row(2)).isZero()) {
        indices = Eigen::Vector3i(2, 3, 0);
    } else if ((uvs.row(2) - uvs.row(3)).isZero()) {
        indices = Eigen::Vector3i(3, 0, 1);
    }

    Eigen::Matrix<Scalar, 3, 3> triangle_vertices;
    triangle_vertices.row(0) = positions.row(indices(0));
    triangle_vertices.row(1) = positions.row(indices(1));
    triangle_vertices.row(2) = positions.row(indices(2));

    Eigen::Matrix<Scalar, 3, 2> triangle_uvs;
    triangle_uvs.row(0) = uvs.row(indices(0));
    triangle_uvs.row(1) = uvs.row(indices(1));
    triangle_uvs.row(2) = uvs.row(indices(2));

    // Calculate from triangle
    return triangle_tangent_bitangent(triangle_vertices, triangle_uvs);
}

///
/// Accumulate per-corner tangent and bitangent vectors and stores them as indexed attributes
/// "tangent" and "bitangent" respectively. Incident facets of a vertex are grouped when both
/// corners along their common edge share the same normal and uv index. The input mesh must have uv
/// initialized, and have "normal" as indexed attributes.
///
/// @param[in,out] mesh                 Input mesh.
/// @param[in]     uv_id                Attribute id for the input indexed uvs.
/// @param[in]     normal_id            Attribute id for the input indexed normals.
/// @param[in]     tangent_id           Attribute id for the output indexed tangents.
/// @param[in]     bitangent_id         Attribute id for the output indexed bitangents.
/// @param[in]     corner_tangents      #C * (3|4) array of per-corner tangent vectors.
/// @param[in]     corner_bitangents    #C * (3|4) array of per-corner bitangent vectors.
///
/// @tparam        Scalar               Mesh scalar type.
/// @tparam        Index                Mesh index type.
/// @tparam        DerivedT             Tangent matrix type.
/// @tparam        DerivedB             Bitangent matrix type.
///
template <typename Scalar, typename Index, typename DerivedT, typename DerivedB>
void accumulate_tangent_bitangent(
    SurfaceMesh<Scalar, Index>& mesh,
    AttributeId uv_id,
    AttributeId normal_id,
    AttributeId tangent_id,
    AttributeId bitangent_id,
    const Eigen::MatrixBase<DerivedT>& corner_tangents,
    const Eigen::MatrixBase<DerivedB>& corner_bitangents)
{
    using RowVector3r = Eigen::Matrix<Scalar, 1, 3>;

    la_runtime_assert(
        mesh.is_triangle_mesh(),
        "Only triangle meshes support tangent accumulation.");
    la_runtime_assert(safe_cast<Index>(corner_tangents.rows()) == mesh.get_num_facets() * 3);
    la_runtime_assert(safe_cast<Index>(corner_bitangents.rows()) == mesh.get_num_facets() * 3);
    la_runtime_assert(corner_tangents.cols() == corner_bitangents.cols());
    la_runtime_assert(corner_tangents.cols() == 3 || corner_tangents.cols() == 4);

    // Compute edge information
    if (!mesh.has_edges()) mesh.initialize_edges();

    const auto nvpf = mesh.get_vertex_per_facet();
    auto vertices = vertex_view(mesh);
    auto facets = facet_view(mesh);
    auto& uv_attr = mesh.template get_indexed_attribute<Scalar>(uv_id);
    auto uv_values = matrix_view(uv_attr.values());
    auto uv_indices = reshaped_view(uv_attr.indices(), 3);
    auto& normal_attr = mesh.template get_indexed_attribute<Scalar>(normal_id);
    auto normal_values = matrix_view(normal_attr.values());
    auto normal_indices = reshaped_view(normal_attr.indices(), 3);

    // Check the uv triangle orientation
    logger().trace("Is orient preserving?");
    std::vector<unsigned char> is_orient_preserving(mesh.get_num_facets());
    tbb::parallel_for(Index(0), mesh.get_num_facets(), [&](Index f) {
        Eigen::Matrix<Scalar, 3, 2> uvs;
        uvs.row(0) = uv_values.row(uv_indices(f, 0));
        uvs.row(1) = uv_values.row(uv_indices(f, 1));
        uvs.row(2) = uv_values.row(uv_indices(f, 2));
        is_orient_preserving[f] = triangle_uv_orientation(uvs);
    });

    auto is_corner_smooth = [&](Index ci, Index cj) {
        const Index fi = ci / 3;
        const Index lvi = ci % 3;
        const Index fj = cj / 3;
        const Index lvj = cj % 3;
        std::array<Index, 2> v, n, uv;
        v[0] = facets(fi, lvi);
        v[1] = facets(fj, lvj);
        n[0] = normal_indices(fi, lvi);
        n[1] = normal_indices(fj, lvj);
        uv[0] = uv_indices(fi, lvi);
        uv[1] = uv_indices(fj, lvj);
        return v[0] == v[1] && n[0] == n[1] && uv[0] == uv[1];
    };

    // Check if two face vertices are collapsed
    auto is_face_degenerate = [&](Index f) {
        for (Index lv = 0; lv < nvpf; ++lv) {
            if (facets(f, lv) == facets(f, (lv + 1) % nvpf)) {
                return true;
            }
        }
        return false;
    };

    // STEP 1: For each vertex v, iterate over each pair of incident facet (fi, fj) that share a
    // common edge eij.
    logger().trace("Loop to unify corner indices");
    DisjointSets<Index> unified_indices(mesh.get_num_corners());
    tbb::parallel_for(Index(0), Index(mesh.get_num_vertices()), [&](Index v) {
        mesh.foreach_corner_around_vertex(v, [&](Index ci) {
            Index eij = mesh.get_corner_edge(ci);
            Index fi = ci / 3;
            Index lvi = ci % 3;
            Index lvi2 = (lvi + 1) % 3;
            Index vi = facets(fi, lvi);
            Index vi2 = facets(fi, lvi2);
            if (is_face_degenerate(fi)) return;

            auto si = is_orient_preserving[fi];
            mesh.foreach_corner_around_edge(eij, [&](Index cj) {
                Index fj = cj / 3;
                Index lvj = cj % 3;
                auto sj = is_orient_preserving[fj];
                if (fi == fj) return;
                if (si != sj) return;
                Index vj = facets(fj, lvj);
                Index lvj2;
                Index vj2;
                if (vi != vj) {
                    // Manifold case where vi, vj are opposite endpoints of eij.
                    // We choose vj2 as the previous vertex around fj.
                    lvj2 = lvj;
                    lvj = (lvj + 1) % 3;
                    vj = facets(fj, lvj);
                    vj2 = facets(fj, lvj2);
                } else {
                    // Nonmanifold case where vi, vj are the same.
                    // We choose vj2 as the *next* vertex around fj.
                    lvj2 = (lvj + 1) % 3;
                    vj2 = facets(fj, lvj2);
                }
                la_debug_assert(vi == vj);
                la_debug_assert(vi2 == vj2);
                // TODO: fix our debug assert to avoid unused variable warning without evaluating it
                (void)vi2;
                (void)vj2;

                if (is_corner_smooth(ci, fj * 3 + lvj) &&
                    is_corner_smooth(fi * 3 + lvi2, fj * 3 + lvj2)) {
                    // mikktspace will only unify the indices if the entire edge is smooth!
                    unified_indices.merge(ci, fj * 3 + lvj);
                }
            });
        });
    });

    // STEP 2: Perform averaging and reindex attribute
    auto& tangents_attr = mesh.template ref_indexed_attribute<Scalar>(tangent_id);
    auto& bitangents_attr = mesh.template ref_indexed_attribute<Scalar>(bitangent_id);

    auto buckets = internal::bucket_sort(unified_indices, tangents_attr.indices().ref_all());

    // TODO: Share index buffer between the two attributes
    std::copy_n(
        tangents_attr.indices().get_all().begin(),
        mesh.get_num_corners(),
        bitangents_attr.indices().ref_all().begin());

    logger().trace("Project and average tangent/bitangent");
    auto& tangent_values_attr = tangents_attr.values();
    tangent_values_attr.resize_elements(buckets.num_representatives);
    auto tangent_values = matrix_ref(tangent_values_attr);
    auto& bitangent_values_attr = bitangents_attr.values();
    bitangent_values_attr.resize_elements(buckets.num_representatives);
    auto bitangent_values = matrix_ref(bitangent_values_attr);

    tbb::parallel_for(Index(0), buckets.num_representatives, [&](Index r) {
        for (Index i = buckets.representative_offsets[r]; i < buckets.representative_offsets[r + 1];
             ++i) {
            Index c = buckets.sorted_elements[i];
            Index f = c / 3;
            Index lv = c % 3;
            la_debug_assert(tangents_attr.indices().get(c) == r);

            // Tangent frame
            RowVector3r t = corner_tangents.row(c).template head<3>();
            RowVector3r bt = corner_bitangents.row(c).template head<3>();
            RowVector3r nrm = normal_values.row(normal_indices(f, lv));

            // Compute weight as the angle between the (projected) edges
            RowVector3r v0 = vertices.row(facets(f, lv));
            RowVector3r v1 = vertices.row(facets(f, (lv + 1) % 3));
            RowVector3r v2 = vertices.row(facets(f, (lv + 2) % 3));
            RowVector3r e01 = v1 - v0;
            RowVector3r e02 = v2 - v0;
            Scalar w = projected_angle_between(e01, e02, nrm);

            tangent_values.row(r).template head<3>() += t * w;
            bitangent_values.row(r).template head<3>() += bt * w;
            if (corner_tangents.cols() == 4) {
                if (tangent_values(r, 3) == 0) {
                    tangent_values(r, 3) = corner_tangents(c, 3);
                    bitangent_values(r, 3) = corner_bitangents(c, 3);
                } else {
                    assert(tangent_values(r, 3) == corner_tangents(c, 3));
                    assert(bitangent_values(r, 3) == corner_bitangents(c, 3));
                }
            }
        }
    });
    logger().trace("Normalize vectors");
    tbb::parallel_for(Index(0), Index(tangent_values.rows()), [&](Index c) {
        tangent_values.row(c).template head<3>().stableNormalize();
        bitangent_values.row(c).template head<3>().stableNormalize();
    });
}

///
/// Computes corner tangents and bitangents. The output tangent/bitangent matrices must be
/// pre-allocated with the correct size. If a 4-th coordinate is provided to the tangent/bitangent
/// matrices, the 4-th column with contain the sign (+1/-1) of the orientation of the 3D triangle
/// wrt to the UV triangle (i.e. whether a flip occurred).
///
/// If the mesh is quad/mixed quad, a triangle from each quad is used for the computation.
///
/// @param[in]     mesh        Input mesh.
/// @param[in]     uv_id       Attribute id for the input indexed uvs.
/// @param[in]     normal_id   Attribute id for the input indexed normals.
/// @param[in,out] tangents    #C * (3|4) array of per-corner tangent vectors.
/// @param[in,out] bitangents  #C * (3|4) array of per-corner bitangent vectors.
///
/// @tparam        Scalar      Mesh scalar type.
/// @tparam        Index       Mesh index type.
/// @tparam        DerivedT    Tangent matrix type.
/// @tparam        DerivedB    Bitangent matrix type.
///
template <typename Scalar, typename Index, typename DerivedT, typename DerivedB>
void corner_tangent_bitangent_raw(
    const SurfaceMesh<Scalar, Index>& mesh,
    AttributeId uv_id,
    AttributeId normal_id,
    Eigen::MatrixBase<DerivedT>& tangents,
    Eigen::MatrixBase<DerivedB>& bitangents,
    bool orthogonalize_bitangent)
{
    auto& uv_attr = mesh.template get_indexed_attribute<Scalar>(uv_id);
    auto& normal_attr = mesh.template get_indexed_attribute<Scalar>(normal_id);

    auto pos_values = vertex_view(mesh);
    auto uv_values = matrix_view(uv_attr.values());
    auto normal_values = matrix_view(normal_attr.values());

    la_runtime_assert(tangents.cols() == bitangents.cols());

    const bool pad_with_sign = (tangents.cols() == 4);

    if (mesh.is_triangle_mesh()) {
        // Specialized regular mesh version for speed
        const Index nvpf = mesh.get_vertex_per_facet();
        auto pos_indices = facet_view(mesh);
        auto uv_indices = reshaped_view(uv_attr.indices(), nvpf);
        auto normal_indices = reshaped_view(normal_attr.indices(), nvpf);

        tbb::parallel_for(Index(0), mesh.get_num_facets(), [&](Index f) {
            Eigen::Matrix<Scalar, 3, 3> corner_positions;
            corner_positions.row(0) = pos_values.row(pos_indices(f, 0));
            corner_positions.row(1) = pos_values.row(pos_indices(f, 1));
            corner_positions.row(2) = pos_values.row(pos_indices(f, 2));

            Eigen::Matrix<Scalar, 3, 2> corner_uvs;
            corner_uvs.row(0) = uv_values.row(uv_indices(f, 0));
            corner_uvs.row(1) = uv_values.row(uv_indices(f, 1));
            corner_uvs.row(2) = uv_values.row(uv_indices(f, 2));

            auto [t, bt, sign] = triangle_tangent_bitangent(corner_positions, corner_uvs);

            for (Index lv = 0; lv < nvpf; lv++) {
                Eigen::RowVector3<Scalar> nrm = normal_values.row(normal_indices(f, lv));
                tangents.row(f * nvpf + lv).template head<3>() =
                    project_on_plane(t, nrm).stableNormalized();
                if (orthogonalize_bitangent) {
                    bitangents.row(f * nvpf + lv).template head<3>() =
                        Scalar(sign ? 1 : -1) * nrm.cross(t).stableNormalized();
                } else {
                    bitangents.row(f * nvpf + lv).template head<3>() =
                        project_on_plane(bt, nrm).stableNormalized();
                }
                if (pad_with_sign) {
                    tangents.row(f * nvpf + lv).w() = Scalar(sign ? 1 : -1);
                    bitangents.row(f * nvpf + lv).w() = Scalar(sign ? 1 : -1);
                }
            }
        });
    } else {
        // For hybrid meshes, only quad-dominant meshes are currently supported.
        auto uv_indices = vector_view(uv_attr.indices());
        auto normal_indices = vector_view(normal_attr.indices());

        tbb::parallel_for(Index(0), mesh.get_num_facets(), [&](Index f) {
            const Index corner_begin = mesh.get_facet_corner_begin(f);

            auto facet = mesh.get_facet_vertices(f);
            auto uv_facet = uv_indices.segment(corner_begin, facet.size());

            auto [t, bt, sign] = [&] {
                if (facet.size() == 3) {
                    Eigen::Matrix<Scalar, 3, 3> corner_positions;
                    corner_positions.row(0) = pos_values.row(facet[0]);
                    corner_positions.row(1) = pos_values.row(facet[1]);
                    corner_positions.row(2) = pos_values.row(facet[2]);

                    Eigen::Matrix<Scalar, 3, 2> corner_uvs;
                    corner_uvs.row(0) = uv_values.row(uv_facet[0]);
                    corner_uvs.row(1) = uv_values.row(uv_facet[1]);
                    corner_uvs.row(2) = uv_values.row(uv_facet[2]);

                    return triangle_tangent_bitangent(corner_positions, corner_uvs);
                } else if (facet.size() == 4) {
                    // Quad
                    Eigen::Matrix<Scalar, 4, 3> corner_positions;
                    corner_positions.row(0) = pos_values.row(facet[0]);
                    corner_positions.row(1) = pos_values.row(facet[1]);
                    corner_positions.row(2) = pos_values.row(facet[2]);
                    corner_positions.row(3) = pos_values.row(facet[3]);

                    Eigen::Matrix<Scalar, 4, 2> corner_uvs;
                    corner_uvs.row(0) = uv_values.row(uv_facet[0]);
                    corner_uvs.row(1) = uv_values.row(uv_facet[1]);
                    corner_uvs.row(2) = uv_values.row(uv_facet[2]);
                    corner_uvs.row(3) = uv_values.row(uv_facet[3]);

                    return quad_tangent_bitangent(corner_positions, corner_uvs);
                } else {
                    // In order to support general polygonal meshes, we'll need to give a proper
                    // definition of a UV mapping over a general polygon. We can use the virtual
                    // vertex method from "Polygon laplacian made simple", but this is left as a
                    // future improvement.
                    //
                    //   Bunge, Astrid, et al. "Polygon laplacian made simple." Computer Graphics
                    //   Forum. Vol. 39. No. 2. 2020.
                    //   https://www.cs.jhu.edu/~misha/MyPapers/EUROG20.pdf
                    //
                    //   Alexa, Marc, and Max Wardetzky. "Discrete Laplacians on general polygonal
                    //   meshes." ACM SIGGRAPH 2011 papers. 2011. 1-10.
                    //   https://ddg.math.uni-goettingen.de/pub/Polygonal_Laplace.pdf
                    throw Error(fmt::format(
                        "Facet {} has {} vertices. Only facets with 3 and 4 vertices are supported "
                        "at the moment.",
                        f,
                        facet.size()));
                }
            }();

            for (Index lv = 0; lv < static_cast<Index>(facet.size()); lv++) {
                Eigen::RowVector3<Scalar> nrm =
                    normal_values.row(normal_indices(corner_begin + lv));
                tangents.row(corner_begin + lv).template head<3>() =
                    project_on_plane(t, nrm).stableNormalized();
                if (orthogonalize_bitangent) {
                    bitangents.row(corner_begin + lv).template head<3>() =
                        Scalar(sign ? 1 : -1) * nrm.cross(t).stableNormalized();
                } else {
                    bitangents.row(corner_begin + lv).template head<3>() =
                        project_on_plane(bt, nrm).stableNormalized();
                }
                if (pad_with_sign) {
                    tangents.row(corner_begin + lv).w() = Scalar(sign ? 1 : -1);
                    bitangents.row(corner_begin + lv).w() = Scalar(sign ? 1 : -1);
                }
            }
        });
    }
}

} // namespace

////////////////////////////////////////////////////////////////////////////////

template <typename Scalar, typename Index>
TangentBitangentResult compute_tangent_bitangent(
    SurfaceMesh<Scalar, Index>& mesh,
    TangentBitangentOptions options)
{
    la_runtime_assert(mesh.get_dimension() == 3, "Mesh must be 3D");
    la_runtime_assert(
        options.output_element_type == AttributeElement::Corner ||
            options.output_element_type == AttributeElement::Indexed,
        "Output element type must be Corner or Indexed");

    auto uv_id = internal::find_matching_attribute<Scalar>(
        mesh,
        options.uv_attribute_name,
        AttributeElement::Indexed,
        AttributeUsage::UV,
        2);
    la_runtime_assert(uv_id != invalid_attribute_id(), "Mesh must have indexed UVs");

    auto normal_id = internal::find_matching_attribute<Scalar>(
        mesh,
        options.normal_attribute_name,
        AttributeElement::Indexed,
        AttributeUsage::Normal,
        0);
    la_runtime_assert(normal_id != invalid_attribute_id(), "Mesh must have indexed normals");

    const size_t num_channels = (options.pad_with_sign ? 4 : 3);

    TangentBitangentResult result;

    if (options.output_element_type == AttributeElement::Corner) {
        result.tangent_id = internal::find_or_create_attribute<Scalar>(
            mesh,
            options.tangent_attribute_name,
            AttributeElement::Corner,
            AttributeUsage::Tangent,
            num_channels,
            internal::ResetToDefault::No);

        result.bitangent_id = internal::find_or_create_attribute<Scalar>(
            mesh,
            options.bitangent_attribute_name,
            AttributeElement::Corner,
            AttributeUsage::Bitangent,
            num_channels,
            internal::ResetToDefault::No);

        auto tangents = attribute_matrix_ref<Scalar>(mesh, result.tangent_id);
        auto bitangents = attribute_matrix_ref<Scalar>(mesh, result.bitangent_id);

        logger().debug("Compute corner tangent info");
        corner_tangent_bitangent_raw(
            mesh,
            uv_id,
            normal_id,
            tangents,
            bitangents,
            options.orthogonalize_bitangent);
    } else if (options.output_element_type == AttributeElement::Indexed) {
        using MatrixType = Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;

        MatrixType corner_tangents(mesh.get_num_corners(), num_channels);
        MatrixType corner_bitangents(mesh.get_num_corners(), num_channels);

        logger().debug("Compute corner tangent info");
        corner_tangent_bitangent_raw(
            mesh,
            uv_id,
            normal_id,
            corner_tangents,
            corner_bitangents,
            options.orthogonalize_bitangent);

        logger().debug("Accumulate tangent info");
        result.tangent_id = internal::find_or_create_attribute<Scalar>(
            mesh,
            options.tangent_attribute_name,
            AttributeElement::Indexed,
            AttributeUsage::Tangent,
            num_channels,
            internal::ResetToDefault::Yes);

        result.bitangent_id = internal::find_or_create_attribute<Scalar>(
            mesh,
            options.bitangent_attribute_name,
            AttributeElement::Indexed,
            AttributeUsage::Bitangent,
            num_channels,
            internal::ResetToDefault::Yes);

        accumulate_tangent_bitangent(
            mesh,
            uv_id,
            normal_id,
            result.tangent_id,
            result.bitangent_id,
            corner_tangents,
            corner_bitangents);
    }

    return result;
}

#define LA_X_compute_tangent_bitangent(_, Scalar, Index)                   \
    template LA_CORE_API TangentBitangentResult compute_tangent_bitangent( \
        SurfaceMesh<Scalar, Index>& mesh,                                  \
        TangentBitangentOptions options);
LA_SURFACE_MESH_X(compute_tangent_bitangent, 0)

} // namespace lagrange
