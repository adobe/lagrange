/*
 * Copyright 2024 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/SurfaceMesh.h>

namespace lagrange {

///
/// Options for computing seam edges.
///
struct SeamEdgesOptions
{
    /// Output attribute name.
    std::string_view output_attribute_name = "@seam_edges";
};

///
/// Computes the seam edges for a given indexed attribute. A seam edge is an edge where either of
/// its endpoint vertex has corners whose attribute index differ accross the edge.
///
/// @param[in,out] mesh                  Input mesh. Modified to add edge information and compute
///                                      output seam edges.
/// @param[in]     indexed_attribute_id  Id of the indexed attribute for which to compute seam
///                                      edges.
/// @param[in]     options               Optional parameters.
///
/// @tparam        Scalar                Mesh scalar type.
/// @tparam        Index                 Mesh index type.
///
/// @return        Id of the newly added per-edge attribute. The newly computed attribute will have
///                value type `uint8_t`, and contain 0 for non-seam edges, and 1 for seam edges.
///
template <typename Scalar, typename Index>
AttributeId compute_seam_edges(
    SurfaceMesh<Scalar, Index>& mesh,
    AttributeId indexed_attribute_id,
    const SeamEdgesOptions& options = {});

} // namespace lagrange
