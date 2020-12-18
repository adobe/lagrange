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

#include <string>

#include <Eigen/Dense>

#include <lagrange/fs/filesystem.h>

namespace lagrange {

///
/// Saves a 2D undirected graph (V,E) using off format.
///
/// @param[in]  filename  Target filename path.
/// @param[in]  V         #V x 2 array of vertices positions.
/// @param[in]  E         #E x 2 array of edge indices.
///
/// @tparam     DerivedV  Vertex array type.
/// @tparam     DerivedE  Edge array type.
///
template <typename DerivedV, typename DerivedE>
void save_graph_off(
    const fs::path &filename,
    const Eigen::MatrixBase<DerivedV> &V,
    const Eigen::MatrixBase<DerivedE> &E)
{
    using namespace Eigen;
    fs::ofstream out(filename);
    if (!out.is_open()) {
        throw std::runtime_error("failed to open file " + filename.string());
    }
    Eigen::MatrixXd VV = V.template cast<double>();
    VV.conservativeResize(V.rows(), 3);
    VV.col(2).setZero();
    out << "OFF\n"
        << VV.rows() << " " << E.rows() << " 0\n"
        << VV.format(IOFormat(FullPrecision, DontAlignCols, " ", "\n", "", "", "", "\n"))
        << (E.array()).format(
               IOFormat(FullPrecision, DontAlignCols, " ", "\n", "2 ", "", "", "\n"));
}

///
/// Saves a 2D undirected graph (V,E) based on filename extension. For now only .off is supported.
///
/// @param[in]  filename  Target filename path.
/// @param[in]  V         #V x 2 array of vertices positions.
/// @param[in]  E         #E x 2 array of edge indices.
///
/// @tparam     DerivedV  Vertex array type.
/// @tparam     DerivedE  Edge array type.
///
template <typename DerivedV, typename DerivedE>
void save_graph(
    const fs::path &filename,
    const Eigen::MatrixBase<DerivedV> &V,
    const Eigen::MatrixBase<DerivedE> &E)
{
    if (filename.extension() == ".off") {
        save_graph_off(filename, V, E);
    } else {
        throw std::runtime_error("Unsupported file extension");
    }
}

} // namespace lagrange
