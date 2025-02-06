/*
 * Copyright 2025 Adobe. All rights reserved.
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
#include <lagrange/scene/SceneExtension.h>

#include <string>

namespace lagrange::scene::internal {

///
/// Convert a node to a string representation.
///
/// @param[in]  mesh_instance  Mesh instance to convert.
/// @param[in]  indent         Indentation level.
///
/// @return     A string representation of the mesh instance.
///
std::string to_string(const SceneMeshInstance& mesh_instance, size_t indent = 0);

///
/// Convert a node to a string representation.
///
/// @param[in]  node    Node to convert.
/// @param[in]  indent  Indentation level.
///
/// @return     A string representation of the node.
///
std::string to_string(const Node& node, size_t indent = 0);

///
/// Convert an image buffer to a string representation.
///
/// @param[in]  image   Image buffer to convert.
/// @param[in]  indent  Indentation level.
///
/// @return     A string representation of the image buffer.
///
std::string to_string(const ImageBufferExperimental& image, size_t indent = 0);

///
/// Convert an image to a string representation.
///
/// @param[in]  image   Image to convert.
/// @param[in]  indent  Indentation level.
///
/// @return     A string representation of the image.
///
std::string to_string(const ImageExperimental& image, size_t indent = 0);

///
/// Convert a texture info object to a string representation.
///
/// @param[in]  texture  Texture to convert.
/// @param[in]  indent   Indentation level.
///
/// @return     A string representation of the texture info.
///
std::string to_string(const TextureInfo& texture_info, size_t indent = 0);

///
/// Convert a material to a string representation.
///
/// @param[in]  material  Material to convert.
/// @param[in]  indent    Indentation level.
///
/// @return     A string representation of the material.
///
std::string to_string(const MaterialExperimental& material, size_t indent = 0);

///
/// Convert a Texture to a string representation.
///
/// @param[in]  texture  Texture to convert.
/// @param[in]  indent   Indentation level.
///
/// @return     A string representation of the texture.
///
std::string to_string(const Texture& texture, size_t indent = 0);

///
/// Convert a light to a string representation.
///
/// @param[in]  light   Light to convert.
/// @param[in]  indent  Indentation level.
///
/// @return     A string representation of the light.
///
std::string to_string(const Light& light, size_t indent = 0);


///
/// Convert a camera to a string representation.
///
/// @param[in]  camera  Camera to convert.
/// @param[in]  indent  Indentation level.
///
/// @return     A string representation of the camera.
///
std::string to_string(const Camera& camera, size_t indent = 0);


///
/// Convert an animation to a string representation.
///
/// @param[in]  animation  Animation to convert.
/// @param[in]  indent     Indentation level.
///
/// @return     A string representation of the animation.
///
std::string to_string(const Animation& animation, size_t indent = 0);

///
/// Convert a skeleton to a string representation.
///
/// @param[in]  skeleton  Skeleton to convert.
/// @param[in]  indent    Indentation level.
///
/// @return     A string representation of the skeleton.
///
std::string to_string(const Skeleton& skeleton, size_t indent = 0);

///
/// Convert a scene to a string representation.
///
/// @param[in]  scene   Scene to convert.
/// @param[in]  indent  Indentation level.
///
/// @tparam     Scalar  Scene scalar type.
/// @tparam     Index   Scene index type.
///
/// @return     A string representation of the scene.
///
template <typename Scalar, typename Index>
std::string to_string(const Scene<Scalar, Index>& scene, size_t indent = 0);

///
/// Convert a scene extension to a string representation.
///
/// @param[in]  extensions  Scene extensions to convert.
/// @param[in]  indent      Indentation level.
///
/// @return     A string representation of the scene extensions.
///
std::string to_string(const Extensions& extensions, size_t indent = 0);

} // namespace lagrange::scene::internal
