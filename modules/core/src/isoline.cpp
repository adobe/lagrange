/*
 * Copyright 2024 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */

#include <lagrange/isoline.h>

#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/foreach_attribute.h>
#include <lagrange/internal/attribute_string_utils.h>
#include <lagrange/internal/visit_attribute.h>
#include <lagrange/mesh_cleanup/remove_isolated_vertices.h>
#include <lagrange/utils/DisjointSets.h>
#include <lagrange/utils/Error.h>
#include <lagrange/utils/StackVector.h>
#include <lagrange/views.h>

#include <tbb/enumerable_thread_specific.h>
#include <tbb/parallel_for.h>

namespace lagrange {

namespace {

enum class Sign {
    Inside,
    Zero,
    Outside,
};

template <typename Index>
struct LocalDataT
{
    std::vector<Index> corners_with_crossing;
    std::vector<StackVector<Index, 4>> new_facets;
};

template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> isoline_internal(
    SurfaceMesh<Scalar, Index> mesh,
    const IsolineOptions& options,
    bool isolines_only,
    const Attribute<double>& values_,
    const Attribute<Index>* indices_ = nullptr)
{
    span<const double> values = values_.get_all();
    span<const Index> indices = indices_ ? indices_->get_all() : span<const Index>();

    SurfaceMesh<Scalar, Index> result = mesh;
    mesh.initialize_edges();
    result.clear_facets();

    const Index dimension = mesh.get_dimension();
    const auto positions = mesh.get_vertex_to_position().get_all();
    const auto c2v = mesh.get_corner_to_vertex().get_all();

    auto is_corner_smooth = [&](Index ci, Index cj) {
        return indices.empty() || indices[ci] == indices[cj];
    };

    auto is_edge_smooth = [&](Index ci, Index cj) {
        Index ci2 = mesh.get_next_corner_around_facet(ci);
        Index cj2 = mesh.get_next_corner_around_facet(cj);
        if (c2v[ci] == c2v[cj]) {
            // Half-edges oriented in the same direction, this is a non-manifold edge
            la_debug_assert(c2v[ci2] == c2v[cj2]);
            return is_corner_smooth(ci, cj) && is_corner_smooth(ci2, cj2);
        } else {
            // Half-edges oriented in opposite directions, this is a manifold edge
            la_debug_assert(c2v[ci2] == c2v[cj]);
            la_debug_assert(c2v[ci] == c2v[cj2]);
            return is_corner_smooth(ci, cj2) && is_corner_smooth(ci2, cj);
        }
    };

    const size_t num_channels = values_.get_num_channels();
    auto eval_corner = [&](Index corner) {
        if (indices.empty()) {
            return values[c2v[corner] * num_channels + options.channel_index];
        } else {
            return values[indices[corner] * num_channels + options.channel_index];
        }
    };

    auto has_crossing = [&](Index ci) {
        Index cj = mesh.get_next_corner_around_facet(ci);
        double xi = eval_corner(ci) - options.isovalue;
        double xj = eval_corner(cj) - options.isovalue;
        if (std::fpclassify(xi) == FP_ZERO || std::fpclassify(xj) == FP_ZERO) {
            return false; // crossing is on vertex
        }
        return std::signbit(xi) != std::signbit(xj);
    };

    auto corner_sign = [&](Index ci) {
        double xi = eval_corner(ci) - options.isovalue;
        if (std::fpclassify(xi) == FP_ZERO) {
            return Sign::Zero;
        }
        if (options.keep_below ? std::signbit(xi) : !std::signbit(xi)) {
            return Sign::Inside;
        } else {
            return Sign::Outside;
        }
    };

    auto interpolate_position = [&](Index ci, span<Scalar> pos) {
        Index cj = mesh.get_next_corner_around_facet(ci);
        double xi = eval_corner(ci);
        double xj = eval_corner(cj);
        Scalar t = static_cast<Scalar>((options.isovalue - xi) / (xj - xi));
        for (Index d = 0; d < dimension; ++d) {
            pos[d] = positions[c2v[ci] * dimension + d] * (1 - t) +
                     positions[c2v[cj] * dimension + d] * t;
        }
    };

    // New vertex ids for edge isocrossing are associated to the "provoking" corner of the edge. We
    // may have shared ids due to corners from different facets sharing the same indices for the
    // isovalue attribute. So we use union-find to "join" provoking corners that share the same
    // isovalue indices.
    std::vector<Index> repr_to_new_vertex(mesh.get_num_corners(), invalid<Index>());
    DisjointSets<Index> repr_corner(mesh.get_num_corners());

    using LocalData = LocalDataT<Index>;
    tbb::enumerable_thread_specific<LocalData> data;

    // First pass to assign new vertex ids to corners with isocrossing
    const Index num_vertices = mesh.get_num_vertices();
    tbb::concurrent_vector<Index> new_vertex_to_provoking_corner;
    tbb::parallel_for(Index(0), mesh.get_num_edges(), [&](Index e) {
        auto& corners_with_crossing = data.local().corners_with_crossing;
        corners_with_crossing.clear();
        mesh.foreach_corner_around_edge(e, [&](Index c) {
            if (has_crossing(c)) {
                corners_with_crossing.push_back(c);
            }
        });
        for (size_t i = 0; i < corners_with_crossing.size(); i++) {
            Index ci = corners_with_crossing[i];
            for (size_t j = i + 1; j < corners_with_crossing.size(); j++) {
                Index cj = corners_with_crossing[j];
                if (is_edge_smooth(ci, cj)) {
                    repr_corner.merge(ci, cj);
                }
            }
        }
        for (auto c : corners_with_crossing) {
            if (c == repr_corner.find(c)) {
                auto it = new_vertex_to_provoking_corner.push_back(c);
                repr_to_new_vertex[c] =
                    num_vertices + static_cast<Index>(it - new_vertex_to_provoking_corner.begin());
            }
        }
    });

    // Second pass to compute new vertex positions
    result.add_vertices(
        static_cast<Index>(new_vertex_to_provoking_corner.size()),
        [&](Index v, span<Scalar> pos) {
            interpolate_position(new_vertex_to_provoking_corner[v], pos);
        });

    // Interpolate vertex attributes for new vertices
    // TODO:
    // - Per-facet attributes (copy to new facets)
    // - Indexed attributes (interpolate)
    // - Corner attributes (interpolate)
    par_foreach_named_attribute_read<Vertex>(mesh, [&](auto name, auto&& old_attr) {
        if (mesh.attr_name_is_reserved(name)) {
            return;
        }
        using AttributeType = std::decay_t<decltype(old_attr)>;
        using ValueType = typename AttributeType::ValueType;
        auto old_values = matrix_view(old_attr);
        auto& new_attr = result.template ref_attribute<ValueType>(name);
        auto new_values = matrix_ref(new_attr);
        for (Index v = num_vertices, i = 0; v < result.get_num_vertices(); ++v, ++i) {
            Index ci = new_vertex_to_provoking_corner[i];
            Index cj = mesh.get_next_corner_around_facet(ci);
            double xi = eval_corner(ci);
            double xj = eval_corner(cj);
            double t = (options.isovalue - xi) / (xj - xi);
            new_values.row(v) = (old_values.row(c2v[ci]).template cast<double>() * (1.0 - t) +
                                 old_values.row(c2v[cj]).template cast<double>() * t)
                                    .template cast<ValueType>();
        }
    });

    // Third pass to compute the connectivity
    tbb::parallel_for(Index(0), mesh.get_num_facets(), [&](Index f) {
        const Index c0 = mesh.get_facet_corner_begin(f);
        const Index c1 = c0 + 1;
        const Index c2 = c0 + 2;
        auto s0 = corner_sign(c0);
        auto s1 = corner_sign(c1);
        auto s2 = corner_sign(c2);
        auto v01 = repr_to_new_vertex[repr_corner.find(c0)];
        auto v12 = repr_to_new_vertex[repr_corner.find(c1)];
        auto v20 = repr_to_new_vertex[repr_corner.find(c2)];
        auto& new_facets = data.local().new_facets;
        StackVector<Index, 4> facet;
        if (isolines_only ? s0 == Sign::Zero : s0 != Sign::Outside) {
            facet.push_back(c2v[c0]);
        }
        if (v01 != invalid<Index>()) {
            facet.push_back(v01);
        }
        if (isolines_only ? s1 == Sign::Zero : s1 != Sign::Outside) {
            facet.push_back(c2v[c1]);
        }
        if (v12 != invalid<Index>()) {
            facet.push_back(v12);
        }
        if (isolines_only ? s2 == Sign::Zero : s2 != Sign::Outside) {
            facet.push_back(c2v[c2]);
        }
        if (v20 != invalid<Index>()) {
            facet.push_back(v20);
        }
        if (facet.empty()) {
            return;
        }
        if (isolines_only) {
            la_debug_assert(facet.size() < 4);
            if (facet.size() == 1) {
                // Only one isocrossing, at a vertex, no edge to record
                return;
            } else if (facet.size() == 2) {
                // Two isocrossings, either at a vertex or an edge
                new_facets.push_back(facet);
            } else {
                // Three isocrossings, either at a vertex or an edge
                new_facets.push_back({facet[0], facet[1]});
                new_facets.push_back({facet[1], facet[2]});
                new_facets.push_back({facet[2], facet[0]});
            }
            // Cannot have 4 isocrossing in a triangle
        } else {
            // Trimming produces either a triangle or a quad
            la_debug_assert(facet.size() == 3 || facet.size() == 4);
            data.local().new_facets.push_back(facet);
        }
    });

    // Gather new facets and add them to the result
    for (const auto& local_data : data) {
        const auto& new_facets = local_data.new_facets;
        result.add_hybrid(
            static_cast<Index>(new_facets.size()),
            [&](Index f) { return static_cast<Index>(new_facets[f].size()); },
            [&](Index f, span<Index> t) {
                std::copy_n(new_facets[f].begin(), t.size(), t.begin());
            });
    }

    // Finally, remove isolated vertices caused by removed facets.
    remove_isolated_vertices(result);

    return result;
}

template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> isoline_internal(
    const SurfaceMesh<Scalar, Index>& mesh,
    const IsolineOptions& options,
    bool isolines_only)
{
    la_runtime_assert(
        mesh.is_triangle_mesh(),
        "Isoline extraction/trimming only works for triangle meshes");

    SurfaceMesh<Scalar, Index> result;

    // We save on compiled binary size by explicitly casting the attribute to double before
    // performing the actual trimming operation...
    internal::visit_attribute_read(mesh, options.attribute_id, [&](auto&& attr) {
        using AttributeType = std::decay_t<decltype(attr)>;
        using ValueType = typename AttributeType::ValueType;
        if (!(attr.get_element_type() == AttributeElement::Vertex ||
              attr.get_element_type() == AttributeElement::Indexed)) {
            throw Error(
                fmt::format(
                    "Isoline attribute element type should be Vertex or Indexed, not {}",
                    internal::to_string(attr.get_element_type())));
        }
        if constexpr (AttributeType::IsIndexed) {
            if constexpr (std::is_same_v<ValueType, double>) {
                result =
                    isoline_internal(mesh, options, isolines_only, attr.values(), &attr.indices());
            } else {
                result = isoline_internal(
                    mesh,
                    options,
                    isolines_only,
                    Attribute<double>::cast_copy(attr.values()),
                    &attr.indices());
            }
        } else {
            if constexpr (std::is_same_v<ValueType, double>) {
                result = isoline_internal(mesh, options, isolines_only, attr);
            } else {
                result = isoline_internal(
                    mesh,
                    options,
                    isolines_only,
                    Attribute<double>::cast_copy(attr));
            }
        }
    });

    return result;
}

} // namespace

template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> trim_by_isoline(
    const SurfaceMesh<Scalar, Index>& mesh,
    const IsolineOptions& options)
{
    return isoline_internal(mesh, options, false);
}

template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> extract_isoline(
    const SurfaceMesh<Scalar, Index>& mesh,
    const IsolineOptions& options)
{
    return isoline_internal(mesh, options, true);
}

#define LA_X_isoline(_, Scalar, Index)                               \
    template LA_CORE_API SurfaceMesh<Scalar, Index> trim_by_isoline( \
        const SurfaceMesh<Scalar, Index>& mesh,                      \
        const IsolineOptions& options);                              \
    template LA_CORE_API SurfaceMesh<Scalar, Index> extract_isoline( \
        const SurfaceMesh<Scalar, Index>& mesh,                      \
        const IsolineOptions& options);
LA_SURFACE_MESH_X(isoline, 0)

} // namespace lagrange
