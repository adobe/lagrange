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
#include <lagrange/permute_vertices.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/invalid.h>
#include <lagrange/views.h>

#include <Eigen/Core>

#include <algorithm>

namespace lagrange {

template <typename Scalar, typename Index>
void permute_vertices(SurfaceMesh<Scalar, Index>& mesh, span<const Index> new_to_old)
{
    la_runtime_assert(mesh.get_num_vertices() == (Index)(new_to_old.size()));
    const Index num_vertices = mesh.get_num_vertices();
    constexpr int invalid_index = -1; // Eigen::PermutationMatrix::Scalar is int.

    // Initialize permutation matrix
    Eigen::PermutationMatrix<Eigen::Dynamic> P(num_vertices);
    auto& old_to_new = P.indices();
    old_to_new.setConstant(invalid_index);
    for (Index i = 0; i < num_vertices; i++) {
        la_runtime_assert(new_to_old[i] < num_vertices, "`new_to_old` index out of bound!");
        old_to_new[new_to_old[i]] = static_cast<int>(i);
    }
    la_runtime_assert(
        std::find(old_to_new.begin(), old_to_new.end(), invalid_index) == old_to_new.end(),
        "`new_to_old` is not a valid permutation of [0, ..., num_vertices-1]!");
    par_foreach_named_attribute_read(mesh, [&](std::string_view name, auto&& attr) {
        using AttributeType = std::decay_t<decltype(attr)>;
        using ValueType = typename AttributeType::ValueType;
        if (attr.get_usage() == AttributeUsage::VertexIndex) {
            auto& attr_ref = mesh.template ref_attribute<ValueType>(name);
            auto data = attr_ref.ref_all();
            std::transform(data.begin(), data.end(), data.begin(), [&](ValueType i) {
                return static_cast<ValueType>(old_to_new[static_cast<Eigen::Index>(i)]);
            });
        }
    });

    // Permute runs in O(n) time.
    auto permute = [&](auto&& attr) {
        auto data = matrix_ref(attr);
        data = P * data;
    };

    // Permute vertex attributes (positions included).
    par_foreach_attribute_write<AttributeElement::Vertex>(mesh, permute);

    // Facet, corner, edge and indexed attributes are all unchanged.
}

#define LA_X_permute_vertices(_, Scalar, Index) \
    template LA_CORE_API void permute_vertices<Scalar, Index>(SurfaceMesh<Scalar, Index>&, span<const Index>);
LA_SURFACE_MESH_X(permute_vertices, 0)

} // namespace lagrange
