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

layout (triangles) in;
layout (triangle_strip, max_vertices=18) out;

uniform mat4 shadowPV[6];

out vec4 vposition;

void main(){


    for(int f = 0; f < 6; f++){

        gl_Layer = f;
        for(int i=0; i < 3; i++){
            vposition = gl_in[i].gl_Position;
            gl_Position = shadowPV[f] * vposition;
            EmitVertex();
        }
        EndPrimitive();

    }

}


#pragma FRAGMENT

in vec4 vposition;

uniform vec3 originPos;
uniform float far;
uniform float near;

void main(){
    //Distance to object
    float d = length(originPos - vposition.xyz);

    //Normalize depth
    gl_FragDepth = (d - near) / (far-near);

}
