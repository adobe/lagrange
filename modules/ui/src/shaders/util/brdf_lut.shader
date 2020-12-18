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

#pragma VERTEX
in vec3 position;
out vec3 vposition;

void main() {
    gl_Position = vec4(position,1);
    vposition = position;   
}

#pragma FRAGMENT
layout(location = 0) out vec4 fragColor;

in vec3 vposition;
#include "util/light.glsl"
#include "util/pbr.glsl"



vec2 IntegrateBRDF(float NdotV, float roughness)
{
    vec3 V;
    V.x = sqrt(1.0 - NdotV*NdotV);
    V.y = 0.0;
    V.z = NdotV;

    float A = 0.0;
    float B = 0.0;

    vec3 N = vec3(0.0, 0.0, 1.0);

    const uint SAMPLE_COUNT = 128u;
    for(uint i = 0u; i < SAMPLE_COUNT; ++i)
    {
        vec2 Xi = hammersley2d(i, SAMPLE_COUNT);
        vec3 H  = GGX_sample_dir(Xi, N, roughness);
        vec3 L  = -reflect(V,H);

        float NdotL = max(L.z, 0.0);
        float NdotH = max(H.z, 0.0);
        float VdotH = max(dot(V, H), 0.0);
        float NdotV = max(dot(N, V), 0.0);  

        if(NdotL > 0.0)
        {               
            float G = G_Smith_GGX_Schlick(NdotV, NdotL, roughness);
            
            float G_Vis = (G * VdotH) / (NdotH * NdotV);            
            float Fc = pow(1.0 - VdotH, 5.0);

            A += (1.0 - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }
    
    A /= float(SAMPLE_COUNT);
    B /= float(SAMPLE_COUNT);    
    return vec2(A, B);
}

void main(){

    vec2 coord = (vposition.xy + vec2(1)) * 0.5; //to [0,1] range    
    fragColor.xy = IntegrateBRDF(coord.x,coord.y);    
    fragColor.z = 0;    
    fragColor.a = 1.0;

}

