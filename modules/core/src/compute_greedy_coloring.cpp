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
#include <lagrange/compute_greedy_coloring.h>

#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/internal/find_attribute_utils.h>
#include <lagrange/utils/Error.h>
#include <lagrange/utils/invalid.h>
#include <lagrange/views.h>

#include <queue>
#include <random>

namespace lagrange {

namespace {

template <typename Scalar, typename Index, typename Func>
void foreach_facet_around_facet(const SurfaceMesh<Scalar, Index>& mesh, Index f, Func&& func)
{
    for (Index c = mesh.get_facet_corner_begin(f); c != mesh.get_facet_corner_end(f); ++c) {
        mesh.foreach_corner_around_edge(mesh.get_corner_edge(c), [&](Index c1) {
            if (c1 != c) {
                func(mesh.get_corner_facet(c1));
            }
        });
    }
}

template <typename Scalar, typename Index, typename Func>
void foreach_vertex_around_vertex(const SurfaceMesh<Scalar, Index>& mesh, Index v, Func&& func)
{
    mesh.foreach_edge_around_vertex_with_duplicates(v, [&](Index e) {
        auto adj_v = mesh.get_edge_vertices(e);
        for (auto vi : adj_v) {
            if (vi != v) {
                func(vi);
            }
        }
    });
}

template <AttributeElement ElementType>
[[maybe_unused]] inline constexpr bool always_false_v = false;

template <AttributeElement ElementType, typename Scalar, typename Index, typename Func>
void foreach_element_around_element(const SurfaceMesh<Scalar, Index>& mesh, Index i, Func&& func)
{
    if constexpr (ElementType == AttributeElement::Vertex) {
        foreach_vertex_around_vertex(mesh, i, func);
    } else if constexpr (ElementType == AttributeElement::Facet) {
        foreach_facet_around_facet(mesh, i, func);
    } else {
        static_assert(always_false_v<ElementType>, "Unsupported element type");
    }
}

template <typename RngType>
auto bounded_rand(RngType& rng, typename RngType::result_type upper_bound) ->
    typename RngType::result_type
{
    typedef typename RngType::result_type rtype;
    rtype threshold = (RngType::max() - RngType::min() + rtype(1) - upper_bound) % upper_bound;
    for (;;) {
        rtype r = rng() - RngType::min();
        if (r >= threshold) return r % upper_bound;
    }
}

template <AttributeElement ElementType, typename Scalar, typename Index>
AttributeId compute_greedy_coloring(
    SurfaceMesh<Scalar, Index>& mesh,
    std::string_view output_attribute_name,
    size_t num_colors_used)
{
    mesh.initialize_edges();

    AttributeId colors_id = internal::find_or_create_attribute<Index>(
        mesh,
        output_attribute_name,
        ElementType,
        AttributeUsage::Scalar,
        1,
        internal::ResetToDefault::Yes);

    auto colors = attribute_vector_ref<Index>(mesh, colors_id);
    colors.setConstant(invalid<Index>());

    const Index num_elements =
        (ElementType == AttributeElement::Vertex ? mesh.get_num_vertices() : mesh.get_num_facets());
    std::vector<bool> is_color_used;
    std::vector<Index> available_colors;
    std::mt19937 gen;

    auto is_valid = [](Index c) { return (c != invalid<Index>()); };

    auto get_random_available_color = [&](Index i) {
        is_color_used.assign(num_colors_used, false);
        foreach_element_around_element<ElementType>(mesh, i, [&](Index f2) {
            if (is_valid(colors[f2])) {
                is_color_used[colors[f2]] = true;
            }
        });
        available_colors.clear();
        for (Index c = 0; c < static_cast<Index>(num_colors_used); ++c) {
            if (!is_color_used[c]) {
                available_colors.push_back(c);
            }
        }
        if (available_colors.empty()) {
            available_colors.push_back(static_cast<Index>(num_colors_used++));
        }
        const auto upper_bound =
            static_cast<std::mt19937::result_type>(available_colors.size() - 1);
        return available_colors[bounded_rand(gen, upper_bound)];
    };

    std::queue<Index> pending;
    std::vector<bool> marked(num_elements, false);
    for (Index i = 0; i < num_elements; ++i) {
        if (!marked[i]) {
            pending.emplace(i);
            marked[i] = true;
            while (!pending.empty()) {
                Index x = pending.front();
                pending.pop();
                colors[x] = get_random_available_color(x);
                foreach_element_around_element<ElementType>(mesh, x, [&](Index y) {
                    if (!marked[y]) {
                        pending.emplace(y);
                        marked[y] = true;
                    }
                });
            }
        }
    }

    return colors_id;
}

} // namespace

template <typename Scalar, typename Index>
AttributeId compute_greedy_coloring(
    SurfaceMesh<Scalar, Index>& mesh,
    const GreedyColoringOptions& options)
{
    if (options.element_type == AttributeElement::Vertex) {
        return compute_greedy_coloring<AttributeElement::Vertex>(
            mesh,
            options.output_attribute_name,
            options.num_color_used);
    } else if (options.element_type == AttributeElement::Facet) {
        return compute_greedy_coloring<AttributeElement::Facet>(
            mesh,
            options.output_attribute_name,
            options.num_color_used);
    } else {
        throw Error("Unsupported element type");
    }
}

#define LA_X_compute_greedy_coloring(_, Scalar, Index)        \
    template LA_CORE_API AttributeId compute_greedy_coloring( \
        SurfaceMesh<Scalar, Index>&,                          \
        const GreedyColoringOptions&);
LA_SURFACE_MESH_X(compute_greedy_coloring, 0)

} // namespace lagrange
