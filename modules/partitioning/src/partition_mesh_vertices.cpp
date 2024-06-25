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
#include <lagrange/partitioning/partition_mesh_vertices.h>

#include <lagrange/Logger.h>
#include <lagrange/utils/safe_cast.h>

#include <type_traits>
////////////////////////////////////////////////////////////////////////////////

namespace {

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <metis.h>
#include <lagrange/utils/warnon.h>
// clang-format on

} // namespace

namespace lagrange::partitioning::internal {

Eigen::Matrix<index_t, Eigen::Dynamic, 1> partition_mesh_vertices_raw(
    index_t num_elems_,
    index_t num_nodes_,
    index_t elem_size_,
    function_ref<void(span<int32_t>)> copy_32,
    function_ref<void(span<int64_t>)> copy_64,
    index_t num_partitions_)
{
    static_assert(
        std::is_signed_v<index_t> == std::is_signed_v<idx_t>,
        "Index type signedness mismatch");

#if IDXTYPEWIDTH == 32
    static_assert(std::is_same_v<idx_t, int32_t>, "Idx_t should be 32bit");
#elif IDXTYPEWIDTH == 64
    static_assert(std::is_same_v<idx_t, int64_t>, "Idx_t should be 64bit");
#endif

    idx_t num_elems = safe_cast<idx_t>(num_elems_);
    idx_t num_nodes = safe_cast<idx_t>(num_nodes_);
    idx_t elem_size = safe_cast<idx_t>(elem_size_);
    idx_t num_partitions = safe_cast<idx_t>(num_partitions_);

    // Copy input buffers
    auto e_ptr = std::unique_ptr<idx_t[]>(new idx_t[num_elems + 1]);
    for (idx_t f = 0; f < num_elems; ++f) {
        e_ptr[f] = f * elem_size;
    }
    e_ptr[num_elems] = num_elems * elem_size;
    auto e_ind = std::unique_ptr<idx_t[]>(new idx_t[num_elems * elem_size]);

    // I tried using if constexpr (std::is_same_v<idx_t, int32_t>), but this would result in a
    // compile error. Could be a bug in the clang compiler?
#if IDXTYPEWIDTH == 32
    copy_32(span<int32_t>(e_ind.get(), num_elems * elem_size));
    (void)copy_64;
#elif IDXTYPEWIDTH == 64
    copy_64(span<int64_t>(e_ind.get(), num_elems * elem_size));
    (void)copy_32;
#else
    static_assert("Unsupported index size");
#endif

    // Sanity check
    Eigen::Matrix<index_t, Eigen::Dynamic, 1> partitions(num_nodes);
    if (num_partitions <= 1) {
        logger().warn("<= 1 partition was requested, skipping partitioning.");
        partitions.setZero();
        return partitions;
    }

    // Outputs
    idx_t objval = 0;
    auto e_part = std::unique_ptr<idx_t[]>(new idx_t[num_elems]);
    auto n_part = std::unique_ptr<idx_t[]>(new idx_t[num_nodes]);
    logger().info("Num parts: {}", num_partitions);

    // Do the job. Lots of options, stuff is explained here:
    // http://glaros.dtc.umn.edu/gkhome/fetch/sw/metis/manual.pdf
    auto err = METIS_PartMeshNodal(
        &num_elems,
        &num_nodes,
        e_ptr.get(),
        e_ind.get(),
        nullptr, // vwgt
        nullptr, // vsize
        &num_partitions,
        nullptr, // tpwgts
        nullptr, // options
        &objval,
        e_part.get(),
        n_part.get());

    // Error handling
    std::string message;
    switch (err) {
    case METIS_OK:
        logger().debug(
            "[partitioning] Computed {} partitions with total score of {}",
            num_partitions,
            objval);
        break;
    case METIS_ERROR_INPUT: message = "[partitioning] Invalid input."; break;
    case METIS_ERROR_MEMORY: message = "[partitioning] Ran out of memory."; break;
    case METIS_ERROR:
    default: message = "[partitioning] Ran out of memory."; break;
    }
    if (!message.empty()) {
        logger().error("{}", message);
        throw std::runtime_error(message);
    }

    // Convert back output
    for (idx_t i = 0; i < num_nodes; ++i) {
        partitions(i) = static_cast<index_t>(n_part[i]);
    }
    return partitions;
}

} // namespace lagrange::partitioning::internal
