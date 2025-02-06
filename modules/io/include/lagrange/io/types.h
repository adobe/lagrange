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

#include <lagrange/AttributeFwd.h>
#include <lagrange/fs/filesystem.h>
#include <lagrange/scene/SceneExtension.h>
#include <lagrange/utils/warning.h>

#include <vector>

namespace lagrange {
namespace io {

enum class FileEncoding { Binary, Ascii };
enum class FileFormat { Obj, Ply, Gltf, Msh, Fbx, Stl, Unknown };

/**
 * Options used when saving a mesh or a scene.
 * Note that not all options are supported for all backends or filetypes.
 */
struct SaveOptions
{
    /**
     * Whether to encode the file as plain text or binary.
     * Some filetypes only support Ascii and will ignore this parameter.
     */
    FileEncoding encoding = FileEncoding::Binary;

    /**
     * Which attributes to save with the mesh?
     */
    enum class OutputAttributes {
        All, ///< All attributes (default)
        SelectedOnly, ///< Only attributes listed in `selected_attributes`
    };

    OutputAttributes output_attributes = OutputAttributes::All;

    /// Attributes to output, usage depends on the above.
    std::vector<AttributeId> selected_attributes;

    /**
     * While Lagrange SurfaceMesh supports vertex, facet, corner, edge and indexed attributes, many
     * filetypes only support a subset of these attribute types. The `AttributeConversionPolicy`
     * provides the options to handle non-supported attributes when saving them.
     */
    enum class AttributeConversionPolicy {
        ExactMatchOnly, ///< Ignore mismatched attributes and print a warning
        ConvertAsNeeded, ///< Convert attribute to supported attribute type when possible.
    };

    /// The attribute conversion policy to use.
    AttributeConversionPolicy attribute_conversion_policy =
        AttributeConversionPolicy::ExactMatchOnly;

    /// Whether to embed images in the file (if supported by the filetype)
    bool embed_images = false;

    std::vector<scene::UserDataConverter*> extension_converters;
};

/**
 * Options used when loading a mesh or a scene.
 * Note that not all options are supported for all backends or filetypes.
 */
struct LoadOptions
{
    LA_IGNORE_DEPRECATION_WARNING_BEGIN

    LoadOptions() = default;
    LoadOptions(LoadOptions&&) = default;
    LoadOptions& operator=(LoadOptions&&) = default;
    LoadOptions(const LoadOptions&) = default;
    LoadOptions& operator=(const LoadOptions&) = default;

    /// Load object ids as facet attribute
    [[deprecated("Use load_object_ids instead")]] bool load_object_id = true;

    LA_IGNORE_DEPRECATION_WARNING_END

    /// Triangulate any polygonal facet with > 3 vertices
    bool triangulate = false;

    /// Load vertex normals
    bool load_normals = true;

    /// Load tangents and bitangents
    bool load_tangents = true;

    /// Load texture coordinates
    bool load_uvs = true;

    /// Load skinning weights attributes (joints id and weight).
    bool load_weights = true;

    /// Load material ids as facet attribute
    bool load_materials = true;

    /// Load vertex colors as vertex attribute
    bool load_vertex_colors = true;

    /// Load object ids as facet attribute
    bool load_object_ids = true;

    /// Load external images
    bool load_images = true;

    /// Stitch duplicate boundary vertices together when loading file formats such as glTF
    bool stitch_vertices = false;

    /**
     * Search path for related files, such as .mtl, .bin, or image textures.
     * By default, searches the same folder as the provided filename.
     */
    fs::path search_path;

    std::vector<scene::UserDataConverter*> extension_converters;
};

} // namespace io
} // namespace lagrange
