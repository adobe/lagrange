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
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <Eigen/Core>
////////////////////////////////////////////////////////////////////////////////

namespace lagrange {

///
/// Computes a mapping from mesh corners (k*f+i) to unique edge ids.
///
/// @param[in]  F         #F x k matrix of facet indices.
/// @param[out] C2E       #F*k vector of unique edge ids per corner.
///
/// @tparam     DerivedF  Type of facet array.
/// @tparam     DerivedC  Type of corner to edge vector.
///
/// @return     Number of unique edges created.
///
template <typename DerivedF, typename DerivedC>
Eigen::Index corner_to_edge_mapping(
    const Eigen::MatrixBase<DerivedF>& F,
    Eigen::PlainObjectBase<DerivedC>& C2E);

} // namespace lagrange

// Templated definitions
#include <lagrange/corner_to_edge_mapping.impl.h>
