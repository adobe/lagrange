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

layout (points) in;
layout (line_strip, max_vertices = 2) out;

in VARYING {
    vec3 pos;
    vec3 normal;
    vec2 uv;
    vec4 color;
    vec3 tangent;
    vec3 bitangent;
} gs_in[];

out VARYING_GEOM {
    vec3 pos_a;
    vec3 pos_b;
    vec3 n;
} gs_out;

uniform float line_length = 1.0f;

void main() {

    vec3 center = gs_in[0].pos;
    vec3 n = normalize(gs_in[0].normal);
    vec3 end = center + n * line_length;
    
    gs_out.pos_a = center;
    gs_out.pos_b = end;
    gs_out.n = n;
    
    gl_Position = PV * vec4(center,1);  
    EmitVertex();

    gl_Position = PV * vec4(end,1); 
    EmitVertex();

    EndPrimitive();
}

#pragma FRAGMENT

layout(location = 0) out vec4 fragColor;

in VARYING_GEOM {
    vec3 pos_a;
    vec3 pos_b;
    vec3 n;
} fs_in;

uniform vec4 color = vec4(0.92, 0.57, 0.2, 1.0);
uniform bool use_direction_color = false;

void main(){

    if(use_direction_color)
        fragColor = vec4(fs_in.n, color.a);
    else
        fragColor = color;  
}
