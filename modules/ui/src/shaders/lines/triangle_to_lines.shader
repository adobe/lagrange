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

#include "layout/default_vertex_layout.glsl"

//Default color when attribute is not set
#pragma property in_color "Default Color" Color(0,0,0,0)


out vec2 screen_out;

void main()
{
    //Pos and normal to world space
    vs_out_pos = (M * vec4(in_pos, 1.0)).xyz;
    vs_out_normal = (NMat * vec4(in_normal,0.0)).xyz;
    vs_out_uv = in_uv;
    vs_out_color = in_color;

    vec4 clip_space = PV * vec4(vs_out_pos,1.0);
    screen_out = screen_size * 0.5f * (clip_space.xy / clip_space.w);

    //To clip space
    gl_Position = clip_space;
}

#pragma GEOMETRY

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

//in VARYING 
in vec3 vs_out_pos[3];
in vec3 vs_out_normal[3];
in vec2 vs_out_uv[3];
in vec4 vs_out_color[3];
in vec3 vs_out_tangent[3];
in vec3 vs_out_bitangent[3];

in vec2 screen_out[3];

// in VARYING {
//     vec3 pos;
//     vec3 normal;
//     vec2 uv;
//     vec4 color;
//     vec3 tangent;
//     vec3 bitangent;
// } gs_in[];

// in VARYING_SCREEN {
//     vec2 screen_pos;
// } gs_screen_in[];


out VARYING_GEOM {
    vec3 pos;
    vec3 normal;
    vec2 uv;
    vec4 color;
    vec3 edge_dist;
} gs_out;


void main() {

    for(int k = 0; k < 3; k++){
        vec2 u = screen_out[(k+1) % 3] - screen_out[k];
        vec2 v = screen_out[(k+2) % 3] - screen_out[k];
        float area_squared = abs(u.x*v.y - u.y*v.x);
        float h = area_squared / length(u-v);

        gs_out.color = vs_out_color[k];
        gs_out.normal = vs_out_normal[k];
        gs_out.color.a *= alpha_multiplier;

        gs_out.pos = vs_out_pos[k];
        gl_Position = PV * vec4(gs_out.pos,1);

        h *= gl_Position.w;
        gs_out.edge_dist = vec3(0,0,0);
        gs_out.edge_dist[k] = abs(h); // absolute value is needed when vertex is behind camera

        EmitVertex();
    }
    EndPrimitive();
}

#pragma FRAGMENT

layout(location = 0) out vec4 fragColor;

in VARYING_GEOM {
    vec3 pos;
    vec3 normal;
    vec2 uv;
    vec4 color;
    vec3 edge_dist;
} fs_in;

#pragma property line_width "Line Width" float(1,1,10)
#pragma property line_color "Line Color" Color(0,0,0,1)

void main(){
    float d = min(fs_in.edge_dist[0],min(fs_in.edge_dist[1],fs_in.edge_dist[2]));

    d *= gl_FragCoord.w; //perspective correction
    d = d - max(line_width - 1, 0) / 2;
    d = smoothstep(0, 1, d);

    fragColor = mix(line_color, fs_in.color, d);
    if(fragColor.a < 0.0001)
        discard;
}
