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
#pragma once

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
    #include <lagrange/legacy/select_facets_in_frustum.h>
#endif

#include <lagrange/SurfaceMesh.h>
#include <array>

namespace lagrange {

///
/// @defgroup   group-surfacemesh-utils Mesh utilities
/// @ingroup    group-surfacemesh
///
/// Various mesh processing utilities
///
/// @{

///
/// An array of four planes that define a frustum.
///
template <typename Scalar>
struct Frustum
{
    /// A plane defined by a normal and a point.
    struct Plane
    {
        std::array<Scalar, 3> normal;
        std::array<Scalar, 3> point;
    };

    /// Four planes that define a frustum.
    std::array<Plane, 4> planes;
};

///
/// Option struct for selecting facets.
///
struct FrustumSelectionOptions
{
    /// If true, then select_facets_in_frustum will stop after it finds the first facet.
    bool greedy = false;

    /// The output attribute name for the selection.
    /// This attribute will be of type uint8_t and is only created/updated if greedy is false.
    std::string_view output_attribute_name = "@is_selected";
};

/**
 * Select all facets that intersect the cone/frustrum bounded by 4 planes
 * defined by (n_i, p_i), where n_i is the plane normal and p_i is a point on
 * the plane.
 *
 * @param[in, out]  mesh        The input mesh.
 * @param[in]       frustum     A collection of four planes.
 * @param[in]       options     Optional arguments (greedy, output_attribute_name).
 *
 * @tparam          Scalar      Mesh scalar type.
 * @tparam          Index       Mesh index type.
 *
 * @return          bool        Whether any facet is selected.
 *
 * @note            When `options.greedy` is true, this function returns as soon as the first facet
 *                  is selected.
 *
 * @post            If `options.greedy` is false, the computed selection is stored in `mesh` as a
 *                  facet attribute named `options.output_attribute_name`.
 *
 * @see             Frustum and FrustumSelectionOptions.
 */
template <typename Scalar, typename Index>
bool select_facets_in_frustum(
    SurfaceMesh<Scalar, Index>& mesh,
    const Frustum<Scalar>& frustum,
    const FrustumSelectionOptions& options = {});

/// @}

} // namespace lagrange
