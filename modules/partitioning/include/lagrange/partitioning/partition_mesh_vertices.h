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

#include <lagrange/partitioning/convert_index_buffer.h>
#include <lagrange/utils/safe_cast.h>

#include <Eigen/Core>
////////////////////////////////////////////////////////////////////////////////

namespace lagrange {
namespace partitioning {

///
/// Low-level function wrapping partitioning call to METIS.
///
/// @param[in]     num_elems       Number of elements in the mesh.
/// @param[in]     num_nodes       Number of nodes in the mesh.
/// @param[in,out] e_ptr           Offset array indicating where each element index starts.
/// @param[in,out] e_ind           Flat array of vertex indices for each element.
/// @param[in]     num_partitions  Number of partitions to produce.
///
/// @return        #V x 1 array of partition ids.
///
Eigen::Matrix<index_t, Eigen::Dynamic, 1> partition_mesh_vertices_raw(
    index_t num_elems,
    index_t num_nodes,
    index_t *e_ptr,
    index_t *e_ind,
    index_t num_partitions);

///
/// Partition mesh vertices into num_partitions using METIS.
///
/// @param[in]  facets          #F x k array of facet indices.
/// @param[in]  num_partitions  Number of partitions to produce.
///
/// @tparam     DerivedF        Type of facet array.
///
/// @return     #V x 1 array of partition ids.
///
template <typename DerivedF>
Eigen::Matrix<index_t, Eigen::Dynamic, 1> partition_mesh_vertices(
    const Eigen::MatrixBase<DerivedF> &facets,
    index_t num_partitions)
{
    const auto num_elems = safe_cast<index_t>(facets.rows());
    const auto num_nodes = safe_cast<index_t>(facets.maxCoeff() + 1);
    auto res = convert_index_buffer(facets);
    return partition_mesh_vertices_raw(
        num_elems,
        num_nodes,
        res.first.get(),
        res.second.get(),
        num_partitions);
}

} // namespace partitioning
} // namespace lagrange
