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
    vs_out.pos += normalize(cameraPos - in_pos);
    vs_out.pos = (M * vec4(in_pos, 1.0)).xyz; 
    vs_out.normal = (NMat * vec4(in_normal,0.0)).xyz;
    vs_out.uv = in_uv;
    vs_out.color = in_color;   

    vec4 clip_space = PV * vec4(vs_out.pos,1.0);
    vs_out.screen_pos = screen_size * 0.5f * (clip_space.xy / clip_space.w);

    //To clip space
    gl_Position = clip_space;
}

#pragma GEOMETRY

layout(points) in;
layout(triangle_strip, max_vertices = 12) out;

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
    vec4 dist;
    vec2 center;
} gs_out;


uniform float point_size = 5.0f;


void main() {
    
    //for(int k=0; k < 1; k++){
    vec4 clip = PV * vec4(gs_in[0].pos,1);
    //vec4 clip_max = PV * vec4(gs_in[0].pos + (cameraPos - gs_in[0].pos),1);
    
    
    vec2 screen = (clip.xy / clip.w) * screen_size;
    gs_out.center = screen;
    gs_out.color = gs_in[0].color;

    /*vec3 u = gs_in[0].pos - gs_in[1].pos;
    vec3 v = gs_in[0].pos - gs_in[2].pos;
    
    vec3 n = cross(u,v);*/
    vec3 n = gs_in[0].normal;
    float dir = dot(gs_in[0].pos - cameraPos,n);

    float sz = point_size;
    vec2 sa = screen + vec2(-sz,-sz);
    vec2 sb = screen + vec2(-sz,sz);
    vec2 sc = screen + vec2(sz,sz);
    vec2 sd = screen + vec2(sz,-sz);

    vec4 ca = vec4((sa / screen_size) * clip.w, clip.z, clip.w);
    vec4 cb = vec4((sb / screen_size) * clip.w, clip.z, clip.w);
    vec4 cc = vec4((sc / screen_size) * clip.w, clip.z, clip.w);
    vec4 cd = vec4((sd / screen_size) * clip.w, clip.z, clip.w);

    float szn = (point_size / screen_size.y) / (50*clip.w);
    float r = szn;
    ca.z -= r;
    cb.z -= r;
    cc.z -= r;
    cd.z -= r;

    //backface
    if(dir > 0){
        gl_Position = ca;
        EmitVertex();   

        gl_Position = cb;
        EmitVertex();   

        gl_Position = cd;
        EmitVertex();   

        gl_Position = cc;
        EmitVertex();   
    }
    //Frontface
    else{
        gl_Position = ca;
        EmitVertex();   

        gl_Position = cd;
        EmitVertex();   

        gl_Position = cb;
        EmitVertex();   

        gl_Position = cc;
        EmitVertex();   
    }

    EndPrimitive();
   // }
}

#pragma FRAGMENT

layout(location = 0) out vec4 fragColor;

in VARYING_GEOM {
    vec3 pos;
    vec3 normal;
    vec2 uv;
    vec4 color;
    vec4 dist;
    vec2 center;
} fs_in;


uniform float point_size = 5.0f;

void main(){
    vec2 center = (((fs_in.center / screen_size) + 1) / 2.0) * screen_size;

    float d = length(center - gl_FragCoord.xy);
    
    d = d / (0.4 * point_size); // ~0-1 radius
    float d2 = d*d;
    float d4= d2*d2;
    d = exp(-1 * d4*d4);

    if(has_color_attrib)
        fragColor = fs_in.color;
    else
        fragColor = uniform_color;

    fragColor.a = d;
    fragColor.a *= alpha_multiplier;
}
