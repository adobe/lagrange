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

#pragma VERTEX
#include "util/default.vertex"


#pragma GEOMETRY

layout(lines) in;
layout(triangle_strip, max_vertices = 4) out;

in VARYING {
    vec3 pos;
    vec3 normal;
    vec2 uv;
    vec4 color;
    vec3 tangent;
    vec3 bitangent;
} gs_in[];

out VARYING_GEOM {
    vec3 pos;
    vec3 normal;
    vec2 uv;
    vec4 color;
    vec3 tangent;
    vec3 bitangent;
    float line_width;
    float t;
} gs_out;

uniform float line_width = 0.0015f;
uniform bool flip_lines = false;
uniform bool is_perspective = true;
uniform float ortho_scale = 1.0f;

float get_distance_factor(vec3 p){
    if(is_perspective)
        return length(p-cameraPos);
    return ortho_scale;
}

vec3 GetPosition(vec3 p, vec3 n, vec3 s) {    
    float w = line_width * get_distance_factor(p);
    return p + s * w ; 
    
}

void main() {
    vec3 p1 = gs_in[0].pos.xyz;
    vec3 p2 = gs_in[1].pos.xyz;

    vec3 v = normalize(p2 - p1);        // vector along the line    
    vec3 n = gs_in[0].normal;
    //vec3 n = normalize(cameraPos - (p1+p2)*0.5);
    vec3 s = flip_lines ? n : cross(n, v); // 'side' vector    

    //Use color of the provoking vertex
    vec4 color = gs_in[0].color;
    
    gs_out.color =  color;
    gs_out.normal =  gs_in[0].normal;
    gs_out.uv =  gs_in[0].uv;
    gs_out.tangent =  gs_in[0].tangent;
    gs_out.bitangent =  gs_in[0].bitangent;
    gs_out.t =  1;
    gs_out.pos = GetPosition(p1, n, +s);
    gs_out.line_width = line_width * length(p1 - cameraPos);
    gl_Position = PV * vec4(gs_out.pos,1);  
    EmitVertex();
    gs_out.t =  -1;
    gs_out.pos = GetPosition(p1, n, -s);    
    gl_Position = PV * vec4(gs_out.pos,1);
    EmitVertex();

    gs_out.color = color;
    gs_out.normal =  gs_in[1].normal;
    gs_out.uv =  gs_in[1].uv;
    gs_out.tangent =  gs_in[1].tangent;
    gs_out.bitangent =  gs_in[1].bitangent;
    gs_out.t =  1;
    gs_out.pos = GetPosition(p2, n, +s);
    gs_out.line_width = line_width * length(p2 - cameraPos);
    gl_Position = PV * vec4(gs_out.pos,1);
    EmitVertex();
    gs_out.t =  -1;
    gs_out.pos = GetPosition(p2, n, -s);
    gl_Position = PV * vec4(gs_out.pos,1);
    EmitVertex();    
    EndPrimitive();
    
}


#pragma FRAGMENT



layout(location = 0) out vec4 fragColor;

in VARYING_GEOM {
    vec3 pos;
    vec3 normal;
    vec2 uv;
    vec4 color;
    vec3 tangent;
    vec3 bitangent;
    float line_width;
    float t;
} fs_in;



void main(){

    fragColor = has_color_attrib ? fs_in.color : uniform_color;
    
    
    float edge_dist = 1 - abs(fs_in.t);
    float grad = fwidth(edge_dist);
    float a = smoothstep(0.0, grad, edge_dist);
    fragColor.a *= a * alpha_multiplier;
    
    
}
