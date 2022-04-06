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
#include "uniforms/ibl.glsl"
#include "uniforms/lights.glsl"
#include "uniforms/materials.glsl"


#include "util/pbr.glsl"


vec3 pbr_ibl_color(
    vec3 N,
    float cos_out,
    vec3 lightOut,
    vec3 f_0,
    float metallic,
    float roughness,
    vec3 baseColor)
{
    vec3 ambient_default = vec3(1.0);

    vec3 ibl_dir = N;
    vec3 specularDir = (2.0 * cos_out * N - lightOut);

    // Sample diffuse irradiance
    vec3 irradiance_diffuse = ambient_default;
    if (has_ibl) irradiance_diffuse = texture(ibl_diffuse, ibl_dir).xyz;

    // Sample specular irradiance
    vec3 irradiance_specular = ambient_default;
    if (has_ibl)
        irradiance_specular =
            textureLod(ibl_specular, specularDir, roughness * float(ibl_specular_levels)).rgb;


    // Diffuse irradiance
    vec3 F = F_schlick(f_0, cos_out);
    vec3 diffuse_fraction = mix(vec3(1.0) - F, vec3(0.0), metallic);
    vec3 diffuse = diffuse_fraction * baseColor * irradiance_diffuse;

    // Specular irradiance
    vec2 brdf = texture(ibl_brdf_lut, vec2(cos_out, roughness)).rg;
    vec3 specular = (f_0 * brdf.x + brdf.y) * irradiance_specular;

    // Final color
    vec3 color = diffuse + specular;
    return color;
}

// Assumes following files (and uniforms) are included
// uniforms/lights
// uniforms/materials
// uniforms/ibl
vec4 pbr(vec3 in_pos, vec3 in_normal, vec2 in_uv, vec4 in_color, vec3 in_tangent, vec3 in_bitangent)
{
    vec3 baseColor;
    float metallic, roughness, opacity;
    read_material(in_uv, baseColor, metallic, roughness, opacity);

    vec3 N = adjust_normal(in_normal, in_tangent, in_bitangent, in_uv);

    vec3 f_0 = get_f0(baseColor, metallic);

    vec3 color = vec3(0);

    vec3 lightOut = normalize(cameraPos - in_pos);
    float cos_out = max(0.0, dot(lightOut, N)); // n dot v

    // MAX_LIGHTS_SPOT == 2
    if (spot_lights_count >= 1)
        color += pbr_color(
            get_light_at_surface(
                spot_lights[0],
                spot_lights_shadow_maps_0,
                in_pos,
                lightOut,
                cos_out),
            f_0,
            N,
            roughness,
            metallic,
            baseColor);
    if (spot_lights_count >= 2)
        color += pbr_color(
            get_light_at_surface(
                spot_lights[1],
                spot_lights_shadow_maps_1,
                in_pos,
                lightOut,
                cos_out),
            f_0,
            N,
            roughness,
            metallic,
            baseColor);

    // MAX_LIGHTS_DIRECTIONAL == 2
    if (directional_lights_count >= 1)
        color += pbr_color(
            get_light_at_surface(
                directional_lights[0],
                directional_lights_shadow_maps_0,
                in_pos,
                lightOut,
                cos_out),
            f_0,
            N,
            roughness,
            metallic,
            baseColor);

    if (directional_lights_count >= 2)
        color += pbr_color(
            get_light_at_surface(
                directional_lights[1],
                directional_lights_shadow_maps_1,
                in_pos,
                lightOut,
                cos_out),
            f_0,
            N,
            roughness,
            metallic,
            baseColor);

    // MAX_LIGHTS_POINT == 1
    /*if (point_lights_count >= 1)
        color += pbr_color(
            get_light_at_surface(
                point_lights[0],
                point_lights_shadow_maps_0,
                in_pos,
                lightOut,
                cos_out),
            f_0,
            N,
            roughness,
            metallic,
            baseColor);*/



    color += pbr_ibl_color(N, cos_out, lightOut, f_0, metallic, roughness, baseColor);

    return vec4(color, opacity);
}