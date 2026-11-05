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

#include <lagrange/SurfaceMesh.h>
#include <lagrange/scene/Scene.h>
#include <lagrange/scene/api.h>

namespace lagrange::scene::utils {

///
/// @addtogroup module-scene
/// @{
///

/// Appends an existing node index to node.children. Does not update the child's parent or the
/// scene's root_nodes. Prefer Scene::add_child() to maintain both sides of the relationship.
/// @deprecated Use Scene::add_child instead.
[[deprecated("Use Scene::add_child instead")]] inline void add_child(
    Node& node,
    ElementId child_index)
{
    node.children.push_back(child_index);
}

/// Adds a node to scene.nodes and appends its index to node.children. Does not set child.parent
/// or update root_nodes. The node reference must remain valid while scene.nodes grows.
/// @deprecated Use Scene::add_child instead.
template <typename Scalar, typename Index>
[[deprecated("Use Scene::add_child instead")]] ElementId
add_child(Scene<Scalar, Index>& scene, Node& node, Node child)
{
    ElementId child_idx = scene.nodes.size();
    scene.nodes.push_back(child);
    node.children.push_back(child_idx);
    return child_idx;
}

/// Adds a mesh to the scene and returns its index. Does not create a node or mesh instance.
/// @deprecated Use Scene::add instead.
template <typename Scalar, typename Index>
[[deprecated("Use Scene::add(mesh) instead")]] ElementId add_mesh(
    Scene<Scalar, Index>& scene,
    SurfaceMesh<Scalar, Index> mesh)
{
    scene.meshes.emplace_back(std::move(mesh));
    return scene.meshes.size() - 1;
}

/// Computes the node-to-world transform by multiplying local transforms along the parent chain.
/// The chain must be acyclic and contain valid node indices, ending at invalid_element. Passing
/// invalid_element returns identity. Recomputes the chain on each call; root_nodes is not used.
template <typename Scalar, typename Index>
Eigen::Affine3f compute_global_node_transform(const Scene<Scalar, Index>& scene, ElementId node_idx)
{
    if (node_idx == lagrange::invalid<size_t>()) return Eigen::Affine3f::Identity();
    const Node& n = scene.nodes[node_idx];
    return compute_global_node_transform<Scalar, Index>(scene, n.parent) * n.transform;
}

///
/// Computes the (extrinsic) view matrix transform (world space -> camera space).
///
/// Following glTF 2.0 §3.10.2, the view matrix is derived from the node's global transform with
/// scaling ignored. This overload extracts the closest proper rotation from the linear part of
/// @p world_from_local via polar decomposition (SVD), correctly handling scale and shear. For
/// scale-free transforms, prefer the Isometry3f overload to avoid the SVD cost.
/// The camera's position and look_at target are expressed in node-local coordinates. Camera space
/// looks along negative Z with positive Y up.
///
/// @param[in]  camera            Camera object.
/// @param[in]  world_from_local  Local -> world transform of the node containing the camera
///                               instance. Any scale/shear component is stripped before use.
///
/// @return     The view transform mapping world space -> camera space.
///
LA_SCENE_API Eigen::Affine3f camera_view_transform(
    const Camera& camera,
    const Eigen::Affine3f& world_from_local = Eigen::Affine3f::Identity());

///
/// @overload
///
/// Overload accepting an isometry (rotation + translation, no scale). Use this when the transform
/// is known to have no scale component, to skip the SVD decomposition.
///
/// @param[in]  camera            Camera object.
/// @param[in]  world_from_local  Local -> world transform of the node containing the camera
///                               instance.
///
/// @return     The view transform mapping world space -> camera space.
///
LA_SCENE_API Eigen::Affine3f camera_view_transform(
    const Camera& camera,
    const Eigen::Isometry3f& world_from_local);

///
/// Computes the intrinsic projection transform from camera space to OpenGL-style clip space.
/// After perspective division, the near and far planes map to Z = -1 and Z = 1, respectively.
///
/// @param[in]  camera  Camera with positive aspect ratio and near-plane distance. Perspective
///                     cameras require a field of view in (0, pi); an absent far plane means
///                     infinite depth. Orthographic cameras require positive orthographic_width
///                     and a far plane. A finite far-plane distance must exceed the near plane.
///
/// @return     The projection transform mapping camera space -> clip space.
///
LA_SCENE_API Eigen::Projective3f camera_projection_transform(const Camera& camera);

/// Flips the second texture-coordinate component in place: (u, v) -> (u, 1 - v).
/// This operation is its own inverse and is used by glTF I/O to convert texture conventions.
/// Selected attributes must have two channels. For indexed attributes, only the value table is
/// modified; indices are unchanged.
///
/// @tparam Scalar        Scene mesh scalar type.
/// @tparam Index         Scene mesh index type.
///
/// @param[in,out] mesh    The mesh to modify.
/// @param[in] attribute_id  Attribute to change, regardless of usage. By default selects all
///                         attributes with AttributeUsage::UV.
template <typename Scalar, typename Index>
void convert_texcoord_uv_st(
    SurfaceMesh<Scalar, Index>& mesh,
    AttributeId attribute_id = invalid<AttributeId>());

/// @}

} // namespace lagrange::scene::utils
