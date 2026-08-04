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
#include <lagrange/utils/function_ref.h>
#include <lagrange/utils/safe_cast.h>
#include <lagrange/weld_indexed_attribute.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <tbb/parallel_for.h>
#include <tbb/parallel_sort.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <limits>
#include <type_traits>

namespace lagrange {

namespace {

// Maps a value type to its corresponding real (non-integer) type for tolerance computations.
template <typename T>
struct NonIntegerT
{
    using type = double;
};
template <>
struct NonIntegerT<float>
{
    using type = float;
};
template <>
struct NonIntegerT<double>
{
    using type = double;
};

// Returns the default relative tolerance (equivalent to Eigen::NumTraits<T>::dummy_precision()).
template <typename T>
constexpr T default_rel_tolerance()
{
    if constexpr (std::is_same_v<T, float>) {
        return T(1e-5);
    } else {
        return T(1e-12);
    }
}

template <typename Index>
struct IndexAndCornerT
{
    Index index;
    Index corner;
};

// Check whether two rows of values are approximately equal.
// Returns true if all channels satisfy |a[c] - b[c]| <= atol + rtol * |b[c]|,
// and optionally if the angle between the two vectors is within the threshold.
template <typename ValueType>
bool rows_are_close(
    const ValueType* row_a,
    const ValueType* row_b,
    size_t num_channels,
    double eps_rel,
    double eps_abs,
    double cos_angle_abs)
{
    using RealType = typename NonIntegerT<ValueType>::type;

    // Element-wise tolerance check.
    for (size_t c = 0; c < num_channels; ++c) {
        RealType diff = std::abs(static_cast<RealType>(row_a[c]) - static_cast<RealType>(row_b[c]));
        RealType tol = static_cast<RealType>(eps_abs) +
                       static_cast<RealType>(eps_rel) * std::abs(static_cast<RealType>(row_b[c]));
        // Use !(diff <= tol) so that NaN causes the comparison to fail (returns false).
        if (!(diff <= tol)) return false;
    }

    // Angle check (only if cos_angle_abs <= 1).
    if (cos_angle_abs <= 1.0) {
        RealType dot = 0;
        RealType norm_a_sq = 0;
        RealType norm_b_sq = 0;
        for (size_t c = 0; c < num_channels; ++c) {
            RealType a = static_cast<RealType>(row_a[c]);
            RealType b = static_cast<RealType>(row_b[c]);
            dot += a * b;
            norm_a_sq += a * a;
            norm_b_sq += b * b;
        }
        RealType threshold =
            static_cast<RealType>(cos_angle_abs) * std::sqrt(norm_a_sq) * std::sqrt(norm_b_sq);
        // Use !(dot >= threshold) so that NaN causes the comparison to fail (returns false).
        if (!(dot >= threshold)) {
            return false;
        }
    }

    return true;
}

// Check whether two rows of values are exactly equal (per-element ==).
template <typename ValueType>
bool rows_are_equal(const ValueType* row_a, const ValueType* row_b, size_t num_channels)
{
    return std::equal(row_a, row_a + num_channels, row_b);
}

// Check whether any channel in a row equals the invalid sentinel value.
template <typename ValueType>
bool row_has_invalid(const ValueType* row, size_t num_channels)
{
    const ValueType inv = lagrange::invalid<ValueType>();
    for (size_t c = 0; c < num_channels; ++c) {
        if (row[c] == inv) return true;
    }
    return false;
}

// Assign a reduced index to the root corner of a group. Updates num_reduced in place.
template <typename Index>
void process_root(
    Index c,
    Index& num_reduced,
    const std::vector<bool>& group_flagged,
    span<const Index> corner_to_value,
    std::vector<Index>& corner_to_reduced,
    std::vector<Index>& index_to_reduced)
{
    if (corner_to_reduced[c] != invalid<Index>()) {
        // If the root corner has already been processed, we can skip it.
        return;
    }
    if (group_flagged[c]) {
        // If the group is flagged, it means a merge happened, and we assign a new index to
        // the corner group.
        corner_to_reduced[c] = num_reduced++;
    } else {
        // If the group is not flagged, we can preserve the original index. In other words,
        // we assign a reduced index based on the original index associated to the corner,
        // not based on the corner group.
        Index i = corner_to_value[c];
        if (index_to_reduced[i] == invalid<Index>()) {
            index_to_reduced[i] = num_reduced++;
        }
        corner_to_reduced[c] = index_to_reduced[i];
    }
    la_debug_assert(corner_to_reduced[c] != invalid<Index>());
}

// Merge corners around each vertex that share the same index or have similar values.
//
// Templated on `Index` only: the per-vertex corner adjacency is supplied as an index-only CSR
// (`vertex_to_corners`) and the value comparison is type-erased through `values_close`. This keeps
// the heavy parallel machinery (TBB, SmallVector, DisjointSets) compiled once per index type
// rather than once per (ValueType, Scalar, Index) combination.
template <typename Index>
void merge_corners_per_vertex(
    Index num_vertices,
    const internal::InverseMapping<Index>& vertex_to_corners,
    const std::vector<bool>& exclude_vertices_mask,
    span<const Index> corner_to_value,
    function_ref<bool(Index, Index)> values_close,
    DisjointSets<Index>& corner_map,
    std::vector<uint8_t>& corner_flagged)
{
    // Sort and find duplicate values shared by corners around the same vertex.
    using IndexAndCorner = IndexAndCornerT<Index>;
    tbb::parallel_for(
        tbb::blocked_range<Index>(0, num_vertices),
        [&](const tbb::blocked_range<Index>& range) {
            for (Index vi = range.begin(); vi < range.end(); vi++) {
                if (exclude_vertices_mask[vi]) continue;

                SmallVector<IndexAndCorner, 16> involved_indices_and_corners;
                vertex_to_corners.foreach_mapped_to(vi, [&](Index ci) {
                    involved_indices_and_corners.push_back({corner_to_value[ci], ci});
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
                        corner_map.merge(it_begin->corner, it->corner);
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
                    c = corner_map.find(c);
                }

                for (auto itr = first; itr != last; itr++) {
                    const auto& [i1, c1] = *itr;

                    // If the corner is not the root of the group, it means it has been merged with
                    // another corner in this inner loop, and we don't need to compare against all
                    // other uniqued indices again.
                    if (corner_map.find(c1) != c1) continue;

                    // Quadratic loop to search for corners with similar values.
                    for (auto itr2 = std::next(itr); itr2 != last; itr2++) {
                        const auto& [i2, c2] = *itr2;
                        if (values_close(i1, i2)) {
                            // Flag any corner group containing merged values.
                            Index root = corner_map.merge(c1, c2);
                            corner_flagged[root] = 1;
                        }
                    }
                }
            }
        });
}

// Assign reduced indices to all corners based on the disjoint sets and flags.
template <typename Index>
Index assign_reduced_indices(
    Index num_corners,
    Index num_values,
    DisjointSets<Index>& corner_map,
    const std::vector<uint8_t>& corner_flagged,
    span<const Index> corner_to_value,
    std::vector<Index>& corner_to_reduced)
{
    // Propagate flags to roots after all merges are done
    std::vector<bool> group_flagged(num_corners, false);
    for (Index c = 0; c < num_corners; ++c) {
        if (corner_flagged[c]) {
            Index rc = corner_map.find(c);
            group_flagged[rc] = true;
        }
    }

    Index num_reduced = 0;
    std::vector<Index> index_to_reduced(num_values, invalid<Index>());

    // Assign reduced indices to corners.
    for (Index c = 0; c < num_corners; ++c) {
        Index rc = corner_map.find(c);
        process_root(
            rc,
            num_reduced,
            group_flagged,
            corner_to_value,
            corner_to_reduced,
            index_to_reduced);
        if (rc != c) {
            corner_to_reduced[c] = corner_to_reduced[rc];
            la_debug_assert(corner_to_reduced[c] != invalid<Index>());
        }
    }

    return num_reduced;
}

// Run the index-only welding pipeline: merge corners around each vertex, optionally merge corner
// groups that share the same value index across vertices, then assign reduced indices.
//
// Templated on `Index` only (not ValueType/Scalar): the value comparison is type-erased through
// `values_close`, so the heavy disjoint-set machinery is compiled once per index type. Fills
// `corner_to_reduced` and returns the number of reduced (welded) values.
template <typename Index>
Index weld_core(
    Index num_corners,
    Index num_vertices,
    Index num_values,
    const internal::InverseMapping<Index>& vertex_to_corners,
    const std::vector<bool>& exclude_vertices_mask,
    span<const Index> corner_to_value,
    function_ref<bool(Index, Index)> values_close,
    bool merge_across_vertices,
    std::vector<Index>& corner_to_reduced)
{
    DisjointSets<Index> corner_map(num_corners);
    std::vector<uint8_t> corner_flagged(num_corners, 0);

    merge_corners_per_vertex<Index>(
        num_vertices,
        vertex_to_corners,
        exclude_vertices_mask,
        corner_to_value,
        values_close,
        corner_map,
        corner_flagged);

    if (merge_across_vertices) {
        // Merge corner groups that share indices
        auto index_to_corner = internal::invert_mapping(
            span<const Index>(corner_to_value.data(), static_cast<size_t>(num_corners)),
            num_values);
        for (Index i = 0; i < num_values; i++) {
            auto it_begin = index_to_corner.data.begin() + index_to_corner.offsets[i];
            auto it_end = index_to_corner.data.begin() + index_to_corner.offsets[i + 1];
            if (it_begin == it_end) continue;
            for (auto it = it_begin + 1; it != it_end; ++it) {
                corner_map.merge(*it_begin, *it);
            }
        }
    }

    corner_to_reduced.assign(num_corners, invalid<Index>());
    return assign_reduced_indices(
        num_corners,
        num_values,
        corner_map,
        corner_flagged,
        corner_to_value,
        corner_to_reduced);
}

// Within each reduced group, sort the member corners by value index and move the unique ones to the
// front of the group's slice (via std::unique). The group offsets are NOT updated and no elements
// are erased; instead the per-group count of unique members is returned, and callers must read only
// the first `unique_count[ri]` entries of each group. Templated on `Index` only, so the heavy
// parallel sort is compiled once per index type rather than once per value type.
template <typename Index>
std::vector<Index> dedup_groups_by_value(
    internal::InverseMapping<Index>& reduced_to_corner,
    Index num_reduced,
    span<const Index> corner_to_value)
{
    std::vector<Index> unique_count(static_cast<size_t>(num_reduced));
    tbb::parallel_for(Index(0), num_reduced, [&](Index ri) {
        auto it_begin = reduced_to_corner.data.begin() + reduced_to_corner.offsets[ri];
        auto it_end = reduced_to_corner.data.begin() + reduced_to_corner.offsets[ri + 1];
        // Sort and unique to avoid summing over the same values more time than necessary.
        // We could probably avoid this to gain a little bit of performance if needed.
        tbb::parallel_sort(it_begin, it_end, [&](Index ci, Index cj) {
            return corner_to_value[ci] < corner_to_value[cj];
        });
        auto new_end = std::unique(it_begin, it_end, [&](Index ci, Index cj) {
            return corner_to_value[ci] == corner_to_value[cj];
        });
        unique_count[ri] = static_cast<Index>(std::distance(it_begin, new_end));
    });
    return unique_count;
}

// Compute the welded attribute values by averaging merged groups. Only the final accumulation is
// ValueType-dependent; the index-only grouping/dedup is handled by `dedup_groups_by_value`.
template <typename ValueType, typename Index>
void compute_welded_values(
    Index num_reduced,
    span<const ValueType> values_data,
    span<const Index> corner_to_value,
    const std::vector<Index>& corner_to_reduced,
    size_t num_channels,
    Attribute<ValueType>& attr_values)
{
    auto reduced_to_corner =
        internal::invert_mapping({corner_to_reduced.data(), corner_to_reduced.size()}, num_reduced);
    std::vector<Index> unique_count =
        dedup_groups_by_value<Index>(reduced_to_corner, num_reduced, corner_to_value);

    Attribute<ValueType> attr_welded_values(
        attr_values.get_element_type(),
        attr_values.get_usage(),
        attr_values.get_num_channels());
    attr_welded_values.resize_elements(num_reduced);
    auto welded_data = attr_welded_values.ref_all();
    std::fill(welded_data.begin(), welded_data.end(), ValueType(0));

    tbb::parallel_for(Index(0), num_reduced, [&](Index ri) {
        const Index* group = reduced_to_corner.data.data() + reduced_to_corner.offsets[ri];
        const Index num = unique_count[ri];
        ValueType* dst = welded_data.data() + static_cast<size_t>(ri) * num_channels;
        for (Index k = 0; k < num; ++k) {
            const ValueType* src =
                values_data.data() + static_cast<size_t>(corner_to_value[group[k]]) * num_channels;
            for (size_t ch = 0; ch < num_channels; ++ch) {
                dst[ch] += src[ch];
            }
        }
        if (num > 1) {
            for (size_t ch = 0; ch < num_channels; ++ch) {
                dst[ch] /= static_cast<ValueType>(num);
            }
        }
    });
    attr_values = std::move(attr_welded_values);
}

// Thin per-(ValueType, Index) shell: builds the type-erased value comparator and forwards the
// index-only topology to `weld_core`, then computes the averaged welded values. All the heavy
// algorithmic code lives in `weld_core` (Index-only) and `compute_welded_values`.
template <typename ValueType, typename Index>
void weld_indexed_attribute_impl(
    IndexedAttribute<ValueType, Index>& attr,
    Index num_corners,
    Index num_vertices,
    const internal::InverseMapping<Index>& vertex_to_corners,
    const std::vector<bool>& exclude_vertices_mask,
    bool merge_across_vertices,
    double eps_rel,
    double eps_abs,
    double cos_angle_abs)
{
    auto& attr_values = attr.values();
    auto& attr_indices = attr.indices();
    const size_t num_channels = attr_values.get_num_channels();
    span<const ValueType> values_data = attr_values.get_all();
    span<Index> corner_to_value = attr_indices.ref_all();

    const Index num_values = static_cast<Index>(attr_values.get_num_elements());

    // The only ValueType-dependent part of the merge pipeline: comparison of two value rows.
    auto values_close = [&](Index i, Index j) -> bool {
        const ValueType* row_i = values_data.data() + static_cast<size_t>(i) * num_channels;
        const ValueType* row_j = values_data.data() + static_cast<size_t>(j) * num_channels;
        if (rows_are_equal(row_i, row_j, num_channels)) {
            return true;
        }
        if (row_has_invalid(row_i, num_channels) || row_has_invalid(row_j, num_channels)) {
            return false;
        }
        // Debug-only finiteness check (equivalent to Eigen's allFinite()).
        if constexpr (std::is_floating_point_v<ValueType>) {
            la_debug_assert(std::all_of(row_i, row_i + num_channels, [](ValueType v) {
                return std::isfinite(v);
            }));
            la_debug_assert(std::all_of(row_j, row_j + num_channels, [](ValueType v) {
                return std::isfinite(v);
            }));
        }
        return rows_are_close(row_i, row_j, num_channels, eps_rel, eps_abs, cos_angle_abs);
    };

    std::vector<Index> corner_to_reduced;
    Index num_reduced = weld_core<Index>(
        num_corners,
        num_vertices,
        num_values,
        vertex_to_corners,
        exclude_vertices_mask,
        span<const Index>(corner_to_value.data(), corner_to_value.size()),
        values_close,
        merge_across_vertices,
        corner_to_reduced);

    if (num_reduced == num_values) {
        // Nothing to weld.
        return;
    }

    compute_welded_values(
        num_reduced,
        values_data,
        span<const Index>(corner_to_value.data(), corner_to_value.size()),
        corner_to_reduced,
        num_channels,
        attr_values);

    tbb::parallel_for(Index(0), num_corners, [&](Index c) {
        corner_to_value[c] = corner_to_reduced[c];
    });
}

} // namespace

template <typename Scalar, typename Index>
void weld_indexed_attribute(
    SurfaceMesh<Scalar, Index>& mesh,
    AttributeId attr_id,
    const WeldOptions& options)
{
    // Extract index-only topology once, independent of the attribute's value type: the set of
    // corners incident to each vertex. Using the intrinsic corner-to-vertex map (rather than edge
    // connectivity) keeps the heavy welding pipeline free of the Scalar type and avoids the need to
    // initialize/clear mesh edges.
    const Index num_vertices = mesh.get_num_vertices();
    const Index num_corners = mesh.get_num_corners();
    span<const Index> corner_to_vertex = mesh.get_corner_to_vertex().get_all();
    const internal::InverseMapping<Index> vertex_to_corners =
        internal::invert_mapping(corner_to_vertex, num_vertices);

    std::vector<bool> exclude_vertices_mask(static_cast<size_t>(num_vertices), false);
    for (auto vi : options.exclude_vertices) {
        la_debug_assert(vi < static_cast<size_t>(num_vertices));
        exclude_vertices_mask[vi] = true;
    }

    lagrange::internal::visit_attribute_write(mesh, attr_id, [&](auto&& attr) {
        using AttributeType = std::decay_t<decltype(attr)>;
        if constexpr (AttributeType::IsIndexed) {
            using ValueType = typename AttributeType::ValueType;
            using RealType = typename NonIntegerT<ValueType>::type;

            // safe_cast<RealType> validates that a caller-provided tolerance is representable in
            // the
            // attribute's real type, throwing (rather than silently saturating to +/-inf inside
            // rows_are_close) if it overflows a lower-precision ValueType such as float. Done once
            // here rather than per element comparison.
            const double eps_rel =
                options.epsilon_rel.has_value()
                    ? static_cast<double>(safe_cast<RealType>(options.epsilon_rel.value()))
                    : static_cast<double>(default_rel_tolerance<RealType>());
            const double eps_abs =
                options.epsilon_abs.has_value()
                    ? static_cast<double>(safe_cast<RealType>(options.epsilon_abs.value()))
                    : static_cast<double>(std::numeric_limits<RealType>::epsilon());

            constexpr double INVALID_COS_ANGLE_ABS = 2.0; // Out of the valid range.
            const double cos_angle_abs = options.angle_abs.has_value()
                                             ? std::cos(options.angle_abs.value())
                                             : INVALID_COS_ANGLE_ABS;
            weld_indexed_attribute_impl<ValueType, Index>(
                attr,
                num_corners,
                num_vertices,
                vertex_to_corners,
                exclude_vertices_mask,
                options.merge_across_vertices,
                eps_rel,
                eps_abs,
                cos_angle_abs);
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
