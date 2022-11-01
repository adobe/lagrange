/*
 * Copyright 2022 Adobe. All rights reserved.
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

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
    #include <lagrange/legacy/compute_vertex_valence.h>
#endif

#include <string_view>

#include <lagrange/SurfaceMesh.h>

namespace lagrange {

/**
 * Option struct for computing vertex valence.
 */
struct VertexValenceOptions
{
    /// Output vertex valence attribute name.
    std::string_view output_attribute_name = "@vertex_valence";
};

/**
 * Compute vertex valence.
 *
 * @param mesh     The input mesh.
 * @param options  Optional settings to control valence computation.
 *
 * @tparam Scalar  Mesh scalar type.
 * @tparam Index   Mesh index type.
 *
 * @return         The vertex attribute id containing valence information.
 *
 * @see `VertexValenceOptions`
 */
template <typename Scalar, typename Index>
AttributeId compute_vertex_valence(
    SurfaceMesh<Scalar, Index>& mesh,
    VertexValenceOptions options = {});

} // namespace lagrange
