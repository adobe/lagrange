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

#include <lagrange/partitioning/types.h>

#include <lagrange/utils/function_ref.h>
#include <lagrange/utils/safe_cast.h>
#include <lagrange/utils/span.h>

#include <Eigen/Core>
////////////////////////////////////////////////////////////////////////////////

namespace lagrange {
namespace partitioning {

namespace internal {

///
/// Low-level function wrapping partitioning call to METIS.
///
/// @param[in]  num_elems       Number of elements in the mesh.
/// @param[in]  num_nodes       Number of nodes in the mesh.
/// @param[in]  elem_size       Number of node per element.
/// @param[in]  copy_32         Callback function to copy element indices into a 32bit index buffer.
/// @param[in]  copy_64         Callback function to copy element indices into a 64bit index buffer.
/// @param[in]  num_partitions  Number of partitions to produce.
///
/// @return     #V x 1 array of partition ids.
///
Eigen::Matrix<index_t, Eigen::Dynamic, 1> partition_mesh_vertices_raw(
    index_t num_elems,
    index_t num_nodes,
    index_t elem_size,
    function_ref<void(span<int32_t>)> copy_32,
    function_ref<void(span<int64_t>)> copy_64,
    index_t num_partitions);

} // namespace internal

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
    const Eigen::MatrixBase<DerivedF>& facets,
    index_t num_partitions)
{
    const auto num_elems = safe_cast<index_t>(facets.rows());
    const auto num_nodes = safe_cast<index_t>(facets.maxCoeff() + 1);
    const auto elem_size = safe_cast<index_t>(facets.cols());
    auto copy_func = [&](auto dst) {
        using value_t = typename std::decay_t<decltype(dst)>::value_type;
        for (Eigen::Index f = 0; f < facets.rows(); ++f) {
            for (Eigen::Index lv = 0; lv < facets.cols(); ++lv) {
                dst[f * facets.cols() + lv] = static_cast<value_t>(facets(f, lv));
            }
        }
    };
    auto copy_32 = [&](span<int32_t> dst) { copy_func(dst); };
    auto copy_64 = [&](span<int64_t> dst) { copy_func(dst); };
    return internal::partition_mesh_vertices_raw(
        num_elems,
        num_nodes,
        elem_size,
        copy_32,
        copy_64,
        num_partitions);
}

} // namespace partitioning
} // namespace lagrange
