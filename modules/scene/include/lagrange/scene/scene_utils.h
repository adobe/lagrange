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
#pragma once

#include <lagrange/scene/Scene.h>
#include <lagrange/SurfaceMesh.h>

namespace lagrange::scene::utils {

// add child to the node. Assumes the child was already added to the scene.
inline void add_child(Node& node, ElementId child_index)
{
    node.children.push_back(child_index);
}

// add child to the node and the scene. Assumes the child was not already added to the scene.
template <typename Scalar, typename Index>
ElementId add_child(Scene<Scalar, Index>& scene, Node& node, Node child)
{
    ElementId child_idx = scene.nodes.size();
    scene.nodes.push_back(child);
    node.children.push_back(child_idx);
    return child_idx;
}

// add mesh to the scene (but not to the node graph!) and return its index.
template <typename Scalar, typename Index>
ElementId add_mesh(Scene<Scalar, Index>& scene, SurfaceMesh<Scalar, Index> mesh)
{
    scene.meshes.emplace_back(std::move(mesh));
    return scene.meshes.size() - 1;
}

// Returns the global transform of a node.
// Note that this has to traverse the node hierarchy up to the root.
// Considering saving the global transforms if you need them often.
template <typename Scalar, typename Index>
Eigen::Affine3f compute_global_node_transform(const Scene<Scalar, Index>& scene, ElementId node_idx)
{
    if (node_idx == lagrange::invalid<size_t>()) return Eigen::Affine3f::Identity();
    const Node& n = scene.nodes[node_idx];
    return compute_global_node_transform<Scalar, Index>(scene, n.parent) * n.transform;
}

/**
 * Lagrange scene and most 3d software use UV texture coordinates.
 * OpenGL and gltf use ST texture coordinates.
 * Those are almost the same thing, except they invert the v/t axis:
 * s = u
 * t = 1-v
 *
 * In practice, we call this function to convert between the two when dealing with gltf.
 *
 * @tparam Scalar        Scene mesh scalar type.
 * @tparam Index         Scene mesh index type.
 *
 * @param mesh           The mesh to modify.
 * @param attribute_id   The attribute to change. By default converts all attributes with usage=UV.
 */
template <typename Scalar, typename Index>
void convert_texcoord_uv_st(
    SurfaceMesh<Scalar, Index>& mesh,
    AttributeId attribute_id = invalid<AttributeId>());

} // namespace lagrange::scene::utils
