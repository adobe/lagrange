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

#include <vector>

#include <lagrange/Mesh.h>
#include <lagrange/MeshTrait.h>
#include <lagrange/attributes/condense_indexed_attribute.h>
#include <lagrange/common.h>
#include <lagrange/compute_vertex_valence.h>
#include <lagrange/utils/la_assert.h>
#include <lagrange/utils/range.h>

namespace lagrange {
template <typename MeshType>
void map_vertex_attribute_to_corner_attribute(MeshType& mesh, const std::string& attr_name)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "MeshType is not a mesh");

    using Index = typename MeshType::Index;
    LA_ASSERT(mesh.has_vertex_attribute(attr_name));

    const Index num_facets = mesh.get_num_facets();
    const Index vertex_per_facet = mesh.get_vertex_per_facet();

    const auto& facets = mesh.get_facets();
    auto vertex_attr = mesh.get_vertex_attribute_array(attr_name);

    auto map_fn = [&](Eigen::Index ci, std::vector<std::pair<Eigen::Index, double>>& weights) {
        weights.clear();
        weights.emplace_back(
            safe_cast<Eigen::Index>(facets(ci / vertex_per_facet, ci % vertex_per_facet)),
            1.0);
    };
    auto corner_attr = to_shared_ptr(vertex_attr->row_slice(num_facets * vertex_per_facet, map_fn));

    if (!mesh.has_corner_attribute(attr_name)) {
        mesh.add_corner_attribute(attr_name);
    }
    mesh.set_corner_attribute_array(attr_name, std::move(corner_attr));
}

template <typename MeshType>
void map_facet_attribute_to_corner_attribute(MeshType& mesh, const std::string& attr_name)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "MeshType is not a mesh");

    using Index = typename MeshType::Index;
    LA_ASSERT(mesh.has_facet_attribute(attr_name));

    const Index num_facets = mesh.get_num_facets();
    const Index vertex_per_facet = mesh.get_vertex_per_facet();

    auto facet_attr = mesh.get_facet_attribute_array(attr_name);

    auto map_fn = [&](Eigen::Index ci, std::vector<std::pair<Eigen::Index, double>>& weights) {
        weights.clear();
        weights.emplace_back(safe_cast<Eigen::Index>(ci / vertex_per_facet), 1.0);
    };
    auto corner_attr = to_shared_ptr(facet_attr->row_slice(num_facets * vertex_per_facet, map_fn));

    if (!mesh.has_corner_attribute(attr_name)) {
        mesh.add_corner_attribute(attr_name);
    }
    mesh.set_corner_attribute_array(attr_name, std::move(corner_attr));
}

template <typename MeshType>
void map_corner_attribute_to_vertex_attribute(MeshType& mesh, const std::string& attr_name)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "MeshType is not a mesh");

    using Index = typename MeshType::Index;
    LA_ASSERT(mesh.has_corner_attribute(attr_name));

    const Index num_vertices = mesh.get_num_vertices();
    const Index num_facets = mesh.get_num_facets();
    const Index vertex_per_facet = mesh.get_vertex_per_facet();

    const auto& facets = mesh.get_facets();
    auto corner_attr = mesh.get_corner_attribute_array(attr_name);

    if (!mesh.has_vertex_attribute("valence")) {
        compute_vertex_valence(mesh);
    }
    const auto& valence = mesh.get_vertex_attribute("valence");
    LA_ASSERT(valence.rows() == num_vertices);

    std::vector<std::vector<std::pair<Eigen::Index, double>>> weights(num_vertices);
    for (Index i = 0; i < num_facets; i++) {
        for (Index j = 0; j < vertex_per_facet; j++) {
            const double w = 1.0 / valence(facets(i, j), 0);
            weights[facets(i, j)].emplace_back(i * vertex_per_facet + j, w);
        }
    }

    auto mapping_fn = [&](Eigen::Index i, std::vector<std::pair<Eigen::Index, double>>& wts) {
        wts.clear();
        wts = weights[i];
    };
    auto vertex_attr = to_shared_ptr(corner_attr->row_slice(num_vertices, mapping_fn));

    if (!mesh.has_vertex_attribute(attr_name)) {
        mesh.add_vertex_attribute(attr_name);
    }
    mesh.set_vertex_attribute_array(attr_name, std::move(vertex_attr));
}

template <typename MeshType>
void map_corner_attribute_to_facet_attribute(MeshType& mesh, const std::string& attr_name)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "MeshType is not a mesh");

    using Index = typename MeshType::Index;
    LA_ASSERT(mesh.has_corner_attribute(attr_name));

    const Index num_facets = mesh.get_num_facets();
    const Index vertex_per_facet = mesh.get_vertex_per_facet();

    auto corner_attr = mesh.get_corner_attribute_array(attr_name);

    auto mapping_fn = [&](Eigen::Index i, std::vector<std::pair<Eigen::Index, double>>& weights) {
        weights.clear();
        for (auto j : range(vertex_per_facet)) {
            weights.emplace_back(i * vertex_per_facet + j, 1.0 / vertex_per_facet);
        }
    };

    auto facet_attr = corner_attr->row_slice(num_facets, mapping_fn);

    if (!mesh.has_facet_attribute(attr_name)) {
        mesh.add_facet_attribute(attr_name);
    }
    mesh.set_facet_attribute_array(attr_name, std::move(facet_attr));
}

template <typename MeshType>
void map_vertex_attribute_to_indexed_attribute(MeshType& mesh, const std::string& attr_name)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "MeshType is not a mesh");

    LA_ASSERT(mesh.has_vertex_attribute(attr_name));

    const auto& attr_values = mesh.get_vertex_attribute(attr_name);
    const auto& attr_indices = mesh.get_facets();

    if (!mesh.has_indexed_attribute(attr_name)) {
        mesh.add_indexed_attribute(attr_name);
    }
    mesh.set_indexed_attribute(attr_name, attr_values, attr_indices);
    condense_indexed_attribute(mesh, attr_name);
}

template <typename MeshType>
void map_facet_attribute_to_indexed_attribute(MeshType& mesh, const std::string& attr_name)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "MeshType is not a mesh");

    using IndexArray = typename MeshType::IndexArray;
    LA_ASSERT(mesh.has_facet_attribute(attr_name));

    const auto num_facets = mesh.get_num_facets();
    const auto vertex_per_facet = mesh.get_vertex_per_facet();
    auto attr_values = mesh.get_facet_attribute(attr_name);
    IndexArray attr_indices(num_facets, vertex_per_facet);
    for (auto i : range(num_facets)) {
        attr_indices.row(i).setConstant(i);
    }

    if (!mesh.has_indexed_attribute(attr_name)) {
        mesh.add_indexed_attribute(attr_name);
    }
    mesh.import_indexed_attribute(attr_name, attr_values, attr_indices);
    condense_indexed_attribute(mesh, attr_name);
}

template <typename MeshType>
void map_corner_attribute_to_indexed_attribute(MeshType& mesh, const std::string& attr_name)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "MeshType is not a mesh");

    using IndexArray = typename MeshType::IndexArray;
    LA_ASSERT(mesh.has_corner_attribute(attr_name));

    const auto num_facets = mesh.get_num_facets();
    const auto vertex_per_facet = mesh.get_vertex_per_facet();
    auto attr_values = mesh.get_corner_attribute(attr_name);
    IndexArray attr_indices(num_facets, vertex_per_facet);
    for (auto i : range(num_facets)) {
        for (auto j : range(vertex_per_facet)) {
            attr_indices(i, j) = i * vertex_per_facet + j;
        }
    }

    if (!mesh.has_indexed_attribute(attr_name)) {
        mesh.add_indexed_attribute(attr_name);
    }
    mesh.import_indexed_attribute(attr_name, attr_values, attr_indices);
    condense_indexed_attribute(mesh, attr_name);
}

template <typename MeshType>
void map_indexed_attribute_to_vertex_attribute(MeshType& mesh, const std::string& attr_name)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "MeshType is not a mesh");
    LA_ASSERT(mesh.has_indexed_attribute(attr_name));

    using AttributeArray = typename MeshType::AttributeArray;
    using Scalar = typename MeshType::Scalar;

    const auto entry = mesh.get_indexed_attribute(attr_name);
    const auto& attr_values = std::get<0>(entry);
    const auto& attr_indices = std::get<1>(entry);

    const auto num_vertices = mesh.get_num_vertices();
    const auto num_facets = mesh.get_num_facets();
    const auto vertex_per_facet = mesh.get_vertex_per_facet();
    const auto& facets = mesh.get_facets();
    AttributeArray vertex_attr(num_vertices, attr_values.cols());
    vertex_attr.setZero();
    Eigen::Matrix<Scalar, Eigen::Dynamic, 1> valence(num_vertices);
    valence.setZero();
    for (auto i : range(num_facets)) {
        for (auto j : range(vertex_per_facet)) {
            vertex_attr.row(facets(i, j)) += attr_values.row(attr_indices(i, j));
            valence[facets(i, j)] += 1;
        }
    }
    vertex_attr.array().colwise() /= valence.array();

    if (!mesh.has_vertex_attribute(attr_name)) {
        mesh.add_vertex_attribute(attr_name);
    }
    mesh.import_vertex_attribute(attr_name, vertex_attr);
}

template <typename MeshType>
void map_indexed_attribute_to_facet_attribute(MeshType& mesh, const std::string& attr_name)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "MeshType is not a mesh");
    LA_ASSERT(mesh.has_indexed_attribute(attr_name));

    using AttributeArray = typename MeshType::AttributeArray;

    const auto entry = mesh.get_indexed_attribute(attr_name);
    const auto& attr_values = std::get<0>(entry);
    const auto& attr_indices = std::get<1>(entry);

    const auto num_facets = mesh.get_num_facets();
    const auto vertex_per_facet = mesh.get_vertex_per_facet();
    AttributeArray facet_attr(num_facets, attr_values.cols());
    facet_attr.setZero();
    for (auto i : range(num_facets)) {
        for (auto j : range(vertex_per_facet)) {
            facet_attr.row(i) += attr_values.row(attr_indices(i, j)) / vertex_per_facet;
        }
    }

    if (!mesh.has_facet_attribute(attr_name)) {
        mesh.add_facet_attribute(attr_name);
    }
    mesh.import_facet_attribute(attr_name, facet_attr);
}

template <typename MeshType>
void map_indexed_attribute_to_corner_attribute(MeshType& mesh, const std::string& attr_name)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "MeshType is not a mesh");
    LA_ASSERT(mesh.has_indexed_attribute(attr_name));

    using AttributeArray = typename MeshType::AttributeArray;
    using Index = typename MeshType::Index;

    const auto entry = mesh.get_indexed_attribute(attr_name);
    const auto& attr_values = std::get<0>(entry);
    const auto& attr_indices = std::get<1>(entry);

    const auto num_facets = mesh.get_num_facets();
    const auto vertex_per_facet = mesh.get_vertex_per_facet();
    AttributeArray corner_attr(num_facets * vertex_per_facet, attr_values.cols());
    corner_attr.setZero();
    for (auto i : range(num_facets)) {
        for (auto j : range(vertex_per_facet)) {
            if (attr_indices(i, j) != INVALID<Index>()) {
                corner_attr.row(i * vertex_per_facet + j) = attr_values.row(attr_indices(i, j));
            }
        }
    }

    if (!mesh.has_corner_attribute(attr_name)) {
        mesh.add_corner_attribute(attr_name);
    }
    mesh.import_corner_attribute(attr_name, corner_attr);
}

} // namespace lagrange
