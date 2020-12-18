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

layout(triangles) in;
layout(triangle_strip, max_vertices = 9) out;

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
} gs_out;


void emit(vec3 pos, vec4 color, vec3 normal, vec2 uv, vec3 tangent, vec3 bitangent){
    gs_out.pos = pos;
    gs_out.color = color;
    gs_out.normal = normal;
    gs_out.uv = uv;
    gl_Position = PV * vec4(gs_out.pos,1);
    EmitVertex();
}

void emit(int k, int color_k){
    emit(gs_in[k].pos, gs_in[color_k].color,gs_in[k].normal,gs_in[k].uv,gs_in[k].tangent,gs_in[k].bitangent);
}

void main(){


    vec3 center_pos = (gs_in[0].pos + gs_in[1].pos + gs_in[2].pos) / 3.0;
    vec4 center_color = (gs_in[0].color + gs_in[1].color + gs_in[2].color) / 3.0;
    vec3 center_normal = (gs_in[0].normal + gs_in[1].normal + gs_in[2].normal) / 3.0;
    vec2 center_uv = (gs_in[0].uv + gs_in[1].uv + gs_in[2].uv) / 3.0;
    vec3 center_tangent = (gs_in[0].tangent + gs_in[1].tangent + gs_in[2].tangent) / 3.0;
    vec3 center_bitangent = (gs_in[0].bitangent + gs_in[1].bitangent + gs_in[2].bitangent) / 3.0;

    emit(0,0);
    emit(1,0);
    emit(center_pos,center_color,center_normal,center_uv,center_tangent, center_bitangent);
    EndPrimitive();

    emit(1,1);
    emit(2,1);
    emit(center_pos,center_color,center_normal,center_uv,center_tangent, center_bitangent);
    EndPrimitive();

    emit(2,2);
    emit(0,2);
    emit(center_pos,center_color,center_normal,center_uv,center_tangent, center_bitangent);
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
} fs_in;

void main(){
    if(has_color_attrib){
        fragColor = fs_in.color;
    }
    else
        fragColor = uniform_color;

    fragColor.a *= alpha_multiplier;
}
