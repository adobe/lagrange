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

#include <lagrange/Mesh.h>
#include <lagrange/MeshTrait.h>
#include <lagrange/legacy/inline.h>
#include <lagrange/utils/range.h>
#include <tbb/parallel_for.h>

#include <functional>
#include <string>

/**
 * Evaluates a function for each mesh element and stores the result as an attribute
 */

namespace lagrange {
LAGRANGE_LEGACY_INLINE
namespace legacy {

template <typename MeshType>
void eval_as_vertex_attribute(
    MeshType& mesh,
    const std::string& attribute_name,
    const std::function<typename MeshType::Scalar(typename MeshType::Index v_idx)>& func,
    bool parallel = true)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");

    using AttributeArray = typename MeshType::AttributeArray;
    using Index = typename MeshType::Index;

    const auto num_vertices = mesh.get_num_vertices();
    AttributeArray attr(num_vertices, 1);

    if (parallel) {
        tbb::parallel_for(
            tbb::blocked_range<Index>(0, num_vertices),
            [&](const tbb::blocked_range<Index>& tbb_range) {
                for (auto v_idx = tbb_range.begin(); v_idx != tbb_range.end(); ++v_idx) {
                    attr(v_idx, 0) = func(v_idx);
                };
            });
    } else {
        for (auto v_idx : range(num_vertices)) {
            attr(v_idx, 0) = func(v_idx);
        }
    }
    if (!mesh.has_vertex_attribute(attribute_name)) {
        mesh.add_vertex_attribute(attribute_name);
    }
    mesh.import_vertex_attribute(attribute_name, attr);
}

template <typename MeshType>
void eval_as_vertex_attribute(
    MeshType& mesh,
    const std::string& attribute_name,
    const std::function<typename MeshType::Scalar(const typename MeshType::VertexType& V)>& func,
    bool parallel = true)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");

    using Index = typename MeshType::Index;

    const auto& V = mesh.get_vertices();
    return eval_as_vertex_attribute(
        mesh,
        attribute_name,
        [&V, &func](Index v_idx) { return func(V.row(v_idx)); },
        parallel);
}

template <typename MeshType>
void eval_as_vertex_attribute(
    MeshType& mesh,
    const std::string& attribute_name,
    const std::function<typename MeshType::Scalar(
        typename MeshType::Scalar x,
        typename MeshType::Scalar y,
        typename MeshType::Scalar z)>& func,
    bool parallel = true)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");

    using Index = typename MeshType::Index;

    const auto& V = mesh.get_vertices();
    return eval_as_vertex_attribute(
        mesh,
        attribute_name,
        [&V, &func](Index v_idx) { return func(V(v_idx, 0), V(v_idx, 1), V(v_idx, 2)); },
        parallel);
}

template <typename MeshType>
void eval_as_facet_attribute(
    MeshType& mesh,
    const std::string& attribute_name,
    const std::function<typename MeshType::Scalar(typename MeshType::Index f_idx)>& func,
    bool parallel = true)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");

    using AttributeArray = typename MeshType::AttributeArray;
    using Index = typename MeshType::Index;

    const auto num_facets = mesh.get_num_facets();
    AttributeArray attr(num_facets, 1);

    if (parallel) {
        tbb::parallel_for(
            tbb::blocked_range<Index>(0, num_facets),
            [&](const tbb::blocked_range<Index>& tbb_range) {
                for (auto f_idx = tbb_range.begin(); f_idx != tbb_range.end(); ++f_idx) {
                    attr(f_idx, 0) = func(f_idx);
                };
            });
    } else {
        for (auto f_idx : range(num_facets)) {
            attr(f_idx, 0) = func(f_idx);
        }
    }

    if (!mesh.has_facet_attribute(attribute_name)) {
        mesh.add_facet_attribute(attribute_name);
    }
    mesh.import_facet_attribute(attribute_name, attr);
}

template <typename MeshType>
void eval_as_edge_attribute_new(
    MeshType& mesh,
    const std::string& attribute_name,
    const std::function<typename MeshType::Scalar(typename MeshType::Index e_idx)>& func,
    bool parallel = true)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");

    using AttributeArray = typename MeshType::AttributeArray;
    using Index = typename MeshType::Index;

    const auto num_edges = mesh.get_num_edges();
    AttributeArray attr(num_edges, 1);

    if (parallel) {
        tbb::parallel_for(
            tbb::blocked_range<Index>(0, num_edges),
            [&](const tbb::blocked_range<Index>& tbb_range) {
                for (auto e_idx = tbb_range.begin(); e_idx != tbb_range.end(); ++e_idx) {
                    attr(e_idx, 0) = func(e_idx);
                };
            });
    } else {
        for (auto e_idx : range(num_edges)) {
            attr(e_idx, 0) = func(e_idx);
        }
    }

    if (!mesh.has_edge_attribute(attribute_name)) {
        mesh.add_edge_attribute(attribute_name);
    }
    mesh.import_edge_attribute(attribute_name, attr);
}

} // namespace legacy
} // namespace lagrange
