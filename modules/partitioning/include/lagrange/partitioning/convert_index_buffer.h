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
#include <lagrange/utils/safe_cast.h>

#include <Eigen/Core>
////////////////////////////////////////////////////////////////////////////////

namespace lagrange {
namespace partitioning {

///
/// Converts a list of facet indices into flat buffers suitable to be used by METIS.
///
/// @param[in]  facets    #F x k array of face indices.
///
/// @tparam     DerivedF  Facet array type.
///
/// @return     A pair of buffers (eptr, eind) following the data structure explained in Section 5.6
///             of the [METIS manual](http://glaros.dtc.umn.edu/gkhome/fetch/sw/metis/manual.pdf).
///
template <typename DerivedF>
std::pair<std::unique_ptr<index_t[]>, std::unique_ptr<index_t[]>> convert_index_buffer(
    const Eigen::MatrixBase<DerivedF> &facets)
{
    const auto num_elems = safe_cast<index_t>(facets.rows());
    const auto elem_size = safe_cast<index_t>(facets.cols());

    auto e_ptr = std::unique_ptr<index_t[]>(new index_t[num_elems + 1]);
    auto e_ind = std::unique_ptr<index_t[]>(new index_t[num_elems * elem_size]);
    for (index_t f = 0; f < num_elems; ++f) {
        e_ptr[f] = f * elem_size;
        for (index_t lv = 0; lv < elem_size; ++lv) {
            e_ind[f * elem_size + lv] = facets(f, lv);
        }
    }
    e_ptr[num_elems] = num_elems * elem_size;
    return std::make_pair(std::move(e_ptr), std::move(e_ind));
}

} // namespace partitioning
} // namespace lagrange
