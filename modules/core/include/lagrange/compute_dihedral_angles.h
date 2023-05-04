/*
 * Copyright 2019 Adobe. All rights reserved.
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
    #include <lagrange/legacy/compute_dihedral_angles.h>
#endif

#include <string_view>

#include <lagrange/SurfaceMesh.h>

namespace lagrange {

///
/// @defgroup   group-surfacemesh-utils Mesh utilities
/// @ingroup    group-surfacemesh
///
/// Various mesh processing utilities.
///
/// @{

///
/// Option struct for computing dihedral angles.
///
struct DihedralAngleOptions
{
    /// Output attribute name. If the attribute already exists, it will be overwritten.
    std::string_view output_attribute_name = "@dihedral_angle";

    /// Precomputed facet normal attribute name. If the attribute does not exist, the algorithm will
    /// compute it.
    std::string_view facet_normal_attribute_name = "@facet_normal";

    /// Whether to recompute the facet normal attribute, or reuse existing cached values if present.
    bool recompute_facet_normals = false;

    /// Whether to keep any newly added facet normal attribute. If such an attribute is already
    /// present in the input mesh, it will not be removed, even if this argument is set to false.
    bool keep_facet_normals = false;
};

///
/// Computes dihedral angles for each edge in the mesh.
///
/// The dihedral angle of an edge is defined as the angle between the __normals__ of two facets
/// adjacent to the edge. The dihedral angle is always in the range @f$[0, \pi]@f$ for manifold
/// edges. For boundary edges, the dihedral angle defaults to 0.  For non-manifold edges, the
/// dihedral angle is not well-defined and will be set to the special value @f$ 2\pi @f$.
///
/// @tparam Scalar      Mesh scalar type
/// @tparam Index       Mesh index type
///
/// @param[in] mesh     The input mesh.
/// @param[in] options  Options for computing dihedral angles.
///
/// @return             The id of the dihedral angle attribute.
///
/// @see DihedralAngleOptions
///
template <typename Scalar, typename Index>
AttributeId compute_dihedral_angles(
    SurfaceMesh<Scalar, Index>& mesh,
    const DihedralAngleOptions& options = {});

/// @}
} // namespace lagrange
