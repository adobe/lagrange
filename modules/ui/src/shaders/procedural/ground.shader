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

#include "uniforms/common.glsl"



//Takes default vertex data
#pragma VERTEX
#include "util/vertex_layout.glsl"
//assumes "uniforms_common" has been included
out VARYING {
    vec3 pos;
    vec3 normal;
    vec2 uv;
    vec4 color;
    vec3 tangent;
    vec3 bitangent;
} vs_out;


void main()
{       
    
    vs_out.pos = vec3(in_pos);
    vs_out.normal = in_normal;
    vs_out.uv = in_uv;
    vs_out.color = in_color;  
    vs_out.tangent = vec3(in_tangent);
    vs_out.bitangent = vec3(in_bitangent); 
    gl_Position = vec4(vs_out.pos,1);
}


#pragma FRAGMENT

layout(location = 0) out vec4 fragColor;

in VARYING {
    vec3 pos;
    vec3 normal;
    vec2 uv;
    vec4 color;
    vec3 tangent;
    vec3 bitangent;
} fs_in;



#include "util/pbr_shading.glsl"

uniform vec2 screen_size_inv;
uniform mat4 Pinv;
uniform mat4 Vinv;



uniform bool show_axes = false;
uniform bool render_pbr = true;
uniform bool shadow_catcher = false;
uniform bool show_grid = false;

uniform float period_primary = 0.1f;
uniform float period_secondary = 0.1f;
uniform vec4 grid_color = vec4(0,0,0,0.058);

uniform float shadow_catcher_alpha = 0.05f;
uniform float ground_height = 0;

float compute_line(vec2 uv, float period, float width){
    vec2 coord = 1.0f * mod(uv, period) / period;
    vec2 grid = abs(fract(coord - 0.5) - 0.5);
    grid = grid / fwidth(coord * width);
    float line = 1 - min(grid.x, grid.y);

    line = max(line,0);
    return line;
}

vec2 compute_axes(vec2 uv, float period, float width){    
    vec2 coord = abs(uv) / period;
    vec2 grid = coord;
    grid = grid / fwidth(coord * width);

    float lx = coord.x > width ? 0 : (1 - grid.x);
    float ly = coord.y > width ? 0 : (1 - grid.y);;
    lx = max(lx, 0);
    ly = max(ly, 0);

    return vec2(lx, ly);
}


//Expects cameraPos uniform
float isect_plane(in vec3 ray, out vec3 pos, out vec3 N){

    vec3 planeN = vec3(0,1,0);
    float planeD = -ground_height;
    float planeDenom = dot(planeN, ray);

     float t = 0;
    if(abs(planeDenom) > 0.0){
        t = -(dot(planeN, cameraPos) + planeD) / planeDenom;

        pos = cameraPos + t * ray;
        N = planeN;
    }
    else {
        return -1.0;
    }

    return t;
}

vec3 calc_ray(in vec2 screen){
    vec4 rayNorm = vec4(screen * 2 - vec2(1), -1,1);
    vec4 rayEye = Pinv * rayNorm;
    rayEye.z = -1;
    rayEye.w = 0;
    return normalize(vec3(Vinv * rayEye));
}


vec4 color_at(vec3 pos){

    vec4 color = vec4(grid_color);
    color.a = 0;

    vec2 uv = pos.xz;

    if(show_grid){        
        float l0 = compute_line(uv, period_primary, 3);
        float l1 = compute_line(uv, period_secondary, 1);

        float line = max(l0,l1);

        color = grid_color;
        color.a *= line;
    }

    if(show_axes){
        vec2 ax = compute_axes(uv,period_primary,2);
        
        color.xyz = mix(color.xyz, vec3(1,0,0), ax.x);
        color.a = mix(color.a, ax.x, ax.x);

        color.xyz = mix(color.xyz, vec3(0,0,1), ax.y);
        color.a = mix(color.a, ax.y, ax.y);
    }

    return color;
}


vec4 sample_between(vec3 pos, vec3 posX, vec3 posY, int maxSamples){
    
    vec4 color = vec4(0);
    vec3 dx = posX - pos;
    vec3 dy = posY - pos;

    for(int i=0; i < maxSamples; i++){
        float a = float(i) / float(maxSamples);
        for(int j=0; j < maxSamples; j++){
            float b = float(j) / float(maxSamples);
            color += color_at(pos + dx * a + dy * b);
        }
    }
    return color / (maxSamples * maxSamples);

}

float recover_depth(vec3 pos){
    vec4 pos_clip = (PV * vec4(pos, 1));

    float near = 0.0f;
    float far = 1.0f;
    
    float clip_depth = pos_clip.z / pos_clip.w;
    float depth_ndc = (1.0 - 0.0) * 0.5 * clip_depth + (1.0 + 0.0) * 0.5;    
    return depth_ndc;

}

void main(){

    vec2 xy = gl_FragCoord.xy * screen_size_inv;
    
    //Get primary ray
    vec3 rayWorld = calc_ray(xy);

    vec3 pos;
    vec3 N;
    
    float t = isect_plane(rayWorld, pos, N);
    if(t <= 0)
        discard;

    //Get rays for position occupancy
    vec3 rayX = calc_ray(xy + vec2(1,0) * screen_size_inv);
    vec3 rayY = calc_ray(xy + vec2(0,1) * screen_size_inv);

    //Get position of other rays based on normal
    vec3 posX = cameraPos - rayX*dot( cameraPos-pos, N )/dot( rayX, N );
    vec3 posY = cameraPos - rayY*dot( cameraPos-pos, N )/dot( rayY, N );

    //Sample between positions
    vec4 color = sample_between(pos,posX,posY,10);

    

    if(shadow_catcher){
        
        float attenuation = 1.0;
        for(int i = 0; i < spot_lights_count; i++){
            attenuation *= get_shadow_at(pos, spot_lights[i]);
        }   

        for(int i = 0; i < directional_lights_count; i++){
            attenuation *= get_shadow_at(pos, directional_lights[i]);
        }

        for(int i = 0; i < point_lights_count; i++){
            attenuation *= get_shadow_at(pos, point_lights[i]);
        }

        float shadow = 1 - attenuation;        
        shadow *= shadow_catcher_alpha;        
        vec4 shadow_color = vec4(vec3(1 - shadow), 1);
        color = vec4(mix(shadow_color.xyz, color.xyz, color.a), 1);
    }
    else if(render_pbr){
        vec4 pbr_color = pbr(pos, N, pos.xz, vec4(1,1,1,1), vec3(pos.x,0,0), vec3(0,0,pos.z));
        //Blend with grid
        color = vec4(mix(pbr_color.xyz, color.xyz, color.a), 1);
    }
    //Only grid
    else {
        if(color.a < 0.0001)
        discard;
    }

    fragColor = vec4(color);    
    gl_FragDepth = recover_depth(pos);

}

