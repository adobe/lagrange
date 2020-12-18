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
uniform MaterialAdobe material;




//Will read individual material properties, either from texture or from constant value
void read_material(
    in vec2 uv, 
    out vec3 baseColor, 
    out float metallic, 
    out float roughness,
    out float opacity
){

    vec4 baseColor_a = material.baseColor.has_texture ? 
        texture(material.baseColor.texture, uv) : material.baseColor.value;    
    
    baseColor = baseColor_a.xyz;

    metallic =  material.metallic.has_texture ? 
        texture(material.metallic.texture, uv).x : material.metallic.value.x;

    roughness =  material.roughness.has_texture ? 
        texture(material.roughness.texture, uv).x : material.roughness.value.x;

    opacity = baseColor_a.a * material.opacity;
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
    
    if(material.normal.has_texture){

        T = normalize(T);
        BT = normalize(BT);

        mat3 TBN = mat3(T, BT, N);        
        vec3 N_in_TBN = TBN * N;

        vec3 Ntex = texture(material.normal.texture, uv).xyz * 2.0f - vec3(1.0f);
        vec3 Ntex_in_world = TBN * Ntex;

        N = normalize(Ntex_in_world);  

    }
    
    return N;
    

}