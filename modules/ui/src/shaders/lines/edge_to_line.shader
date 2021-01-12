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
#include "util/vertex_layout.glsl"


out VARYING {
    vec3 pos;
    vec3 normal;
    vec2 uv;
    vec4 color;
    vec2 screen_pos;
} vs_out;

void main()
{       
    //Pos and normal to world space
    vs_out.pos = (M * vec4(in_pos, 1.0)).xyz; 
    vs_out.normal = (NMat * vec4(in_normal,0.0)).xyz;
    vs_out.uv = in_uv;
    vs_out.color = has_color_attrib ? in_color : uniform_color;   

    vec4 clip_space = PV * vec4(vs_out.pos,1.0);
    vs_out.screen_pos = screen_size * 0.5f * (clip_space.xy / clip_space.w);

    //To clip space
    gl_Position = clip_space;
}



#pragma GEOMETRY

layout(lines) in;
layout(triangle_strip, max_vertices = 4) out;

in VARYING {
    vec3 pos;
    vec3 normal;
    vec2 uv;
    vec4 color;
    vec2 screen_pos;
} gs_in[];

out VARYING_GEOM {
    vec3 pos;
    vec3 normal;
    vec2 uv;
    vec4 color; 
    float edge_dist;
} gs_out;


uniform float line_width = 3;

vec4 world_to_clip(vec4 v){
    return PV * vec4(v);
}

vec4 clip_to_world(vec4 v){
    return PVinv* vec4(v);
}

vec4 clip_to_ndc(vec4 v){
    v /= v.w;
    return v;
}

vec3 ndc_to_screen(vec4 v){
    const float zrange = 1;
    return vec3(screen_size, zrange) * ((v.xyz + vec3(1.0f)) * 0.5f);
}

vec3 screen_to_ndc(vec3 s){
    
    const float zrange = 1;
    return s  * vec3(1/screen_size.x, 1/screen_size.y, 1/zrange) * 2.0 - vec3(1.0);
}

vec4 ndc_to_clip(vec3 ndc, float w){
    return vec4(ndc, 1) * w;
}

vec4 screen_to_clip(vec3 screen, float w){
    return ndc_to_clip(screen_to_ndc(screen), w);

}

void main() {

    
    vec4 p1 = vec4(gs_in[0].pos.xyz,1);
    vec4 p2 = vec4(gs_in[1].pos.xyz,1);

    //To clip space
    vec4 p1c = world_to_clip(p1);
    vec4 p2c = world_to_clip(p2);

    //Project two edge points to screen
    vec3 p1s = ndc_to_screen(clip_to_ndc(p1c));
    vec3 p2s = ndc_to_screen(clip_to_ndc(p2c));


    vec2 ds = p2s.xy - p1s.xy;
    //Calculate normal direction and length in screen space
    vec3 ns = vec3(normalize(vec2(-ds.y, ds.x)) * line_width * 0.5f, 0);

    
    //World space normal
    vec3 nw = normalize(gs_in[0].normal);
    vec3 side = cross(nw, normalize(p2.xyz - p1.xyz)); // 'side' vector 

    //Quad in screen space back to clip space
    vec4 P[4];
    P[0] = screen_to_clip(p1s + ns, p1c.w);
    P[1] = screen_to_clip(p1s - ns, p1c.w);
    P[2] = screen_to_clip(p2s + ns, p2c.w);
    P[3] = screen_to_clip(p2s - ns, p2c.w);

    //How far should the quad extend
    float w1 = length(clip_to_world(P[0]) - clip_to_world(P[1])) * 0.5f;
    float w2 = length(clip_to_world(P[2]) - clip_to_world(P[3])) * 0.5f;

    //Calculate quad that's tangential to the edge
    P[0] = world_to_clip(vec4(p1.xyz + side*w1, 1));
    P[1] = world_to_clip(vec4(p1.xyz - side*w1, 1));
    P[2] = world_to_clip(vec4(p2.xyz + side*w2, 1));
    P[3] = world_to_clip(vec4(p2.xyz - side*w2, 1));

    //Interpolated distances
    float dist[4];
    dist[0] = 1;
    dist[1] = -1;
    dist[2] = 1;
    dist[3] = -1;
    
    gs_out.color = gs_in[0].color;

    gs_out.edge_dist = dist[0];
    gl_Position = P[0];
    EmitVertex();

    gs_out.edge_dist = dist[1];
    gl_Position = P[1];
    EmitVertex();

    gs_out.edge_dist = dist[2];
    gl_Position = P[2];
    EmitVertex();


    gs_out.edge_dist = dist[3];
    gl_Position = P[3];
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
    float edge_dist;
} fs_in;

void main(){

    float d = abs(fs_in.edge_dist);

    fragColor = vec4(d,0,0,1);

    //Edge function
    d = exp2(-2 * d * d);

    fragColor = fs_in.color;
    fragColor.a *= d;
    fragColor.a *= alpha_multiplier;

    if(fragColor.a < 0.0001){
        discard;
    }

    //Tiny offset is accetable for edges here when glPolygonOffset fails
    gl_FragDepth += 0.000001f;
    return;

    
}

