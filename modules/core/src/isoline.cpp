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
#include <lagrange/utils/fmt/format.h>
#include <lagrange/views.h>

#include <tbb/enumerable_thread_specific.h>
#include <tbb/parallel_for.h>

#include <string>
#include <vector>

namespace lagrange {

namespace {

enum class Sign {
    Inside,
    Zero,
    Outside,
};

enum class IsolineMode {
    Trim, ///< Keep only the region on the `keep_below` side of the isoline.
    Extract, ///< Output the isoline itself as a collection of size-2 edge facets.
    Insert, ///< Keep the whole mesh, inserting the isoline as a chain of interior edges.
};

// Provenance of a corner in the trimmed mesh, expressed in terms of corners of the parent facet.
// The corner value is obtained by linearly interpolating along the parent edge:
//     value = value(c0) * (1 - t) + value(c1) * t
// For a corner that survives trimming (i.e. an original mesh vertex), we set c0 == c1 and t == 0 so
// the original value is preserved exactly.
template <typename Index>
struct CornerSource
{
    Index c0;
    Index c1;
    double t;
};

// Linearly interpolate rows `a` and `b` of `src` by parameter `t`, writing the result into row
// `dst_row` of `dst`. Interpolation is done in double precision; integer destination attributes are
// truncated on the final cast, matching the rest of the isoline interpolation paths.
template <typename DstDerived, typename SrcDerived>
void interpolate_row(
    Eigen::MatrixBase<DstDerived>& dst,
    Eigen::Index dst_row,
    const Eigen::MatrixBase<SrcDerived>& src,
    Eigen::Index a,
    Eigen::Index b,
    double t)
{
    using ValueType = typename DstDerived::Scalar;
    dst.row(dst_row) =
        (src.row(a).template cast<double>() * (1.0 - t) + src.row(b).template cast<double>() * t)
            .template cast<ValueType>();
}

template <typename Index>
struct LocalDataT
{
    std::vector<Index> corners_with_crossing;
    std::vector<StackVector<Index, 4>> new_facets;
    std::vector<Index> new_facet_to_old_facet;
    std::vector<StackVector<CornerSource<Index>, 4>> new_facet_corner_sources;
};

// Read-only view of the scalar field sampled on `mesh`, plus the geometric queries the isoline
// passes need. Bundling these into one object (instead of a web of capturing lambdas) lets each
// pass
// be a standalone function. Holds references/spans into `mesh`, so it must not outlive it.
template <typename Scalar, typename Index>
struct IsolineField
{
    const SurfaceMesh<Scalar, Index>& mesh;
    const IsolineOptions& options;
    span<const double> values;
    span<const Index> indices;
    span<const Index> c2v;
    span<const Scalar> positions;
    Index dimension;
    size_t num_channels;

    IsolineField(
        const SurfaceMesh<Scalar, Index>& mesh_,
        const IsolineOptions& options_,
        const Attribute<double>& values_,
        const Attribute<Index>* indices_)
        : mesh(mesh_)
        , options(options_)
        , values(values_.get_all())
        , indices(indices_ ? indices_->get_all() : span<const Index>())
        , c2v(mesh_.get_corner_to_vertex().get_all())
        , positions(mesh_.get_vertex_to_position().get_all())
        , dimension(mesh_.get_dimension())
        , num_channels(values_.get_num_channels())
    {}

    // Scalar field value at `corner` (looked up via the vertex or the indexed attribute).
    double eval_corner(Index corner) const
    {
        const Index row = indices.empty() ? c2v[corner] : indices[corner];
        return values[row * num_channels + options.channel_index];
    }

    // Whether corners `ci` and `cj` carry the same isovalue index (the field is continuous there).
    bool is_corner_smooth(Index ci, Index cj) const
    {
        return indices.empty() || indices[ci] == indices[cj];
    }

    bool is_edge_smooth(Index ci, Index cj) const
    {
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
    }

    // Whether the edge leaving `ci` is crossed by the isoline in its interior (not at a vertex).
    bool has_crossing(Index ci) const
    {
        Index cj = mesh.get_next_corner_around_facet(ci);
        double xi = eval_corner(ci) - options.isovalue;
        double xj = eval_corner(cj) - options.isovalue;
        if (std::fpclassify(xi) == FP_ZERO || std::fpclassify(xj) == FP_ZERO) {
            return false; // crossing is on vertex
        }
        return std::signbit(xi) != std::signbit(xj);
    }

    // Classify `ci` relative to the isoline (kept side, on the line, or outside).
    Sign corner_sign(Index ci) const
    {
        double xi = eval_corner(ci) - options.isovalue;
        if (std::fpclassify(xi) == FP_ZERO) {
            return Sign::Zero;
        }
        if (options.keep_below ? std::signbit(xi) : !std::signbit(xi)) {
            return Sign::Inside;
        } else {
            return Sign::Outside;
        }
    }

    // Interpolation parameter of the isocrossing along the edge starting at corner `ci`.
    double crossing_param(Index ci) const
    {
        Index cj = mesh.get_next_corner_around_facet(ci);
        double xi = eval_corner(ci);
        double xj = eval_corner(cj);
        return (options.isovalue - xi) / (xj - xi);
    }

    // Position of the isocrossing along the edge starting at corner `ci`.
    void interpolate_position(Index ci, span<Scalar> pos) const
    {
        Index cj = mesh.get_next_corner_around_facet(ci);
        Scalar t = static_cast<Scalar>(crossing_param(ci));
        for (Index d = 0; d < dimension; ++d) {
            pos[d] = positions[c2v[ci] * dimension + d] * (1 - t) +
                     positions[c2v[cj] * dimension + d] * t;
        }
    }
};

// Pass 1: assign a new output-vertex id to each edge isocrossing. Provoking corners that share the
// same isovalue index are merged via union-find, so a crossing shared by adjacent facets becomes a
// single vertex. Returns the map from new vertex (offset by `num_vertices`) to its provoking
// corner, and fills `repr_to_new_vertex` keyed by representative corner.
template <typename Scalar, typename Index>
tbb::concurrent_vector<Index> assign_crossing_vertices(
    const IsolineField<Scalar, Index>& field,
    Index num_vertices,
    DisjointSets<Index>& repr_corner,
    std::vector<Index>& repr_to_new_vertex,
    tbb::enumerable_thread_specific<LocalDataT<Index>>& data)
{
    const auto& mesh = field.mesh;
    tbb::concurrent_vector<Index> new_vertex_to_provoking_corner;
    tbb::parallel_for(Index(0), mesh.get_num_edges(), [&](Index e) {
        auto& corners_with_crossing = data.local().corners_with_crossing;
        corners_with_crossing.clear();
        mesh.foreach_corner_around_edge(e, [&](Index c) {
            if (field.has_crossing(c)) {
                corners_with_crossing.push_back(c);
            }
        });
        for (size_t i = 0; i < corners_with_crossing.size(); i++) {
            Index ci = corners_with_crossing[i];
            for (size_t j = i + 1; j < corners_with_crossing.size(); j++) {
                Index cj = corners_with_crossing[j];
                if (field.is_edge_smooth(ci, cj)) {
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
    return new_vertex_to_provoking_corner;
}

// Pass 3: for each parent facet, walk its boundary and emit the sub-facet(s) it contributes to the
// output, recording per-corner provenance for later attribute interpolation. The emitted polygons
// depend on `mode`: Extract yields the isoline segments, Trim the kept side, Insert both sides. The
// results accumulate into the thread-local `data` buffers.
template <typename Scalar, typename Index>
void build_subfacets(
    const IsolineField<Scalar, Index>& field,
    IsolineMode mode,
    bool keep_attributes,
    DisjointSets<Index>& repr_corner,
    const std::vector<Index>& repr_to_new_vertex,
    tbb::enumerable_thread_specific<LocalDataT<Index>>& data)
{
    const auto& mesh = field.mesh;
    const auto& c2v = field.c2v;
    tbb::parallel_for(Index(0), mesh.get_num_facets(), [&](Index f) {
        const Index c0 = mesh.get_facet_corner_begin(f);
        const Index cc[3] = {c0, c0 + 1, c0 + 2};
        const Sign sign[3] = {
            field.corner_sign(cc[0]),
            field.corner_sign(cc[1]),
            field.corner_sign(cc[2])};
        // New vertex inserted on the edge leaving each corner (invalid if the edge isn't crossed).
        const Index vv[3] = {
            repr_to_new_vertex[repr_corner.find(cc[0])],
            repr_to_new_vertex[repr_corner.find(cc[1])],
            repr_to_new_vertex[repr_corner.find(cc[2])]};

        auto& new_facets = data.local().new_facets;
        auto& new_facet_to_old_facet = data.local().new_facet_to_old_facet;
        auto& new_facet_corner_sources = data.local().new_facet_corner_sources;

        auto emit = [&](const StackVector<Index, 4>& poly,
                        const StackVector<CornerSource<Index>, 4>& poly_src) {
            new_facets.push_back(poly);
            if (keep_attributes) {
                new_facet_corner_sources.push_back(poly_src);
                new_facet_to_old_facet.push_back(f);
            }
        };

        // Walk the facet boundary as (corner, crossing, corner, crossing, ...), keeping the corners
        // for which `keep_corner(sign)` holds plus every isocrossing vertex. Walking in corner
        // order preserves the parent facet's orientation. The provenance of each output corner is
        // recorded so corner/indexed attributes can be interpolated within the parent facet.
        auto build_polygon = [&](auto&& keep_corner,
                                 StackVector<Index, 4>& poly,
                                 StackVector<CornerSource<Index>, 4>& poly_src) {
            for (int k = 0; k < 3; ++k) {
                if (keep_corner(sign[k])) {
                    poly.push_back(c2v[cc[k]]);
                    if (keep_attributes) poly_src.push_back({cc[k], cc[k], 0.0});
                }
                if (vv[k] != invalid<Index>()) {
                    poly.push_back(vv[k]);
                    if (keep_attributes) {
                        poly_src.push_back(
                            {cc[k],
                             mesh.get_next_corner_around_facet(cc[k]),
                             field.crossing_param(cc[k])});
                    }
                }
            }
        };

        if (mode == IsolineMode::Extract) {
            // Collect the isoline points on this facet (zero-valued corners and edge crossings) and
            // emit the segment(s) joining them.
            StackVector<Index, 4> points;
            StackVector<CornerSource<Index>, 4> points_src;
            build_polygon([](Sign s) { return s == Sign::Zero; }, points, points_src);
            la_debug_assert(points.size() < 4);
            if (points.size() == 2) {
                emit(points, points_src);
            } else if (points.size() == 3) {
                // Degenerate facet lying entirely on the isoline: emit its three edges.
                emit({points[0], points[1]}, {points_src[0], points_src[1]});
                emit({points[1], points[2]}, {points_src[1], points_src[2]});
                emit({points[2], points[0]}, {points_src[2], points_src[0]});
            }
            // size 0 or 1: the isoline only touches this facet at a single point, nothing to
            // record.
            return;
        }

        // Trim/Insert: emit the polygon on the kept side of the isoline (a triangle or a quad).
        StackVector<Index, 4> inside;
        StackVector<CornerSource<Index>, 4> inside_src;
        build_polygon([](Sign s) { return s != Sign::Outside; }, inside, inside_src);
        if (inside.size() >= 3) {
            la_debug_assert(inside.size() == 3 || inside.size() == 4);
            emit(inside, inside_src);
        }

        // For insertion we also emit the polygon on the other side, sharing the isocrossing
        // vertices, so the whole mesh is kept with the isoline as a chain of interior edges. We
        // skip it when no corner is strictly outside, otherwise a facet lying on the isoline would
        // be emitted twice.
        if (mode == IsolineMode::Insert &&
            (sign[0] == Sign::Outside || sign[1] == Sign::Outside || sign[2] == Sign::Outside)) {
            StackVector<Index, 4> outside;
            StackVector<CornerSource<Index>, 4> outside_src;
            build_polygon([](Sign s) { return s != Sign::Inside; }, outside, outside_src);
            if (outside.size() >= 3) {
                la_debug_assert(outside.size() == 3 || outside.size() == 4);
                emit(outside, outside_src);
            }
        }
    });
}

// Append all thread-local sub-facets to `result`. When propagating attributes, also concatenate the
// per-facet parent map and per-corner provenance in the same order the facets are added, so their
// indexing matches the result mesh.
template <typename Scalar, typename Index>
void gather_subfacets(
    tbb::enumerable_thread_specific<LocalDataT<Index>>& data,
    bool keep_attributes,
    SurfaceMesh<Scalar, Index>& result,
    std::vector<Index>& new_facet_to_old_facet,
    std::vector<CornerSource<Index>>& corner_sources)
{
    if (keep_attributes) {
        size_t total_facets = 0;
        for (const auto& local_data : data) {
            total_facets += local_data.new_facets.size();
        }
        new_facet_to_old_facet.reserve(total_facets);
        corner_sources.reserve(total_facets * 4); // at most 4 corners per (quad) sub-facet
    }
    for (const auto& local_data : data) {
        const auto& new_facets = local_data.new_facets;
        result.add_hybrid(
            static_cast<Index>(new_facets.size()),
            [&](Index f) { return static_cast<Index>(new_facets[f].size()); },
            [&](Index f, span<Index> t) {
                std::copy_n(new_facets[f].begin(), t.size(), t.begin());
            });
        if (keep_attributes) {
            new_facet_to_old_facet.insert(
                new_facet_to_old_facet.end(),
                local_data.new_facet_to_old_facet.begin(),
                local_data.new_facet_to_old_facet.end());
            for (const auto& corners : local_data.new_facet_corner_sources) {
                corner_sources.insert(corner_sources.end(), corners.begin(), corners.end());
            }
        }
    }
}

// Propagate vertex, facet, corner, and indexed attributes from the parent mesh onto the sub-facets
// produced by the connectivity pass. Facet attributes are inherited from the parent facet; corner
// and indexed attributes are linearly interpolated within the parent facet using the recorded
// corner provenance. `new_facet_to_old_facet` maps each result facet to its parent facet, and
// `corner_sources` maps each result corner to its provenance; both are indexed in result order.
template <typename Scalar, typename Index>
void propagate_attributes(
    const IsolineField<Scalar, Index>& field,
    SurfaceMesh<Scalar, Index>& result,
    const tbb::concurrent_vector<Index>& new_vertex_to_provoking_corner,
    Index num_vertices,
    const std::vector<Index>& new_facet_to_old_facet,
    const std::vector<CornerSource<Index>>& corner_sources)
{
    const auto& mesh = field.mesh;

    // Interpolate vertex attributes for the new isocrossing vertices.
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
            interpolate_row(
                new_values,
                v,
                old_values,
                field.c2v[ci],
                field.c2v[cj],
                field.crossing_param(ci));
        }
    });

    // Copy facet attributes from each parent facet to the sub-facets it produced.
    par_foreach_named_attribute_read<Facet>(mesh, [&](auto name, auto&& old_attr) {
        if (mesh.attr_name_is_reserved(name)) {
            return;
        }
        using AttributeType = std::decay_t<decltype(old_attr)>;
        using ValueType = typename AttributeType::ValueType;
        auto old_values = matrix_view(old_attr);
        auto& new_attr = result.template ref_attribute<ValueType>(name);
        auto new_values = matrix_ref(new_attr);
        for (Index f = 0; f < result.get_num_facets(); ++f) {
            new_values.row(f) = old_values.row(new_facet_to_old_facet[f]);
        }
    });

    // Interpolate corner attributes within each parent facet.
    par_foreach_named_attribute_read<Corner>(mesh, [&](auto name, auto&& old_attr) {
        if (mesh.attr_name_is_reserved(name)) {
            return;
        }
        using AttributeType = std::decay_t<decltype(old_attr)>;
        using ValueType = typename AttributeType::ValueType;
        auto old_values = matrix_view(old_attr);
        auto& new_attr = result.template ref_attribute<ValueType>(name);
        auto new_values = matrix_ref(new_attr);
        for (Index c = 0; c < result.get_num_corners(); ++c) {
            const auto& src = corner_sources[c];
            interpolate_row(new_values, c, old_values, src.c0, src.c1, src.t);
        }
    });

    // Interpolate indexed attributes within each parent facet. Surviving corners keep their
    // original value index (preserving sharing), while each isocrossing corner gets a freshly
    // interpolated value appended to the value buffer.
    par_foreach_named_attribute_read<Indexed>(mesh, [&](auto name, auto&& old_attr) {
        if (mesh.attr_name_is_reserved(name)) {
            return;
        }
        using AttributeType = std::decay_t<decltype(old_attr)>;
        using ValueType = typename AttributeType::ValueType;
        auto old_values = matrix_view(old_attr.values());
        auto old_indices = old_attr.indices().get_all();

        auto& new_attr = result.template ref_indexed_attribute<ValueType>(name);
        const size_t old_num_values = new_attr.values().get_num_elements();
        size_t num_new_values = 0;
        for (const auto& src : corner_sources) {
            if (src.c0 != src.c1) ++num_new_values;
        }
        new_attr.values().resize_elements(old_num_values + num_new_values);
        auto new_values = matrix_ref(new_attr.values());
        auto new_indices = new_attr.indices().ref_all();
        size_t next_value = old_num_values;
        for (Index c = 0; c < result.get_num_corners(); ++c) {
            const auto& src = corner_sources[c];
            if (src.c0 == src.c1) {
                new_indices[c] = old_indices[src.c0];
            } else {
                interpolate_row(
                    new_values,
                    static_cast<Eigen::Index>(next_value),
                    old_values,
                    old_indices[src.c0],
                    old_indices[src.c1],
                    src.t);
                new_indices[c] = static_cast<Index>(next_value);
                ++next_value;
            }
        }
    });
}

template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> isoline_internal(
    SurfaceMesh<Scalar, Index> mesh,
    const IsolineOptions& options,
    IsolineMode mode,
    const Attribute<double>& values_,
    const Attribute<Index>* indices_ = nullptr)
{
    SurfaceMesh<Scalar, Index> result = mesh;
    const bool keep_attributes = options.keep_attributes;
    if (!keep_attributes) {
        result = SurfaceMesh<Scalar, Index>::stripped_copy(mesh);
    }
    mesh.initialize_edges();
    result.clear_facets();

    const IsolineField<Scalar, Index> field(mesh, options, values_, indices_);

    // New vertex ids for edge isocrossing are associated to the "provoking" corner of the edge. We
    // may have shared ids due to corners from different facets sharing the same indices for the
    // isovalue attribute. So we use union-find to "join" provoking corners that share the same
    // isovalue indices.
    const Index num_vertices = mesh.get_num_vertices();
    std::vector<Index> repr_to_new_vertex(mesh.get_num_corners(), invalid<Index>());
    DisjointSets<Index> repr_corner(mesh.get_num_corners());
    tbb::enumerable_thread_specific<LocalDataT<Index>> data;

    // Pass 1: assign new output-vertex ids to the edge isocrossings.
    auto new_vertex_to_provoking_corner =
        assign_crossing_vertices(field, num_vertices, repr_corner, repr_to_new_vertex, data);

    // Pass 2: compute the positions of the new vertices.
    result.add_vertices(
        static_cast<Index>(new_vertex_to_provoking_corner.size()),
        [&](Index v, span<Scalar> pos) {
            field.interpolate_position(new_vertex_to_provoking_corner[v], pos);
        });

    // Pass 3: compute the connectivity of the output facets.
    build_subfacets(field, mode, keep_attributes, repr_corner, repr_to_new_vertex, data);

    // Gather the new facets (and, when keeping attributes, the provenance maps) into the result.
    std::vector<Index> new_facet_to_old_facet;
    std::vector<CornerSource<Index>> corner_sources;
    gather_subfacets(data, keep_attributes, result, new_facet_to_old_facet, corner_sources);

    if (keep_attributes) {
        propagate_attributes(
            field,
            result,
            new_vertex_to_provoking_corner,
            num_vertices,
            new_facet_to_old_facet,
            corner_sources);
    }

    // Finally, remove isolated vertices caused by removed facets.
    remove_isolated_vertices(result);

    return result;
}

template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> isoline_internal(
    const SurfaceMesh<Scalar, Index>& mesh,
    const IsolineOptions& options,
    IsolineMode mode)
{
    la_runtime_assert(
        mesh.is_triangle_mesh(),
        "Isoline extraction/trimming/insertion only works for triangle meshes");

    SurfaceMesh<Scalar, Index> result;

    // We save on compiled binary size by explicitly casting the attribute to double before
    // performing the actual trimming operation...
    internal::visit_attribute_read(mesh, options.attribute_id, [&](auto&& attr) {
        using AttributeType = std::decay_t<decltype(attr)>;
        using ValueType = typename AttributeType::ValueType;
        if (!(attr.get_element_type() == AttributeElement::Vertex ||
              attr.get_element_type() == AttributeElement::Indexed)) {
            throw Error(format(
                "Isoline attribute element type should be Vertex or Indexed, not {}",
                internal::to_string(attr.get_element_type())));
        }
        if constexpr (AttributeType::IsIndexed) {
            if constexpr (std::is_same_v<ValueType, double>) {
                result = isoline_internal(mesh, options, mode, attr.values(), &attr.indices());
            } else {
                result = isoline_internal(
                    mesh,
                    options,
                    mode,
                    Attribute<double>::cast_copy(attr.values()),
                    &attr.indices());
            }
        } else {
            if constexpr (std::is_same_v<ValueType, double>) {
                result = isoline_internal(mesh, options, mode, attr);
            } else {
                result = isoline_internal(mesh, options, mode, Attribute<double>::cast_copy(attr));
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
    return isoline_internal(mesh, options, IsolineMode::Trim);
}

template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> extract_isoline(
    const SurfaceMesh<Scalar, Index>& mesh,
    const IsolineOptions& options)
{
    return isoline_internal(mesh, options, IsolineMode::Extract);
}

template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> insert_isoline(
    const SurfaceMesh<Scalar, Index>& mesh,
    const IsolineOptions& options)
{
    return isoline_internal(mesh, options, IsolineMode::Insert);
}

#define LA_X_isoline(_, Scalar, Index)                               \
    template LA_CORE_API SurfaceMesh<Scalar, Index> trim_by_isoline( \
        const SurfaceMesh<Scalar, Index>& mesh,                      \
        const IsolineOptions& options);                              \
    template LA_CORE_API SurfaceMesh<Scalar, Index> extract_isoline( \
        const SurfaceMesh<Scalar, Index>& mesh,                      \
        const IsolineOptions& options);                              \
    template LA_CORE_API SurfaceMesh<Scalar, Index> insert_isoline(  \
        const SurfaceMesh<Scalar, Index>& mesh,                      \
        const IsolineOptions& options);
LA_SURFACE_MESH_X(isoline, 0)

} // namespace lagrange
