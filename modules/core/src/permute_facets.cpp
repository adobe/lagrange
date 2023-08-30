/*
 * Copyright 2023 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/foreach_attribute.h>
#include <lagrange/permute_facets.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/invalid.h>
#include <lagrange/views.h>

#include <Eigen/Core>

#include <algorithm>

namespace lagrange {

template <typename Scalar, typename Index>
void permute_facets(SurfaceMesh<Scalar, Index>& mesh, span<const Index> new_to_old)
{
    la_runtime_assert(mesh.get_num_facets() == static_cast<Index>(new_to_old.size()));
    const Index num_facets = mesh.get_num_facets();
    const Index num_corners = mesh.get_num_corners();

    constexpr int invalid_index = -1; // Eigen::PermutationMatrix::Scalar is int.

    // Facet permutation.
    Eigen::PermutationMatrix<Eigen::Dynamic> P_facet(num_facets);
    auto& old_to_new = P_facet.indices();
    old_to_new.setConstant(invalid_index);
    for (Index i = 0; i < num_facets; i++) {
        la_runtime_assert(new_to_old[i] < num_facets, "`new_to_old` index out of bound!");
        old_to_new[new_to_old[i]] = static_cast<int>(i);
    }
    la_runtime_assert(
        std::find(old_to_new.begin(), old_to_new.end(), invalid_index) == old_to_new.end(),
        "`new_to_old` is not a valid permutation of [0, ..., num_facets-1]!");

    // Corner permutation.
    Eigen::PermutationMatrix<Eigen::Dynamic> P_corner(num_corners);
    auto& old_to_new_corner = P_corner.indices();
    Index cid = 0;
    for (Index fid = 0; fid < num_facets; fid++) {
        Index old_fid = new_to_old[fid];
        Index vertex_per_facet = mesh.get_facet_size(old_fid);
        for (Index j = 0; j < vertex_per_facet; j++) {
            Index old_cid = mesh.get_facet_corner_begin(old_fid) + j;
            old_to_new_corner[old_cid] = static_cast<int>(cid);
            cid++;
        }
    }
    la_debug_assert(
        std::find(old_to_new_corner.begin(), old_to_new_corner.end(), invalid_index) ==
            old_to_new_corner.end(),
        "`new_to_old_corner` is not a valid permutation of [0, ..., num_corners-1]!");

    // Update facet and corner index attributes.
    par_foreach_named_attribute_read(mesh, [&](std::string_view name, auto&& attr) {
        using AttributeType = std::decay_t<decltype(attr)>;
        using ValueType = typename AttributeType::ValueType;
        if (attr.get_usage() == AttributeUsage::FacetIndex) {
            auto& attr_ref = mesh.template ref_attribute<ValueType>(name);
            auto data = attr_ref.ref_all();
            std::transform(data.begin(), data.end(), data.begin(), [&](ValueType i) {
                return static_cast<ValueType>(old_to_new[static_cast<Eigen::Index>(i)]);
            });
        } else if (attr.get_usage() == AttributeUsage::CornerIndex) {
            auto& attr_ref = mesh.template ref_attribute<ValueType>(name);
            auto data = attr_ref.ref_all();
            std::transform(data.begin(), data.end(), data.begin(), [&](ValueType i) {
                // Some corner index attributes such as next-corner-around-edge uses invalid index
                // to mark the end of the list.
                if (i == invalid<ValueType>()) return i;
                return static_cast<ValueType>(old_to_new_corner[static_cast<Eigen::Index>(i)]);
            });
        }
    });

    // Permute runs in O(n) time.
    auto permute_facet = [&](auto&& attr) {
        auto data = matrix_ref(attr);
        data = P_facet * data;
    };
    auto permute_corner = [&](auto&& attr) {
        auto data = matrix_ref(attr);
        data = P_corner * data;
    };

    // Permute facet attributes (facet-to-first-corner attribute included).
    par_foreach_attribute_write<AttributeElement::Facet>(mesh, permute_facet);
    // Permute corner attributes (corner-to-vertex attribute included).
    par_foreach_attribute_write<AttributeElement::Corner>(mesh, permute_corner);
}

#define LA_X_permute_facets(_, Scalar, Index) \
    template void permute_facets<Scalar, Index>(SurfaceMesh<Scalar, Index>&, span<const Index>);
LA_SURFACE_MESH_X(permute_facets, 0)

} // namespace lagrange

