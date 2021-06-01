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


layout (location = 0) in vec3 in_pos;
layout (location = 1) in vec4 in_value;
layout (location = 2) in vec3 in_normal;

out VARYING {   
    vec3 pos;
    vec3 normal;
    vec4 value;
    vec2 screen_pos;
} vs_out;

void main()
{       
    //Pos and normal to world space
    vs_out.pos = (M * vec4(in_pos, 1.0)).xyz; 
    vs_out.value = in_value;  
    vs_out.normal = (NMat * vec4(in_normal,0.0)).xyz;

    vec4 clip_space = PV * vec4(vs_out.pos,1.0);
    vs_out.screen_pos = screen_size * 0.5f * (clip_space.xy / clip_space.w);

    //To clip space
    gl_Position = PV * vec4(vs_out.pos,1.0);
}

#pragma GEOMETRY

layout(triangles) in;
layout(triangle_strip, max_vertices = 4) out;

in VARYING {
    vec3 pos;
    vec3 normal;
    vec4 value;
    vec2 screen_pos;
} gs_in[];


out VARYING_GEOM {
    vec3 pos;
    vec3 normal;
    vec4 value;
    vec2 screen_pos;
    vec3 edge_dist;
} gs_out;


void main() {

    for(int k = 0; k < 3; k++){
        vec2 u = gs_in[(k+1) % 3].screen_pos - gs_in[k].screen_pos;
        vec2 v = gs_in[(k+2) % 3].screen_pos - gs_in[k].screen_pos;
        float area_squared = abs(u.x*v.y - u.y*v.x);
        float h = area_squared / length(u-v);

        gs_out.value = gs_in[k].value;
        gs_out.normal = gs_in[k].normal;
        
        gs_out.pos = gs_in[k].pos;
        gl_Position = PV * vec4(gs_out.pos,1);

        h *= gl_Position.w;
        gs_out.edge_dist = vec3(0);
        gs_out.edge_dist[k] = h;
        
        EmitVertex();    
    }
    EndPrimitive();
}

#pragma FRAGMENT

layout(location = 0) out vec4 fragColor;

in VARYING_GEOM {
    vec3 pos;
    vec3 normal;
    vec4 value;
    vec2 screen_pos;
    vec3 edge_dist;
} fs_in;

#include "surface/colormap.frag"

#pragma property line_width "Line Width" float(1,0,100) 
#pragma property opacity "Opacity" float(1,0,1) 

void main(){


    float d = min(fs_in.edge_dist[0],min(fs_in.edge_dist[1],fs_in.edge_dist[2]));

    d = max(d - line_width, 0); //expand width    
    d *= gl_FragCoord.w; //perspective correction
    d = exp2(-2 * d * d); //edge function

    
    //fragColor = fs_in.value;
    fragColor = colormapped_fragment_color(fs_in.value);
    fragColor.a *= d * opacity;
    if(fragColor.a < 0.0001)
        discard;
}
