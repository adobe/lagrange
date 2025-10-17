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
#include <lagrange/ui/components/Input.h>
#include <lagrange/ui/default_shaders.h>
#include <lagrange/ui/types/ShaderLoader.h>


namespace lagrange {
namespace ui {


void register_default_shaders(Registry& r)
{
    {
        ShaderDefinition d;
        d.path = "surface/simple.shader";
        d.path_type = ShaderLoader::PathType::VIRTUAL;
        d.display_name = "Simple";
        register_shader_as(r, DefaultShaders::Simple, d);
    }


    {
        ShaderDefinition d;
        d.path = "surface/surface.shader";
        d.path_type = ShaderLoader::PathType::VIRTUAL;
        d.display_name = "Physically Based Render";
        d.defines = ShaderDefines{{"SHADING_PBR", ""}};
        register_shader_as(r, DefaultShaders::PBR, d);
    }

    {
        ShaderDefinition d;
        d.path = "surface/surface.shader";
        d.path_type = ShaderLoader::PathType::VIRTUAL;
        d.display_name = "Skeletal Physically Based Render";
        d.defines = ShaderDefines{{"SHADING_PBR", ""}, {"SKELETAL", ""}};
        register_shader_as(r, DefaultShaders::PBRSkeletal, d);
    }


    {
        ShaderDefinition d;
        d.path = "texture.shader";
        d.path_type = ShaderLoader::PathType::VIRTUAL;
        d.display_name = "TextureView";
        register_shader_as(r, DefaultShaders::TextureView, d);
    }


    {
        ShaderDefinition d;
        d.path = "lines/triangle_to_lines.shader";
        d.path_type = ShaderLoader::PathType::VIRTUAL;
        d.display_name = "TrianglesToLines";
        register_shader_as(r, DefaultShaders::TrianglesToLines, d);
    }

    {
        ShaderDefinition d;
        d.path = "post/outline.shader";
        d.path_type = ShaderLoader::PathType::VIRTUAL;
        d.display_name = "Outline";
        register_shader_as(r, DefaultShaders::Outline, d);
    }

    {
        ShaderDefinition d;
        d.path = "depth/to_texture.shader";
        d.path_type = ShaderLoader::PathType::VIRTUAL;
        d.display_name = "ShadowDepth";
        register_shader_as(r, DefaultShaders::ShadowDepth, d);
    }

    {
        ShaderDefinition d;
        d.path = "depth/to_cubemap.shader";
        d.path_type = ShaderLoader::PathType::VIRTUAL;
        d.display_name = "ShadowCubemap";
        register_shader_as(r, DefaultShaders::ShadowCubemap, d);
    }


    {
        ShaderDefinition d;
        d.path = "surface/vertex_attribute.shader";
        d.path_type = ShaderLoader::PathType::VIRTUAL;
        d.display_name = "Surface Attribute";
        register_shader_as(r, DefaultShaders::SurfaceVertexAttribute, d);
    }

    {
        ShaderDefinition d;
        d.path = "lines/attribute_to_lines.shader";
        d.path_type = ShaderLoader::PathType::VIRTUAL;
        d.display_name = "Line Attribute";
        register_shader_as(r, DefaultShaders::LineVertexAttribute, d);
    }

    {
        ShaderDefinition d;
        d.path = "surface/edge_attribute.shader";
        d.path_type = ShaderLoader::PathType::VIRTUAL;
        d.display_name = "Surface Attribute (Edge)";
        register_shader_as(r, DefaultShaders::SurfaceEdgeAttribute, d);
    }

    {
        ShaderDefinition d;
        d.path = "surface/objectid.shader";
        d.path_type = ShaderLoader::PathType::VIRTUAL;
        d.display_name = "ObjectID";
        register_shader_as(r, DefaultShaders::ObjectID, d);
    }


    {
        ShaderDefinition d;
        d.path = "face_id.shader";
        d.path_type = ShaderLoader::PathType::VIRTUAL;
        d.display_name = "MeshElementID";
        register_shader_as(r, DefaultShaders::MeshElementID, d);
    }

    {
        ShaderDefinition d;
        d.path = "skybox.shader";
        d.path_type = ShaderLoader::PathType::VIRTUAL;
        d.display_name = "Skybox";
        register_shader_as(r, DefaultShaders::Skybox, d);
    }

    {
        ShaderDefinition d;
        d.path = "lines/edge_to_line.shader";
        d.path_type = ShaderLoader::PathType::VIRTUAL;
        d.display_name = "Edges To Lines";
        register_shader_as(r, DefaultShaders::EdgesToLines, d);
    }
}

} // namespace ui
} // namespace lagrange
