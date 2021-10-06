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

uniform samplerCube texCube;

#define M_PI 3.1415926535897932384626433832795

void main(){



    vec3 irradiance = vec3(0);

    vec3 basis_Z = normalize(fs_in.pos);
    vec3 basis_X = cross(vec3(0,1,0), basis_Z);
    vec3 basis_Y = cross(basis_Z, basis_X);

    float angleStep = 0.025f;
    int sample_num = 0;
    for(float alpha = 0; alpha < 2 * M_PI; alpha += angleStep){
        for(float beta = 0; beta < M_PI / 2; beta += angleStep){


            vec3 cartesian = vec3(sin(beta)*cos(alpha), sin(beta)*sin(alpha), cos(beta));
            vec3 sampleDir = cartesian.x * basis_X + cartesian.y * basis_Y + cartesian.z * basis_Z;
            float weight = cos(beta) * sin(beta);

            irradiance += texture(texCube, sampleDir).rgb * weight;
            sample_num++;
        }
    }

    fragColor.xyz = M_PI * irradiance * (1.0 / sample_num);
    fragColor.a = 1.0;

}

