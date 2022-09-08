/*
 * Copyright 2019 Adobe. All rights reserved.
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
#include "layout/default_fragment_layout.glsl"

uniform samplerCube texCube;
uniform int sampleCount;
uniform float roughness;

#include "util/light.glsl"
#include "util/pbr.glsl"



//https://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
void main(){


    vec3 normal = normalize(vs_out_pos);
    vec3 lightIn = normal;
    vec3 lightOut = lightIn;

    float totalWeight = 0.0;
    vec3 color = vec3(0.0);
    for (int i=0; i < sampleCount; i++){

        vec2 sampleSpherical = hammersley2d(i,sampleCount);
        vec3 sampleDir = GGX_sample_dir(sampleSpherical, normal, roughness);
        vec3 lightDir = -reflect(lightOut, sampleDir);

        float cosLight = max(0.0,dot(normal, lightDir));
        if(cosLight <= 0.0)
            continue;
        color += texture(texCube, sampleDir).rgb * cosLight;
        totalWeight += cosLight;
    }

    fragColor.xyz = color / totalWeight;
    fragColor.a = 1.0;
}

