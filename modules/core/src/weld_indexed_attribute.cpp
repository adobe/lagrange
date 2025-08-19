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
#include <tbb/parallel_sort.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <algorithm>
#include <limits>

namespace lagrange {

namespace {

// Unsigned integer type using the most significant bit as a flag.
template <typename Index>
struct IndexWithFlagT
{
    explicit IndexWithFlagT(Index i = 0) { set_index(i); }

    void set_index(Index i)
    {
        i &= ~Mask; // Clear the flag
        i |= (m_value & Mask); // Set the flag if it was set
        m_value = i;
    }

    Index index() const
    {
        return m_value & ~Mask; // Clear the flag
    }

    void set_flag(bool enabled)
    {
        if (enabled) {
            m_value |= Mask; // Set the flag
        } else {
            m_value &= ~Mask; // Clear the flag
        }
    }

    bool flag() const
    {
        return (m_value & Mask) != 0; // Check the flag
    }

    Index m_value = 0;

    static constexpr Index Mask = Index(1) << (8 * sizeof(Index) - 1);

    static_assert(std::is_unsigned_v<Index>, "Index must be unsigned");
    static_assert(Mask != 0 && (Mask << 1 == 0));
};

template <typename ValueType, typename Scalar, typename Index, typename Func>
void weld_indexed_attribute(
    SurfaceMesh<Scalar, Index>& mesh,
    IndexedAttribute<ValueType, Index>& attr,
    span<const size_t> exclude_vertices,
    bool merge_accross_vertices,
    Func equal)
{
    std::vector<bool> exclude_vertices_mask(mesh.get_num_vertices(), false);
    for (auto vi : exclude_vertices) {
        la_debug_assert(vi < mesh.get_num_vertices());
        exclude_vertices_mask[vi] = true;
    }

    const bool had_edges = mesh.has_edges();
    mesh.initialize_edges();
    auto& attr_values = attr.values();
    auto values = matrix_view(attr_values);
    auto corner_to_value = vector_ref(attr.indices());

    const Index num_vertices = mesh.get_num_vertices();
    const Index num_values = static_cast<Index>(values.rows());
    const Index num_corners = mesh.get_num_corners();

    using IndexWithFlag = IndexWithFlagT<Index>;
    std::vector<IndexWithFlag> corner_map(num_corners);
    for (Index c = 0; c < num_corners; ++c) {
        corner_map[c].set_index(c);
    }

    auto find_and_compress = [&](Index c) {
        while (corner_map[c].index() != c) {
            corner_map[c].set_index(corner_map[corner_map[c].index()].index());
            c = corner_map[c].index();
        }
        return c;
    };

    auto merge_groups = [&](Index c1, Index c2, bool flag = false) {
        auto r1 = find_and_compress(c1);
        auto r2 = find_and_compress(c2);
        corner_map[r2].set_index(r1);
        corner_map[r1].set_flag(corner_map[r1].flag() || corner_map[r2].flag() || flag);
    };

    // Sort and find duplicate values shared by corners around the same vertex.
    tbb::parallel_for(
        tbb::blocked_range<Index>(0, num_vertices),
        [&](const tbb::blocked_range<Index>& range) {
            for (Index vi = range.begin(); vi < range.end(); vi++) {
                if (exclude_vertices_mask[vi]) return;

                struct IndexAndCorner
                {
                    Index index;
                    Index corner;
                };

                SmallVector<IndexAndCorner, 16> involved_indices_and_corners;
                mesh.foreach_corner_around_vertex(vi, [&](Index ci) {
                    involved_indices_and_corners.push_back({corner_to_value(ci), ci});
                });
                la_debug_assert(involved_indices_and_corners.size() > 0);

                // 1st pass: merge corners with same indices
                auto first = involved_indices_and_corners.begin();
                auto last = involved_indices_and_corners.end();
                tbb::parallel_sort(first, last, [](const auto& a, const auto& b) {
                    return a.index < b.index;
                });
                for (auto it_begin = first; it_begin != last;) {
                    // First the first corner after it_begin that has a different index
                    auto it_end = std::find_if(it_begin, last, [&](const auto& x) {
                        return (x.index != it_begin->index);
                    });
                    for (auto it = it_begin; it != it_end; ++it) {
                        corner_map[it->corner].set_index(it_begin->corner);
                    }
                    it_begin = it_end;
                }

                // 2nd pass: merge corners with same values
                last = std::unique(first, last, [](const auto& a, const auto& b) {
                    return a.index == b.index;
                });
                if (std::distance(first, last) <= 1) continue;

                // Update corner associated to the uniqued index to be the root of the group
                for (auto itr = first; itr != last; itr++) {
                    Index& c = itr->corner;
                    c = corner_map[c].index();
                }

                for (auto itr = first; itr != last; itr++) {
                    const auto& [i1, c1] = *itr;

                    // If the corner is not the root of the group, it means it has been merged with
                    // another corner in this inner loop, and we don't need to compare against all
                    // other uniqued indices again.
                    if (corner_map[c1].index() != c1) continue;

                    // Quadratic loop to search for corners with similar values.
                    for (auto itr2 = std::next(itr); itr2 != last; itr2++) {
                        const auto& [i2, c2] = *itr2;
                        if (equal(i1, i2)) {
                            // Flag any corner group containing merged values.
                            merge_groups(c1, c2, true);
                        }
                    }
                }
            }
        });

    if (merge_accross_vertices) {
        // Merge corner groups that share indices
        auto index_to_corner = internal::invert_mapping(
            {corner_to_value.data(), static_cast<size_t>(num_corners)},
            num_values);
        for (Index i = 0; i < num_values; i++) {
            auto it_begin = index_to_corner.data.begin() + index_to_corner.offsets[i];
            auto it_end = index_to_corner.data.begin() + index_to_corner.offsets[i + 1];
            if (it_begin == it_end) continue;
            for (auto it = it_begin + 1; it != it_end; ++it) {
                merge_groups(*it_begin, *it);
            }
        }
    }

    Index num_reduced = 0;
    std::vector<Index> corner_to_reduced(num_corners, invalid<Index>());
    std::vector<Index> index_to_reduced(num_values, invalid<Index>());

    auto process_root = [&](Index c) {
        if (corner_to_reduced[c] != invalid<Index>()) {
            // If the root corner has already been processed, we can skip it.
            return;
        }
        if (corner_map[c].flag()) {
            // If the group is flagged, it means a merge happened, and we assign a new index to
            // the corner group.
            corner_to_reduced[c] = num_reduced++;
        } else {
            // If the group is not flagged, we can preserve the original index. In other words,
            // we assign a reduced index based on the original index associated to the corner,
            // not based on the corner group.
            Index i = corner_to_value[c];
            if (index_to_reduced[i] == invalid<Index>()) {
                index_to_reduced[corner_to_value[c]] = num_reduced++;
            }
            corner_to_reduced[c] = index_to_reduced[corner_to_value[c]];
        }
        la_debug_assert(corner_to_reduced[c] != invalid<Index>());
    };

    // Assign reduced indices to corners.
    for (Index c = 0; c < num_corners; ++c) {
        Index rc = find_and_compress(c);
        process_root(rc);
        if (rc != c) {
            corner_to_reduced[c] = corner_to_reduced[rc];
            la_debug_assert(corner_to_reduced[c] != invalid<Index>());
        }
    }

    if (num_reduced == num_values) {
        // Nothing to weld.
        return;
    }

    auto reduced_to_corner =
        internal::invert_mapping({corner_to_reduced.data(), corner_to_reduced.size()}, num_reduced);

    Attribute<ValueType> attr_welded_values(
        attr_values.get_element_type(),
        attr_values.get_usage(),
        attr_values.get_num_channels());
    attr_welded_values.resize_elements(num_reduced);
    auto welded_values = matrix_ref(attr_welded_values);
    welded_values.setZero();
    tbb::parallel_for(Index(0), num_reduced, [&](Index ri) {
        auto it_begin = reduced_to_corner.data.begin() + reduced_to_corner.offsets[ri];
        auto it_end = reduced_to_corner.data.begin() + reduced_to_corner.offsets[ri + 1];
        // Sort and unique to avoid summing over the same values more time than necessary.
        // We could probably avoid this to gain a little bit of performance if needed.
        tbb::parallel_sort(it_begin, it_end, [&](Index ci, Index cj) {
            return corner_to_value[ci] < corner_to_value[cj];
        });
        it_end = std::unique(it_begin, it_end, [&](Index ci, Index cj) {
            return corner_to_value[ci] == corner_to_value[cj];
        });
        for (auto it = it_begin; it != it_end; ++it) {
            welded_values.row(ri) += values.row(corner_to_value[*it]);
        }
        auto num = std::distance(it_begin, it_end);
        if (num > 1) {
            welded_values.row(ri) /= static_cast<ValueType>(num);
        }
    });
    attr_values = std::move(attr_welded_values);
    tbb::parallel_for(Index(0), num_corners, [&](auto c) {
        corner_to_value[c] = corner_to_reduced[c];
    });

    if (!had_edges) {
        // Initializing edges is an implementation detail and should not leak outside of the
        // function.
        mesh.clear_edges();
    }
}

template <typename DerivedA, typename DerivedB>
bool allclose(
    const Eigen::DenseBase<DerivedA>& a,
    const Eigen::DenseBase<DerivedB>& b,
    const typename DerivedA::RealScalar& rtol =
        Eigen::NumTraits<typename DerivedA::RealScalar>::dummy_precision(),
    const typename DerivedA::RealScalar& atol =
        Eigen::NumTraits<typename DerivedA::RealScalar>::epsilon(),
    const typename DerivedA::RealScalar& cos_angle_abs = 1)
{
    // TODO: Use two different checks for absolute and relative tolerances.
    return ((a.derived() - b.derived()).array().abs() <= (atol + rtol * b.derived().array().abs()))
               .all() &&
           (cos_angle_abs > 1 || (a.derived().dot(b.derived()) >=
                                  cos_angle_abs * a.derived().norm() * b.derived().norm()));
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

            const RealType eps_rel = options.epsilon_rel.has_value()
                                         ? safe_cast<RealType>(options.epsilon_rel.value())
                                         : Eigen::NumTraits<RealType>::dummy_precision();
            const RealType eps_abs = options.epsilon_abs.has_value()
                                         ? safe_cast<RealType>(options.epsilon_abs.value())
                                         : Eigen::NumTraits<RealType>::epsilon();

            constexpr RealType INVALID_COS_ANGLE_ABS = 2; // Out of the the valid range.
            const RealType cos_angle_abs = options.angle_abs.has_value()
                                               ? std::cos(options.angle_abs.value())
                                               : INVALID_COS_ANGLE_ABS;
            weld_indexed_attribute(
                mesh,
                attr,
                options.exclude_vertices,
                options.merge_accross_vertices,
                [&, eps_rel, eps_abs, cos_angle_abs](Index i, Index j) -> bool {
                    return allclose(
                        values.row(i).template cast<RealType>(),
                        values.row(j).template cast<RealType>(),
                        eps_rel,
                        eps_abs,
                        cos_angle_abs);
                });
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
