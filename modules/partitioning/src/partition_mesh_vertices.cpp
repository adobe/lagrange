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

namespace lagrange {
namespace partitioning {

Eigen::Matrix<index_t, Eigen::Dynamic, 1> partition_mesh_vertices_raw(
    index_t num_elems,
    index_t num_nodes,
    index_t *e_ptr,
    index_t *e_ind,
    index_t num_partitions)
{
    static_assert(
        std::is_same<::lagrange::partitioning::index_t, idx_t>::value,
        "Index types don't match");

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
        e_ptr,
        e_ind,
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
        partitions(i) = n_part[i];
    }
    return partitions;
}

} // namespace partitioning
} // namespace lagrange
