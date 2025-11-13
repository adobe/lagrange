/*
 * Copyright 2021 Adobe. All rights reserved.
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

#include <lagrange/fs/filesystem.h>
#include <lagrange/ui/Entity.h>
#include <lagrange/ui/api.h>
#include <lagrange/ui/components/IBL.h>

namespace lagrange {
namespace ui {

/// Utility functions for Image Based Lights (IBLs)

/// @brief  Generates Image Based Light from image file or .ibl file given by path.
///         Throws std::runtime_error on failure.
/// @param path
/// @param resolution
/// @return IBL
LA_UI_API IBL generate_ibl(const fs::path& path, size_t resolution = 1024);

/// @brief  Generates Image Based Light from given rectangular texture.
///         Throws std::runtime_error on failure.
/// @param path
/// @param resolution
/// @return IBL
LA_UI_API IBL
generate_ibl(const std::shared_ptr<Texture>& background_texture, size_t resolution = 1024);

/// @brief Returns first <IBL> entity found in registry. If there are none, returns invalid Entity
/// @param registry
/// @return Entity
LA_UI_API Entity get_ibl_entity(const Registry& registry);

/// @brief Returns pointer to the first IBL found in the registry. Nullptr if there is none
/// @param registry
/// @return IBL pointer
LA_UI_API const IBL* get_ibl(const Registry& registry);

/// @copydoc
LA_UI_API IBL* get_ibl(Registry& registry);

/// @brief Adds IBL to the scene
LA_UI_API Entity add_ibl(Registry& registry, IBL ibl);

/// @brief Removes all ibls
LA_UI_API void clear_ibl(Registry& registry);

/// @brief Saves IBL as individual .png files in given folder.
/// Generated files:
/// background_rect.png,
/// background_{00-05}.png,
/// diffuse_{00-05}.png,
/// specular_{00-05}_mip_{00-miplevels}.png
/// Cube maps follow GL_TEXTURE_CUBE_MAP_POSITIVE_X + i pattern.
LA_UI_API bool save_ibl(const IBL& ibl, const fs::path& folder);

} // namespace ui
} // namespace lagrange
