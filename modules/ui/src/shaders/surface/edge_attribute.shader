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

out VARYING {
    vec3 pos;
    vec4 value;
} vs_out;

void main()
{
    //Pos and normal to world space
    vs_out.pos = (M * vec4(in_pos, 1.0)).xyz;
    vs_out.value = in_value;

    //To clip space
    gl_Position = PV * vec4(vs_out.pos,1.0);
}

#pragma GEOMETRY

layout(triangles) in;
layout(triangle_strip, max_vertices = 9) out;

in VARYING {
    vec3 pos;
    vec4 value;
} gs_in[];


out VARYING_GEOM {
    vec4 value;
} gs_out;


void emit(vec3 pos, vec4 value){
    gs_out.value = value;
    gl_Position = PV * vec4(pos,1);
    EmitVertex();
}

void emit(int k, int color_k){
    emit(gs_in[k].pos, gs_in[color_k].value);
}

void main(){


    vec3 center_pos = (gs_in[0].pos + gs_in[1].pos + gs_in[2].pos) / 3.0;
    vec4 center_value = (gs_in[0].value + gs_in[1].value + gs_in[2].value) / 3.0;

    emit(0,0);
    emit(1,0);
    emit(center_pos,center_value);
    EndPrimitive();

    emit(1,1);
    emit(2,1);
    emit(center_pos,center_value);
    EndPrimitive();

    emit(2,2);
    emit(0,2);
    emit(center_pos,center_value);
    EndPrimitive();

}

#pragma FRAGMENT

layout(location = 0) out vec4 fragColor;

in VARYING_GEOM {
    vec4 value;
} fs_in;


#include "surface/colormap.frag"

#pragma property opacity "Opacity" float(1,0,1)

void main(){

    fragColor = colormapped_fragment_color(fs_in.value);
    fragColor.a *= opacity;
}
