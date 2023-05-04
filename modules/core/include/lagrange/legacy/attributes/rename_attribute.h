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

#include <string>

#include <lagrange/Mesh.h>
#include <lagrange/MeshTrait.h>
#include <lagrange/common.h>
#include <lagrange/legacy/inline.h>
#include <lagrange/utils/assert.h>

namespace lagrange {
LAGRANGE_LEGACY_INLINE
namespace legacy {

template <typename MeshType>
void rename_vertex_attribute(
    MeshType& mesh,
    const std::string& old_name,
    const std::string& new_name)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "MeshType is not a mesh");

    using AttributeArray = typename MeshType::AttributeArray;
    la_runtime_assert(mesh.has_vertex_attribute(old_name));

    AttributeArray attr_values;
    mesh.export_vertex_attribute(old_name, attr_values);
    mesh.remove_vertex_attribute(old_name);
    mesh.add_vertex_attribute(new_name);
    mesh.import_vertex_attribute(new_name, attr_values);
}

template <typename MeshType>
void rename_facet_attribute(
    MeshType& mesh,
    const std::string& old_name,
    const std::string& new_name)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "MeshType is not a mesh");

    using AttributeArray = typename MeshType::AttributeArray;
    la_runtime_assert(mesh.has_facet_attribute(old_name));

    AttributeArray attr_values;
    mesh.export_facet_attribute(old_name, attr_values);
    mesh.remove_facet_attribute(old_name);
    mesh.add_facet_attribute(new_name);
    mesh.import_facet_attribute(new_name, attr_values);
}

template <typename MeshType>
void rename_corner_attribute(
    MeshType& mesh,
    const std::string& old_name,
    const std::string& new_name)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "MeshType is not a mesh");

    using AttributeArray = typename MeshType::AttributeArray;
    la_runtime_assert(mesh.has_corner_attribute(old_name));

    AttributeArray attr_values;
    mesh.export_corner_attribute(old_name, attr_values);
    mesh.remove_corner_attribute(old_name);
    mesh.add_corner_attribute(new_name);
    mesh.import_corner_attribute(new_name, attr_values);
}

template <typename MeshType>
void rename_edge_attribute(MeshType& mesh, const std::string& old_name, const std::string& new_name)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "MeshType is not a mesh");

    using AttributeArray = typename MeshType::AttributeArray;
    la_runtime_assert(mesh.has_edge_attribute(old_name));

    AttributeArray attr_values;
    mesh.export_edge_attribute(old_name, attr_values);
    mesh.remove_edge_attribute(old_name);
    mesh.add_edge_attribute(new_name);
    mesh.import_edge_attribute(new_name, attr_values);
}

template <typename MeshType>
void rename_indexed_attribute(
    MeshType& mesh,
    const std::string& old_name,
    const std::string& new_name)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "MeshType is not a mesh");

    using AttributeArray = typename MeshType::AttributeArray;
    using IndexArray = typename MeshType::IndexArray;
    la_runtime_assert(mesh.has_indexed_attribute(old_name));

    AttributeArray attr_values;
    IndexArray attr_indices;
    mesh.export_indexed_attribute(old_name, attr_values, attr_indices);
    mesh.remove_indexed_attribute(old_name);
    mesh.add_indexed_attribute(new_name);
    mesh.import_indexed_attribute(new_name, attr_values, attr_indices);
}

} // namespace legacy
} // namespace lagrange
