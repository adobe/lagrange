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
#include "util/material.glsl"

//pragma uniform SHADER_NAME   DISPLAY_NAME     TYPE(DEFAULT_VALUE) [tag0,tag1]
//For Texture, the value is used as a fallback value -> it will generate name_texture_bound and name_default_value
#pragma property material_base_color "Base Color" Texture2D(0.7,0.7,0.7,1)
#pragma property material_roughness "Roughness" Texture2D(0.4)
#pragma property material_metallic "Metallic" Texture2D(0.1)
#pragma property material_normal "Normal" Texture2D [normal]
#pragma property material_opacity "Opacity" float(1,0,1)

// Currently not used in shader
// TODO: change default value and add range
#pragma property material_index_of_refraction "Index of Refraction" float(1.5,1,3)
#pragma property material_glow "Glow" Texture2D(1, 1, 1, 1)
#pragma property material_glow_intensity "Glow Intensity" float(0,0,10)
#pragma property material_translucence "Translucence" Texture2D(0.0)
#pragma property material_interior_color "Interior Color" Color(1,1,1,1)

//Will read individual material properties, either from texture or from constant value
void read_material(
    in vec2 uv,
    out vec3 baseColor,
    out float metallic,
    out float roughness,
    out float opacity
){

    vec4 baseColor_a = material_base_color_texture_bound ?
        texture(material_base_color, uv) : material_base_color_default_value;

    baseColor = baseColor_a.xyz;

    metallic =  material_metallic_texture_bound ?
        texture(material_metallic, uv).x : material_metallic_default_value;

    roughness =  material_roughness_texture_bound ?
        texture(material_roughness, uv).x : material_roughness_default_value;

    opacity = baseColor_a.a * material_opacity;
}

// Will decide which color to use
// By default, the base color is unchanged
// If "has_color_attrib" is set, it will return fragment color
// If "uniform_color" is not negative, it will return uniform color
vec3 adjust_color(
    in bool has_color_attrib, in vec4 fragment_color, in vec4 uniform_color, vec3 baseColor){

    if(has_color_attrib){
        baseColor = fragment_color.xyz;
    }
    else if(uniform_color.x >= 0.0f){
        baseColor = uniform_color.xyz;
    }
    return baseColor;
}

// If normal texture is set, the normal will be adjusted
// If not, normal will be normalized and returned
vec3 adjust_normal(vec3 N, vec3 T, vec3 BT, vec2 uv){

    N = normalize(N);

    if(material_normal_texture_bound){

        T = normalize(T);
        BT = normalize(BT);

        mat3 TBN = mat3(T, BT, N);
        vec3 N_in_TBN = TBN * N;

        vec3 Ntex = texture(material_normal, uv).xyz * 2.0f - vec3(1.0f);
        vec3 Ntex_in_world = TBN * Ntex;

        N = normalize(Ntex_in_world);

    }

    return N;


}