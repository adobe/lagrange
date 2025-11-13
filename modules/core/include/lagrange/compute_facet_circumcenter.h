/*
 * Copyright 2025 Adobe. All rights reserved.
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

#include <lagrange/SurfaceMesh.h>

namespace lagrange {

struct FacetCircumcenterOptions
{
    /// Output facet circumcenter attribute name.
    std::string_view output_attribute_name = "@facet_circumcenter";
};

///
/// Compute per-facet circumcenter.
///
/// @param[in,out] mesh     Input mesh.
/// @param[in]     options  Option settings to control the computation.
///
/// @return Attribute ID of the facet circumcenters.
///
template <typename Scalar, typename Index>
AttributeId compute_facet_circumcenter(
    SurfaceMesh<Scalar, Index>& mesh,
    FacetCircumcenterOptions options = {});

} // namespace lagrange
