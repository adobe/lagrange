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

#include <cmath>

#include <lagrange/DisjointSets.h>
#include <lagrange/Mesh.h>
#include <lagrange/MeshTrait.h>
#include <lagrange/chain_corners_around_edges.h>
#include <lagrange/chain_corners_around_vertices.h>
#include <lagrange/common.h>
#include <lagrange/corner_to_edge_mapping.h>
#include <lagrange/utils/geometry3d.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <igl/internal_angles.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <tbb/parallel_for.h>

namespace lagrange {

namespace detail {

///
/// Computes tangent and bitangent of a triangle
///
/// @param[in]  vertices  3x3 matrix of vertices, one vertex per row.
/// @param[in]  uvs       3x2 matrix of uvs, one uv per row.
/// @param[out] sign      UV triangle orientation.
///
/// @tparam     Scalar    Scalar type.
///
/// @return     Eigen::Matrix<Scalar, 2, 3>  Tangent is first row, bitangent second row.
///
template <typename Scalar>
Eigen::Matrix<Scalar, 2, 3> triangle_tangent_bitangent(
    const Eigen::Matrix<Scalar, 3, 3>& vertices,
    const Eigen::Matrix<Scalar, 3, 2>& uvs,
    bool& sign)
{
    const auto edge0 = (vertices.row(1) - vertices.row(0)).eval();
    const auto edge1 = (vertices.row(2) - vertices.row(0)).eval();

    const auto edge_uv0 = (uvs.row(1) - uvs.row(0)).eval();
    const auto edge_uv1 = (uvs.row(2) - uvs.row(0)).eval();

    const Scalar uv_signed_area = (edge_uv0.x() * edge_uv1.y() - edge_uv0.y() * edge_uv1.x());
    const Scalar r = (uv_signed_area == Scalar(0) ? Scalar(1) : Scalar(1) / uv_signed_area);

    sign = (uv_signed_area > 0);

    Eigen::Matrix<Scalar, 2, 3> T_BT;
    T_BT.row(0) = ((edge0 * edge_uv1.y() - edge1 * edge_uv0.y()) * r).stableNormalized();
    T_BT.row(1) = ((edge1 * edge_uv0.x() - edge0 * edge_uv1.x()) * r).stableNormalized();
    return T_BT;
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
/// @param[in]  vertices  4x3 matrix of vertices, one vertex per row.
/// @param[in]  uvs       4x2 matrix of uvs, one uv per row.
/// @param[out] sign      UV triangle orientation.
///
/// @tparam     Scalar    Scalar type.
///
/// @return     Eigen::Matrix<Scalar, 2, 3>  Tangent is first row, bitangent second row.
///
template <typename Scalar>
Eigen::Matrix<Scalar, 2, 3> quad_tangent_bitangent(
    const Eigen::Matrix<Scalar, 4, 3>& vertices,
    const Eigen::Matrix<Scalar, 4, 2>& uvs,
    bool& sign)
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
    triangle_vertices.row(0) = vertices.row(indices(0));
    triangle_vertices.row(1) = vertices.row(indices(1));
    triangle_vertices.row(2) = vertices.row(indices(2));

    Eigen::Matrix<Scalar, 3, 2> triangle_uvs;
    triangle_uvs.row(0) = uvs.row(indices(0));
    triangle_uvs.row(1) = uvs.row(indices(1));
    triangle_uvs.row(2) = uvs.row(indices(2));

    // Calculate from triangle
    return triangle_tangent_bitangent(triangle_vertices, triangle_uvs, sign);
}

///
/// Accumulate per-corner tangent and bitangent vectors and stores them as indexed attributes
/// "tangent" and "bitangent" respectively. Incident facets of a vertex are grouped when both
/// corners along their common edge share the same normal and uv index. The input mesh must have uv
/// initialized, and have "normal" as indexed attributes.
///
/// @param[in,out] mesh        Input mesh.
/// @param[in]     tangents    #F * 3 array of per-corner tangent vectors.
/// @param[in]     bitangents  #F * 3 array of per-corner bitangent vectors.
///
/// @tparam        MeshType    Mesh type.
/// @tparam        DerivedT    Tangent matrix type.
/// @tparam        DerivedB    Bitangent matrix type.
///
template <typename MeshType, typename DerivedT, typename DerivedB>
void accumulate_tangent_bitangent(
    MeshType& mesh,
    const Eigen::MatrixBase<DerivedT>& tangents,
    const Eigen::MatrixBase<DerivedB>& bitangents)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");
    using Scalar = typename MeshType::Scalar;
    using Index = typename MeshType::Index;
    using FacetArray = typename MeshType::FacetArray;
    using AttributeArray = typename MeshType::AttributeArray;
    using IndexArray = Eigen::Matrix<Index, Eigen::Dynamic, 1>;
    using RowVector3r = Eigen::Matrix<Scalar, 1, 3>;

    la_runtime_assert(mesh.get_vertex_per_facet() == 3, "Only triangle meshes are supported for this.");
    la_runtime_assert(safe_cast<Index>(tangents.rows()) == mesh.get_num_facets() * 3);
    la_runtime_assert(safe_cast<Index>(bitangents.rows()) == mesh.get_num_facets() * 3);
    la_runtime_assert(tangents.cols() == bitangents.cols());
    la_runtime_assert(tangents.cols() == 3 || tangents.cols() == 4);

    // Compute edge information
    logger().trace("Corner to edge mapping");
    IndexArray c2e;
    corner_to_edge_mapping(mesh.get_facets(), c2e);
    logger().trace("Chain corners around edges");
    IndexArray e2c;
    IndexArray next_corner_around_edge;
    chain_corners_around_edges(mesh.get_facets(), c2e, e2c, next_corner_around_edge);
    logger().trace("Chain corners around vertices");
    IndexArray v2c;
    IndexArray next_corner_around_vertex;
    chain_corners_around_vertices(
        mesh.get_num_vertices(),
        mesh.get_facets(),
        v2c,
        next_corner_around_vertex);

    const auto nvpf = mesh.get_vertex_per_facet();
    const auto& vertices = mesh.get_vertices();
    const auto& facets = mesh.get_facets();
    const auto& uv_values = std::get<0>(mesh.get_indexed_attribute("uv"));
    const auto& uv_indices = std::get<1>(mesh.get_indexed_attribute("uv"));
    const auto& normal_values = std::get<0>(mesh.get_indexed_attribute("normal"));
    const auto& normal_indices = std::get<1>(mesh.get_indexed_attribute("normal"));

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

    // Iterate over facets incident to eij until fi and fj are found. The edge is considered smooth
    // if both corners at (eij, fi) and (eij, fj) share the same uv and normal indices.
    auto is_edge_smooth = [&](Index eij, Index fi, Index fj) {
        std::array<Index, 2> v, n, uv;
        std::fill(v.begin(), v.end(), INVALID<Index>());
        for (Index c = e2c[eij]; c != INVALID<Index>(); c = next_corner_around_edge[c]) {
            Index f = c / 3;
            Index lv = c % 3;
            if (f == fi || f == fj) {
                Index v0 = facets(f, lv);
                Index v1 = facets(f, (lv + 1) % 3);
                Index n0 = normal_indices(f, lv);
                Index n1 = normal_indices(f, (lv + 1) % 3);
                Index uv0 = uv_indices(f, lv);
                Index uv1 = uv_indices(f, (lv + 1) % 3);
                if (v[0] == INVALID<Index>()) {
                    v[0] = v0;
                    v[1] = v1;
                    n[0] = n0;
                    n[1] = n1;
                    uv[0] = uv0;
                    uv[1] = uv1;
                    continue;
                }
                if (v0 != v[0]) {
                    std::swap(v0, v1);
                    std::swap(n0, n1);
                    std::swap(uv0, uv1);
                }
                assert(v0 == v[0]);
                return (n0 == n[0] && n1 == n[1] && uv0 == uv[0] && uv1 == uv[1]);
            }
        }
        assert(false);
        return false;
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
    DisjointSets<Index> unified_indices((Index)tangents.rows());
    tbb::parallel_for(Index(0), Index(mesh.get_num_vertices()), [&](Index v) {
        for (Index ci = v2c[v]; ci != INVALID<Index>(); ci = next_corner_around_vertex[ci]) {
            Index eij = c2e[ci];
            Index fi = ci / 3;
            Index lvi = ci % 3;
            Index vi = facets(fi, lvi);
            if (is_face_degenerate(fi)) continue;

            auto si = is_orient_preserving[fi];
            for (Index cj = e2c[eij]; cj != INVALID<Index>(); cj = next_corner_around_edge[cj]) {
                Index fj = cj / 3;
                Index lvj = cj % 3;
                auto sj = is_orient_preserving[fj];
                if (fi == fj) continue;
                if (si != sj) continue;
                Index vj = facets(fj, lvj);
                if (vi != vj) {
                    lvj = (lvj + 1) % 3;
                    vj = facets(fj, lvj);
                    assert(vi == vj);
                }

                if (is_edge_smooth(eij, fi, fj)) {
                    unified_indices.merge(ci, fj * 3 + lvj);
                }
            }
        }
    });

    // STEP 2: Perform averaging and reindex attribute
    logger().trace("Compute new indices");
    const Index num_corners = (Index)tangents.rows();
    std::vector<Index> repr(num_corners, INVALID<Index>());
    Index num_indices = 0;
    for (Index n = 0; n < num_corners; ++n) {
        Index r = unified_indices.find(n);
        if (repr[r] == INVALID<Index>()) {
            repr[r] = num_indices++;
        }
        repr[n] = repr[r];
    }
    logger().trace("Compute offsets");
    std::vector<Index> indices(num_corners);
    std::vector<Index> offsets(num_indices + 1, 0);
    for (Index c = 0; c < num_corners; ++c) {
        offsets[repr[c] + 1]++;
    }
    for (Index r = 1; r <= num_indices; ++r) {
        offsets[r] += offsets[r - 1];
    }
    {
        // Bucket sort for corner indices
        std::vector<Index> counter = offsets;
        for (Index c = 0; c < num_corners; ++c) {
            indices[counter[repr[c]]++] = c;
        }
    }

    logger().trace("Project and average tangent/bitangent");
    FacetArray tangent_indices(mesh.get_num_facets(), 3);
    AttributeArray tangent_values(num_indices, tangents.cols());
    AttributeArray bitangent_values(num_indices, bitangents.cols());
    tangent_values.setZero();
    bitangent_values.setZero();
    tbb::parallel_for(Index(0), Index(offsets.size() - 1), [&](Index r) {
        for (Index i = offsets[r]; i < offsets[r + 1]; ++i) {
            Index c = indices[i];
            Index f = c / 3;
            Index lv = c % 3;
            assert(repr[c] == r);

            // Project tangent frame onto the tangent plane
            RowVector3r nrm = normal_values.row(normal_indices(f, lv));
            RowVector3r t = tangents.row(c).template head<3>();
            RowVector3r bt = bitangents.row(c).template head<3>();
            t = project_on_plane(t, nrm).stableNormalized();
            bt = project_on_plane(bt, nrm).stableNormalized();

            // Compute weight as the angle between the (projected) edges
            RowVector3r v0 = vertices.row(facets(f, lv));
            RowVector3r v1 = vertices.row(facets(f, (lv + 1) % 3));
            RowVector3r v2 = vertices.row(facets(f, (lv + 2) % 3));
            RowVector3r e01 = v1 - v0;
            RowVector3r e02 = v2 - v0;
            Scalar w = projected_angle_between(e01, e02, nrm);

            tangent_values.row(r).template head<3>() += t * w;
            bitangent_values.row(r).template head<3>() += bt * w;
            if (tangents.cols() == 4) {
                if (tangent_values(r, 3) == 0) {
                    tangent_values(r, 3) = tangents(c, 3);
                    bitangent_values(r, 3) = bitangents(c, 3);
                } else {
                    assert(tangent_values(r, 3) == tangents(c, 3));
                    assert(bitangent_values(r, 3) == bitangents(c, 3));
                }
            }

            tangent_indices(f, lv) = r;
        }
    });
    logger().trace("Normalize vectors");
    tbb::parallel_for(Index(0), Index(tangent_values.rows()), [&](Index c) {
        tangent_values.row(c).template head<3>().stableNormalize();
        bitangent_values.row(c).template head<3>().stableNormalize();
    });

    logger().trace("Write attributes");
    FacetArray bitangent_indices = tangent_indices;
    mesh.add_indexed_attribute("tangent");
    mesh.add_indexed_attribute("bitangent");
    mesh.import_indexed_attribute("tangent", tangent_values, tangent_indices);
    mesh.import_indexed_attribute("bitangent", bitangent_values, bitangent_indices);
}

///
/// Computes corner tangents and bitangents.
///
/// If the mesh is quad/mixed quad, a triangle from each quad is used for the computation.
///
/// @param[in,out] mesh           Mesh with UVs initialized and indexed attribute "normal".
/// @param[out]    T              Per-corner tangent vectors.
/// @param[out]    BT             Per-corner bitangent vectors.
/// @param[in]     pad_with_sign  Whether to add a 4th coordinate with the sign of the uv triangles.
///
/// @tparam        MeshType       Type of the mesh.
/// @tparam        DerivedT       Tangent matrix type.
/// @tparam        DerivedB       Bitangent matrix type.
///
template <typename MeshType, typename DerivedT, typename DerivedB>
void corner_tangent_bitangent_raw(
    const MeshType& mesh,
    Eigen::PlainObjectBase<DerivedT>& T,
    Eigen::PlainObjectBase<DerivedB>& BT,
    bool pad_with_sign)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");

    la_runtime_assert(mesh.get_vertices().cols() == 3, "Mesh must be 3D");
    la_runtime_assert(mesh.is_uv_initialized(), "UVs must be initialized");

    using Scalar = typename MeshType::Scalar;
    using Index = typename MeshType::Index;

    const auto& F = mesh.get_facets();
    const auto& V = mesh.get_vertices();
    const auto& UV_val = mesh.get_uv();
    const auto& UV_indices = mesh.get_uv_indices();

    const auto n_faces = mesh.get_num_facets();
    const auto per_facet = mesh.get_vertex_per_facet();

    T.resize(n_faces * per_facet, pad_with_sign ? 4 : 3);
    BT.resize(n_faces * per_facet, pad_with_sign ? 4 : 3);

    // Triangular mesh
    if (per_facet == 3) {
        tbb::parallel_for(Index(0), Index(F.rows()), [&](Index i) {
            const auto& indices = F.row(i);
            Eigen::Matrix<Scalar, 3, 3> vertices;
            vertices.row(0) = V.row(indices(0));
            vertices.row(1) = V.row(indices(1));
            vertices.row(2) = V.row(indices(2));

            Eigen::Matrix<Scalar, 3, 2> uvs;
            uvs.row(0) = UV_val.row(UV_indices(i, 0));
            uvs.row(1) = UV_val.row(UV_indices(i, 1));
            uvs.row(2) = UV_val.row(UV_indices(i, 2));

            bool sign;
            auto T_BT = triangle_tangent_bitangent(vertices, uvs, sign);

            for (Index k = 0; k < per_facet; k++) {
                T.row(i * per_facet + k).template head<3>() = T_BT.row(0);
                BT.row(i * per_facet + k).template head<3>() = T_BT.row(1);
                if (pad_with_sign) {
                    T.row(i * per_facet + k).w() = Scalar(sign ? 1 : -1);
                    BT.row(i * per_facet + k).w() = Scalar(sign ? 1 : -1);
                }
            }
        });
    }
    // Quad or mixed mesh
    else if (per_facet == 4) {
        tbb::parallel_for(Index(0), Index(F.rows()), [&](Index i) {
            const auto& indices = F.row(i);

            bool sign;
            Eigen::Matrix<Scalar, 2, 3> T_BT;

            // Triangle in mixed mesh
            if (indices(3) == INVALID<typename MeshType::Index>()) {
                Eigen::Matrix<Scalar, 3, 3> vertices;
                vertices.row(0) = V.row(indices(0));
                vertices.row(1) = V.row(indices(1));
                vertices.row(2) = V.row(indices(2));

                Eigen::Matrix<Scalar, 3, 2> uvs;
                uvs.row(0) = UV_val.row(UV_indices(i, 0));
                uvs.row(1) = UV_val.row(UV_indices(i, 1));
                uvs.row(2) = UV_val.row(UV_indices(i, 2));

                T_BT = triangle_tangent_bitangent(vertices, uvs, sign);
            }
            // Quad
            else {
                Eigen::Matrix<Scalar, 4, 3> vertices;
                vertices.row(0) = V.row(indices(0));
                vertices.row(1) = V.row(indices(1));
                vertices.row(2) = V.row(indices(2));
                vertices.row(3) = V.row(indices(3));

                Eigen::Matrix<Scalar, 4, 2> uvs;
                uvs.row(0) = UV_val.row(UV_indices(i, 0));
                uvs.row(1) = UV_val.row(UV_indices(i, 1));
                uvs.row(2) = UV_val.row(UV_indices(i, 2));
                uvs.row(3) = UV_val.row(UV_indices(i, 3));

                T_BT = quad_tangent_bitangent(vertices, uvs, sign);
            }

            for (Index k = 0; k < per_facet; k++) {
                if (indices(k) == INVALID<typename MeshType::Index>()) break;
                T.row(i * per_facet + k).template head<3>() = T_BT.row(0);
                BT.row(i * per_facet + k).template head<3>() = T_BT.row(1);
                if (pad_with_sign) {
                    T.row(i * per_facet + k).w() = Scalar(sign ? 1 : -1);
                    BT.row(i * per_facet + k).w() = Scalar(sign ? 1 : -1);
                }
            }
        });
    }
}

} // namespace detail

////////////////////////////////////////////////////////////////////////////////

///
/// Computes corner tangents and bitangents, adds them to mesh as "tangent" and "bitangent"
/// attribute.
///
/// If the mesh is quad/mixed quad, a triangle from each quad is used for the computation.
///
/// @param[in,out] mesh           Mesh with UVs initialized and indexed attribute "normal".
/// @param[in]     pad_with_sign  Whether to add a 4th coordinate with the sign of the uv triangles.
///
/// @tparam        MeshType       Type of the mesh.
///
template <typename MeshType>
void compute_corner_tangent_bitangent(MeshType& mesh, bool pad_with_sign = false)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");

    la_runtime_assert(mesh.get_vertices().cols() == 3, "Mesh must be 3D");
    la_runtime_assert(mesh.is_uv_initialized(), "UVs must be initialized");

    typename MeshType::AttributeArray T;
    typename MeshType::AttributeArray BT;
    detail::corner_tangent_bitangent_raw(mesh, T, BT, pad_with_sign);

    mesh.add_corner_attribute("tangent");
    mesh.add_corner_attribute("bitangent");
    mesh.import_corner_attribute("tangent", T);
    mesh.import_corner_attribute("bitangent", BT);
}

///
/// Computes tangents and bitangents, adds them to mesh as "tangent" and "bitangent" indexed
/// attributes.
///
/// This function only supports triangle meshes.
///
/// @param[in,out] mesh           Mesh with UVs initialized and indexed attribute "normal".
/// @param[in]     pad_with_sign  Whether to add a 4th coordinate with the sign of the uv triangles.
///
/// @tparam        MeshType       Type of the mesh.
///
template <typename MeshType>
void compute_indexed_tangent_bitangent(MeshType& mesh, bool pad_with_sign = false)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");

    la_runtime_assert(mesh.get_vertices().cols() == 3, "Mesh must be 3D");
    la_runtime_assert(mesh.is_uv_initialized(), "UVs must be initialized");
    la_runtime_assert(mesh.get_vertex_per_facet() == 3, "Input must be a triangle mesh");
    la_runtime_assert(mesh.has_indexed_attribute("normal"), "Mesh must have indexed normals");

    typename MeshType::AttributeArray T;
    typename MeshType::AttributeArray BT;
    logger().debug("Compute corner tangent info");
    detail::corner_tangent_bitangent_raw(mesh, T, BT, pad_with_sign);
    logger().debug("Accumulate tangent info");
    detail::accumulate_tangent_bitangent(mesh, T, BT);
}

} // namespace lagrange
