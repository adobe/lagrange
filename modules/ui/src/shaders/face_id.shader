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

out VARYING_ID {
    vec3 pos;
    flat int vertex_id;
} vs_out_id;

void main()
{
    //Pos and normal to world space
    vs_out_id.pos = (M * vec4(in_pos, 1.0)).xyz;
    vs_out_id.vertex_id = gl_VertexID;

    //To clip space
    gl_Position = PV * vec4(vs_out_id.pos,1.0);
}

#pragma GEOMETRY

layout (triangles) in;
layout(triangle_strip, max_vertices = 4) out;


in VARYING_ID {
    vec3 pos;
    flat int vertex_id;
} vs_in_id[];

out VARYING_GEOM {
    vec3 bary_pos;
    flat int primitive_id;
    flat ivec3 vertex_ids;
} gs_out;

void main() {

    gs_out.vertex_ids = ivec3(
        vs_in_id[0].vertex_id,
        vs_in_id[1].vertex_id,
        vs_in_id[2].vertex_id
    );
    gs_out.primitive_id = gl_PrimitiveIDIn;

    gs_out.bary_pos = vec3(1,0,0);
    gl_Position = PV * vec4(vs_in_id[0].pos,1);
    EmitVertex();

    gs_out.bary_pos = vec3(0,1,0);
    gl_Position = PV * vec4(vs_in_id[1].pos,1);
    EmitVertex();

    gs_out.bary_pos = vec3(0,0,1);
    gl_Position = PV * vec4(vs_in_id[2].pos,1);
    EmitVertex();

    EndPrimitive();
}




#pragma FRAGMENT

layout(location = 0) out vec4 fragColor;

in VARYING_GEOM {
    vec3 bary_pos;
    flat int primitive_id;
    flat ivec3 vertex_ids;
} fs_in;


vec4 index_to_color(int i){
    int r = (i & 0x000000FF);
    int g = (i & 0x0000FF00) >> 8;
    int b = (i & 0x00FF0000) >> 16;
    return vec4(r / 255.0f, g / 255.0f, b / 255.0f, 1.0f);
}

#define ELEMENT_FACE 0
#define ELEMENT_EDGE 1
#define ELEMENT_VERTEX 2

#pragma property element_mode "ElementMode" int(0,0,2)
#pragma property debug_output "DebugOutput" bool(false)

void main(){


    vec3 b = fs_in.bary_pos;

    //Get closest vertex id color (largest bary coord)
    if(element_mode == ELEMENT_VERTEX){

        if(b.x > b.y){
            if(b.x > b.z){
                fragColor = index_to_color(fs_in.vertex_ids[0]);
            }
            else{
                fragColor = index_to_color(fs_in.vertex_ids[2]);
            }
        }
        else {
            if(b.y > b.z){
                fragColor = index_to_color(fs_in.vertex_ids[1]);
            }
            else{
                fragColor = index_to_color(fs_in.vertex_ids[2]);
            }
        }
        return;
    }
    //Get closest edge id color (smallest bary coord)
    //Indexed as 3*face_id + edge_id, where edge_id = 0,1,2
    else if(element_mode == ELEMENT_EDGE){

        int edge_id = 3 * fs_in.primitive_id;
        if(b.x < b.y){
            if(b.x < b.z){
                //fragColor = vec4(1,0,0,1);
                edge_id += 0;
                if(b.x > 0.1){
                    discard;
                }
            }
            else {
                //fragColor = vec4(0,0,1,1);
                edge_id += 2;
                if(b.z > 0.1){
                    discard;
                }
            }
        }
        else {
            if(b.y < b.z){
                //fragColor = vec4(0,1,0,1);
                edge_id += 1;
                if(b.y > 0.1){
                    discard;
                }
            }
            else{
                //fragColor = vec4(0,0,1,1);
                edge_id += 2;
                if(b.z > 0.1){
                    discard;
                }
            }
        }

        fragColor = index_to_color(edge_id) ;
    }
    else if(element_mode == ELEMENT_FACE){
        fragColor = index_to_color(fs_in.primitive_id);
    }

    if(debug_output){
        fragColor.xyz *= 5;

        vec2 d = vec2(
            length(dFdx(fragColor.xyz)),
            length(dFdy(fragColor.xyz))
        );
        fragColor.xyz *= 1.0 - vec3(max(d.x, d.y));
    }

    gl_FragDepth = gl_FragCoord.z;// + depth_offset;
}
