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
#include "util/light.glsl"

#define MAX_LIGHTS_SPOT 2
uniform SpotLight spot_lights[MAX_LIGHTS_SPOT];
uniform sampler2D spot_lights_shadow_maps_0;
uniform sampler2D spot_lights_shadow_maps_1;
uniform int spot_lights_count;

#define MAX_LIGHTS_POINT 1
uniform PointLight point_lights[MAX_LIGHTS_POINT];
uniform samplerCube point_lights_shadow_maps_0;
uniform int point_lights_count;

#define MAX_LIGHTS_DIRECTIONAL 2
uniform DirectionalLight directional_lights[MAX_LIGHTS_DIRECTIONAL];
uniform sampler2D directional_lights_shadow_maps_0;
uniform sampler2D directional_lights_shadow_maps_1;
uniform int directional_lights_count;

float get_shadow_at(vec3 surface_pos, SpotLight L, sampler2D shadow_map){
    return getShadowSquare(
        surface_pos,
        L.PV,
        shadow_map,
        L.shadow_near,
        L.shadow_far
    );
}

float get_shadow_at(vec3 surface_pos, DirectionalLight L, sampler2D shadow_map){
    return getShadowSquare(
        surface_pos,
        L.PV,
        shadow_map,
        L.shadow_near,
        L.shadow_far
    );
}

float get_shadow_at(vec3 surface_pos, PointLight L, samplerCube shadow_map){
    return getShadowCube(
        surface_pos,
        L.position,
        shadow_map,
        L.shadow_near,
        L.shadow_far
    );
}


LightAtSurface get_light_at_surface(
    SpotLight spot,
    sampler2D shadow_map,
    vec3 surface_pos,
    vec3 lightOut,
    float cos_out
    ){

    vec3 lightPos = spot.position;
    vec3 intensity = spot.intensity;
    float lightDistance = length(lightPos - surface_pos);

    LightAtSurface L;
    L.lightOut = lightOut;
    L.cos_out = cos_out;
    L.lightIn = normalize(lightPos - surface_pos);
    L.lightHalf = normalize(L.lightIn + L.lightOut);


    L.shadow_attenuation = get_shadow_at(surface_pos, spot, shadow_map);
    L.attenuation = max(1.0 / (lightDistance*lightDistance),1.0);
    //Apply spot factor
    L.attenuation *= calculate_spot_factor(
        lightPos, L.lightIn, spot.direction, spot.cone_angle
    );


    L.radiance = L.attenuation * L.shadow_attenuation * intensity;


    return L;
}



LightAtSurface get_light_at_surface(
    DirectionalLight dir,
    sampler2D shadow_map,
    vec3 surface_pos,
    vec3 lightOut,
    float cos_out
    ){


    vec3 intensity = dir.intensity;


    LightAtSurface L;
    L.lightOut = lightOut;
    L.cos_out = cos_out;
    L.lightIn = normalize(-dir.direction);
    L.lightHalf = normalize(L.lightIn + L.lightOut);

    L.attenuation = 1.0;
    L.shadow_attenuation = get_shadow_at(surface_pos, dir, shadow_map);
    L.radiance = L.shadow_attenuation * intensity;

    return L;
}


LightAtSurface get_light_at_surface(
    PointLight point,
    samplerCube shadow_map,
    vec3 surface_pos,
    vec3 lightOut,
    float cos_out
    ){

    vec3 lightPos = point.position;
    vec3 intensity = point.intensity;
    float lightDistance = length(lightPos - surface_pos);

    LightAtSurface L;
    L.lightOut = lightOut;
    L.cos_out = cos_out;
    L.lightIn = normalize(lightPos - surface_pos);
    L.lightHalf = normalize(L.lightIn + L.lightOut);

    L.shadow_attenuation = get_shadow_at(surface_pos, point, shadow_map);
    L.attenuation = max(1.0 / (lightDistance*lightDistance), 1.0);

    L.radiance = L.attenuation * L.shadow_attenuation * intensity;


    return L;
}
