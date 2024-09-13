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
#include <lagrange/AttributeTypes.h>
#include <lagrange/IndexedAttribute.h>
#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/internal/invert_mapping.h>
#include <lagrange/internal/visit_attribute.h>
#include <lagrange/utils/DisjointSets.h>
#include <lagrange/utils/SmallVector.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/safe_cast.h>
#include <lagrange/views.h>
#include <lagrange/weld_indexed_attribute.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <tbb/parallel_for.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <algorithm>
#include <limits>

namespace lagrange {

namespace {

template <typename ValueType, typename Scalar, typename Index, typename Func>
void weld_indexed_attribute(
    SurfaceMesh<Scalar, Index>& mesh,
    IndexedAttribute<ValueType, Index>& attr,
    Func equal)
{
    mesh.initialize_edges();
    auto& attr_values = attr.values();
    auto values = matrix_view(attr_values);
    auto indices = matrix_ref(attr.indices());

    const Index num_vertices = mesh.get_num_vertices();
    const Index num_values = static_cast<Index>(values.rows());

    // Sort and find duplicate rows
    std::vector<Index> index_map(num_values);
    std::iota(index_map.begin(), index_map.end(), 0);
    tbb::parallel_for((Index)0, num_vertices, [&](Index vi) {
        SmallVector<Index, 16> involved_indices;
        mesh.foreach_corner_around_vertex(vi, [&](Index ci) {
            involved_indices.push_back(indices(ci));
        });
        la_debug_assert(involved_indices.size() > 0);

        // Extract unique indices.
        auto first = involved_indices.begin();
        auto last = involved_indices.end();
        std::sort(first, last);
        last = std::unique(first, last);

        // Extract unique values.
        auto num_unique_indices = std::distance(first, last);
        if (num_unique_indices <= 1) return;

        // TODO: This is not thread-safe if corners from multiple vertices are assigned the same
        // indices (e.g. a cube where each facet is assigned the same uv vertices).
        for (auto itr = first; itr != last; itr++) {
            Index i = *itr;
            if (index_map[i] != i) continue;
            for (auto itr2 = std::next(itr); itr2 != last; itr2++) {
                Index j = *itr2;
                if (equal(i, j)) {
                    index_map[j] = i;
                }
            }
        }
    });

    std::vector<Index> old2new(num_values);
    Index count = 0;
    for (Index i = 0; i < num_values; i++) {
        if (index_map[i] == i) {
            old2new[i] = count++;
        } else {
            old2new[i] = old2new[index_map[i]];
        }
    }
    if (count == num_values) {
        // Nothing to weld.
        return;
    }

    auto mapping = internal::invert_mapping({old2new.data(), old2new.size()}, count);

    Attribute<ValueType> attr_welded_values(
        attr_values.get_element_type(),
        attr_values.get_usage(),
        attr_values.get_num_channels());
    attr_welded_values.resize_elements(count);
    auto welded_values = matrix_ref(attr_welded_values);
    welded_values.setZero();
    tbb::parallel_for((Index)0, count, [&](Index i) {
        for (Index j = mapping.offsets[i]; j < mapping.offsets[i + 1]; j++) {
            welded_values.row(i) += values.row(mapping.data[j]);
        }
        if (mapping.offsets[i + 1] - mapping.offsets[i] > 0) {
            welded_values.row(i) /=
                static_cast<ValueType>(mapping.offsets[i + 1] - mapping.offsets[i]);
        }
    });
    attr_values = std::move(attr_welded_values);
    tbb::parallel_for((Eigen::Index)0, indices.size(), [&](auto i) {
        indices.data()[i] = old2new[indices.data()[i]];
    });
}

template <typename DerivedA, typename DerivedB>
bool allclose(
    const Eigen::DenseBase<DerivedA>& a,
    const Eigen::DenseBase<DerivedB>& b,
    const typename DerivedA::RealScalar& rtol =
        Eigen::NumTraits<typename DerivedA::RealScalar>::dummy_precision(),
    const typename DerivedA::RealScalar& atol =
        Eigen::NumTraits<typename DerivedA::RealScalar>::epsilon())
{
    return ((a.derived() - b.derived()).array().abs() <= (atol + rtol * b.derived().array().abs()))
        .all();
}

} // namespace

template <typename Scalar, typename Index>
void weld_indexed_attribute(
    SurfaceMesh<Scalar, Index>& mesh,
    AttributeId attr_id,
    const WeldOptions& options)
{
    lagrange::internal::visit_attribute_write(mesh, attr_id, [&](auto&& attr) {
        using AttributeType = std::decay_t<decltype(attr)>;
        if constexpr (AttributeType::IsIndexed) {
            using ValueType = typename AttributeType::ValueType;
            using RealType = typename Eigen::NumTraits<ValueType>::NonInteger;
            auto values = matrix_view(attr.values());
            if (options.epsilon_rel.has_value() || options.epsilon_abs.has_value()) {
                const RealType eps_rel = options.epsilon_rel.has_value()
                                             ? safe_cast<RealType>(options.epsilon_rel.value())
                                             : Eigen::NumTraits<RealType>::dummy_precision();
                const RealType eps_abs = options.epsilon_abs.has_value()
                                             ? safe_cast<RealType>(options.epsilon_abs.value())
                                             : Eigen::NumTraits<RealType>::epsilon();
                weld_indexed_attribute(mesh, attr, [&, eps_rel, eps_abs](Index i, Index j) -> bool {
                    return allclose(
                        values.row(i).template cast<RealType>(),
                        values.row(j).template cast<RealType>(),
                        eps_rel,
                        eps_abs);
                });
            } else {
                weld_indexed_attribute(mesh, attr, [&](Index i, Index j) -> bool {
                    return (values.row(i).array() == values.row(j).array()).all();
                });
            }
        }
    });
}

#define LA_X_weld_indexed_attribute(ValueType, Scalar, Index)        \
    template LA_CORE_API void weld_indexed_attribute<Scalar, Index>( \
        SurfaceMesh<Scalar, Index>&,                                 \
        AttributeId,                                                 \
        const WeldOptions& options);

LA_SURFACE_MESH_X(weld_indexed_attribute, 0)

} // namespace lagrange
