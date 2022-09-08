/*
 * Copyright 2018 Adobe. All rights reserved.
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

#include <cassert>

#include <lagrange/Edge.h>
#include <lagrange/common.h>

namespace lagrange {

template <typename DerivedF>
EdgeType<typename DerivedF::Scalar> get_opposite_edge(
    const Eigen::PlainObjectBase<DerivedF>& facets,
    typename DerivedF::Scalar fid,
    typename DerivedF::Scalar vid)
{
    using Edge = EdgeType<typename DerivedF::Scalar>;
    assert(facets.cols() == 3);
    const auto& f = facets.row(fid);
    if (f[0] == vid) {
        return Edge(f[1], f[2]);
    } else if (f[1] == vid) {
        return Edge(f[2], f[0]);
    } else if (f[2] == vid) {
        return Edge(f[0], f[1]);
    } else {
        throw std::runtime_error(
            "Facet " + std::to_string(fid) + " does not contain vertex " + std::to_string(vid));
    }
}
} // namespace lagrange
