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
layout(location = 0) out vec4 fragColor;

in VARYING {
    vec3 pos;
    vec3 normal;
    vec2 uv;
    vec4 color;
    vec3 tangent;
    vec3 bitangent;
} fs_in;

#include "util/pbr_shading.glsl"

void main(){
    
    //discard;
    fragColor = pbr(fs_in.pos, fs_in.normal, fs_in.uv, fs_in.color, fs_in.tangent, fs_in.bitangent);
    fragColor.a *= alpha_multiplier;

    
    
}