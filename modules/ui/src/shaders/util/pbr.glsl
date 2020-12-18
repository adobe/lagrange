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
#define M_PI 3.1415926535897932384626433832795

float radicalInverse_VdC(uint bits) {
     bits = (bits << 16u) | (bits >> 16u);
     bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
     bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
     bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
     bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
     return float(bits) * 2.3283064365386963e-10; // / 0x100000000
 }

vec2 hammersley2d(uint i, uint N) {
     return vec2(float(i)/float(N), radicalInverse_VdC(i));
}


float G_Schlick(float cos_, float k){
    return cos_ / (cos_ * (1.0 - k) + k);
}

float G_Smith_GGX_Schlick(float cos_in_N, float cos_out_N, float roughness){    
    float alpha = roughness*roughness;
    float k = alpha / 2.0;
    //float alpha = roughness + 1.0; //alternative version
    //float k = (alpha*alpha) / 8.0;
    return G_Schlick(cos_in_N, k) * G_Schlick(cos_out_N, k);
}


vec3 GGX_sample_dir(vec2 Xi, vec3 N, float roughness)
{
    float a = roughness*roughness;

    float phi = 2.0 * M_PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a*a - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta*cosTheta);

    // from spherical coordinates to cartesian coordinates
    vec3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;

    // from tangent-space vector to world-space sample vector
    vec3 up = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(up, N));
    vec3 bitangent = cross(N, tangent);

    vec3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
    return normalize(sampleVec);
}


//
vec3 F_schlick(vec3 f_0, float cos_l_h){
    //float cos_l_h = max(0.0,dot(l,h));
    cos_l_h = min(cos_l_h,1.0);
    return f_0 + (vec3(1.0) - f_0) * pow(1.0 - cos_l_h, 5.0);
    
}


float D_GGX_Trowbridge(float cos_h_N, float roughness){
    //alpha = roughness^2 (per Disney, UE4)
    float alpha = roughness*roughness;
    float alpha2 = alpha*alpha;
    //float cos_h_N = max(0,dot(h,N));

    float denom = ((cos_h_N*cos_h_N) * (alpha2 - 1.0) + 1.0);
    return alpha2 / (
        M_PI * denom * denom
    );

}


vec3 get_f0(in vec3 baseColor, in float metallic){
    const vec3 f_0_dielectric = vec3(0.08f); //adobe's 0.08
    return mix(f_0_dielectric, baseColor, metallic);
}

vec3 pbr_color(
    in LightAtSurface L, 
    in vec3 f_0,
    in vec3 N,
    in float roughness,
    in float metallic,
    in vec3 baseColor
){
        
    float cos_in = max(0.0, dot(L.lightIn,N)); //n dot l        
    float cos_half = max(0.0, dot(L.lightHalf,N));
    float cos_diff = max(0.0, dot(L.lightHalf,L.lightIn));
    float cos_diff2 = max(0.0, dot(L.lightHalf,L.lightOut));

    //Fresnel reflectance (directly reflected light)
    vec3 F = F_schlick(f_0, cos_diff);

    //Distribution of microfacet normals
    float D = D_GGX_Trowbridge(cos_half, roughness);

    //Geometry Shadowing/Masking function
    float G = G_Smith_GGX_Schlick(cos_in, L.cos_out, roughness);
    
    //Denominator 
    float denom = max(4 * cos_in * L.cos_out,0.00001);

    //Final specular direct light component
    vec3 specular = (F*D*G) / denom;
    specular = max(specular,vec3(0));

    //Fraction of diffuse is based on metallic and fresnel (total reflection) values
    vec3 diffuse_fraction = mix(vec3(1.0) - F, vec3(0.0), metallic);

    vec3 diffuse = diffuse_fraction * baseColor;

    vec3 color = (diffuse + specular) * cos_in  * L.radiance;
    return color;

}

vec3 gamma_correction(vec3 color){
    const float gamma = 2.2;
    return pow(color, vec3(1.0 / gamma));
}