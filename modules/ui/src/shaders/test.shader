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
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec4 in_tangent;
layout (location = 3) in vec4 in_bitangent;
layout (location = 4) in vec2 in_uv;

#ifdef ATTRIB_QUANTITY_1D
layout (location = 5) in float in_quantity;
#endif

#ifdef ATTRIB_QUANTITY_2D
layout (location = 5) in vec2 in_quantity;
#endif

#ifdef ATTRIB_QUANTITY_3D
layout (location = 5) in vec3 in_quantity;
#endif

#ifdef ATTRIB_QUANTITY_4D
layout (location = 5) in vec4 in_quantity;
#endif

#ifdef ATTRIB_QUANTITY_3x3
layout (location = 5) in vec3 in_quantity_0;
layout (location = 6) in vec3 in_quantity_1;
layout (location = 7) in vec3 in_quantity_2;
#endif


void main()
{       
    //Pos and normal to world space
    vs_out.pos = (M * vec4(in_pos, 1.0)).xyz; 
    vs_out.normal = (NMat * vec4(in_normal,0.0)).xyz;
    vs_out.tangent = (NMat * vec4(in_tangent)).xyz;;
    vs_out.bitangent = (NMat * vec4(in_bitangent)).xyz;; 
    vs_out.uv = in_uv;

    //To clip space
    gl_Position = PV * vec4(vs_out.pos,1.0);
}

out VARYING {
    vec3 pos;
    vec3 normal;
    vec2 uv;
    vec4 color;
    vec3 tangent;
    vec3 bitangent;
} vs_out;


#ifdef INDEXING_EDGE
#ifdef PRIMITIVE_TRIANGLE

#pragma GEOMETRY
#include "util/split_by_edge.geom"

#endif
#endif



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

