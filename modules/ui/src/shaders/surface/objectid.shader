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



vec4 index_to_color(int i){
    int r = (i & 0x000000FF);
    int g = (i & 0x0000FF00) >> 8;
    int b = (i & 0x00FF0000) >> 16;
    return vec4(r / 255.0f, g / 255.0f, b / 255.0f, 1.0f);
}




in VARYING {
    vec3 pos;
    vec3 normal;
    vec2 uv;
    vec4 color;
    vec3 tangent;
    vec3 bitangent;
} fs_in;

void main(){
    fragColor = index_to_color(object_id);
}
