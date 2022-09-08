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

#include <lagrange/Mesh.h>
#include <lagrange/MeshTrait.h>

#include <igl/adjacency_list.h>
#include <igl/barycenter.h>
#include <igl/triangle_triangle_adjacency.h>

namespace lagrange {
namespace subdivision {

///
/// Performs one step of sqrt(3)-subdivision. Implementation based on:
///
/// <BLOCKQUOTE> Kobbelt, Leif. "√3-subdivision." Proceedings of the 27th annual conference on
/// Computer graphics and interactive techniques. 2000. https://doi.org/10.1145/344779.344835
/// </BLOCKQUOTE>
///
/// @param[in]  VI         #VI x 3 input vertices positions.
/// @param[in]  FI         #FI x 3 input facet indices.
/// @param[out] VO         #VO x 3 output vertices positions.
/// @param[out] FO         #FO x 3 output facet indices.
///
template <typename DerivedVI, typename DerivedFI, typename DerivedVO, typename DerivedFO>
void sqrt_subdivision(
    const Eigen::MatrixBase<DerivedVI> &VI,
    const Eigen::MatrixBase<DerivedFI> &FI,
    Eigen::PlainObjectBase<DerivedVO> &VO,
    Eigen::PlainObjectBase<DerivedFO> &FO)
{
    using Scalar = typename DerivedVI::Scalar;
    using Index = typename DerivedFI::Scalar;
    using MatrixXs = Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;
    using MatrixXi = Eigen::Matrix<Index, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;
    using RowVector3s = Eigen::Matrix<Scalar, 1, 3>;

    // Step 1: new vertices at barycenter of every face
    MatrixXs BC;
    igl::barycenter(VI, FI, BC);

    // Step 2: move old vertices to new positions
    VO.resize(VI.rows() + FI.rows(), VI.cols());
    VO.topRows(VI.rows()) = VI;
    VO.bottomRows(BC.rows()) = BC;
    std::vector<std::vector<Index> > VV;
    igl::adjacency_list(FI, VV);
    for (Index i = 0; i < (Index) VI.rows(); ++i) {
        Scalar n = static_cast<Scalar>(VV[i].size());
        Scalar an = static_cast<Scalar>((4.0 - 2.0 * std::cos(2.0 * M_PI / n)) / 9.0);
        RowVector3s sum_vi = RowVector3s::Zero();
        for (auto j : VV[i]) {
            sum_vi += VI.row(j);
        }
        VO.row(i) = (1 - an) * VI.row(i) + an / n * sum_vi;
    }

    // Step 3: compute a new set of faces, and flip edges as required
    FO.resize(3 * FI.rows(), FI.cols());
    for (Index f = 0; f < (Index) FI.rows(); ++f) {
        FO.row(3 * f + 0) << VI.rows() + f, FI(f, 0), FI(f, 1);
        FO.row(3 * f + 1) << VI.rows() + f, FI(f, 1), FI(f, 2);
        FO.row(3 * f + 2) << VI.rows() + f, FI(f, 2), FI(f, 0);
    }
    MatrixXi TT, TTi;
    igl::triangle_triangle_adjacency(FI, TT, TTi);
    for (Index f = 0; f < (Index) FI.rows(); ++f) {
        for (Index i = 0; i < (Index) FI.cols(); ++i) {
            if (TT(f, i) != static_cast<Index>(-1) && TT(f, i) > f) {
                Index g = TT(f, i);
                Index j = TTi(f, i);
                FO(3 * f + i, 2) = VI.rows() + g;
                FO(3 * g + j, 2) = VI.rows() + f;
            }
        }
    }
}

///
/// Performs one step of sqrt(3)-subdivision. Implementation based on:
///
/// <BLOCKQUOTE> Kobbelt, Leif. "√3-subdivision." Proceedings of the 27th annual conference on
/// Computer graphics and interactive techniques. 2000. https://doi.org/10.1145/344779.344835
/// </BLOCKQUOTE>
///
/// @note       This function currently does not remap any mesh attribute.
///
/// @param[in]  mesh      Input mesh to subdivide.
///
/// @tparam     MeshType  Mesh type.
///
/// @return     Subdivided mesh.
///
template <typename MeshType>
std::unique_ptr<MeshType> sqrt_subdivision(const MeshType &mesh)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");

    using VertexArray = typename MeshType::VertexArray;
    using FacetArray = typename MeshType::FacetArray;

    VertexArray V;
    FacetArray F;
    sqrt_subdivision(mesh.get_vertices(), mesh.get_facets(), V, F);
    return lagrange::create_mesh(V, F);
}

} // namespace subdivision
} // namespace lagrange
