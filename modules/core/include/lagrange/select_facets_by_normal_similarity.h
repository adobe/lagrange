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
    #include <lagrange/legacy/select_facets_by_normal_similarity.h>
#endif
#include <limits> // needed in options
#include <optional> // needed in options

namespace lagrange {


///
/// @defgroup   group-surfacemesh-utils Mesh utilities
/// @ingroup    group-surfacemesh
///
/// Various mesh processing utilities
///
/// @{

///
/// Option struct for selecting facets based on normal similarity.
///
struct SelectFacetsByNormalSimilarityOptions
{
    /// Increasing this would select a larger region
    double flood_error_limit = std::numeric_limits<double>::max();

    ///
    /// There are two types of error limits when flood-search goes from one facet to its neighboring
    /// facet:
    ///
    /// 1. first-order-limit  := difference(seed normal, neighboring facet normal)
    /// 2. second-order-limit := difference(the one facet's normal, neighboring facet normal)
    ///
    /// Setting flood_second_to_first_order_limit_ratio > 0.0 allows the selected region to grow on
    /// low-curvature areas even though the normals differ from the seed normal.
    double flood_second_to_first_order_limit_ratio = 1.0 / 6.0;

    /// The attribute name for the facet normal
    std::string_view facet_normal_attribute_name = "@facet_normal";

    /// Users can specify whether a facet is selectable by an uint8 attribute.
    /// e.g. "@is_facet_selectable"
    std::optional<std::string_view> is_facet_selectable_attribute_name;

    /// The attribute name for the selection output
    std::string_view output_attribute_name = "@is_selected";

    /// select_facets_by_normal_similarity uses either BFS or DFS in its flooding search
    enum class SearchType {
        BFS = 0, ///< Breadth-First Search
        DFS = 1 ///< Depth-First Search
    };

    /// The search type (BFS or DFS)
    SearchType search_type = SearchType::DFS;

    /// Smoothing boundary of selected region (reduce ears)
    int num_smooth_iterations = 3;


}; // SelectFaceByNormalSimilarityOptions

/**
 * Given a seed facet, selects facets around it based on the change in triangle normals.
 *
 * @param[in, out] mesh           The input mesh.
 * @param[in]      seed_facet_id  The index of the seed facet
 * @param[in]      options        Optional arguments (greedy, output_attribute_name).
 *
 * @tparam         Scalar         Mesh scalar type.
 * @tparam         Index          Mesh index type.
 *
 * @return         AttributeId    The attribute id of the selection, whose attribute name is given
 *                 by options.output_attribute_name.
 *
 * @note           Currently only support triangular mesh and will throw error if
 *                 mesh.is_triangle_mesh() is false! The function will check if the mesh contains
 *                 facet normal by looking for options.facet_normal_attribute_name, and if not
 *                 found, will call compute_facet_normal(meshm,
 *                 options.facet_normal_attribute_name).
 *
 * @see            @ref SelectFacetsByNormalSimilarityOptions
 */

template <typename Scalar, typename Index>
AttributeId select_facets_by_normal_similarity(
    SurfaceMesh<Scalar, Index>& mesh,
    const Index seed_facet_id,
    const SelectFacetsByNormalSimilarityOptions& options = {});

/// @}

} // namespace lagrange
