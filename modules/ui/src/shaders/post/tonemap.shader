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

in vec3 vposition;
uniform sampler2D color_tex;
uniform sampler2D depth_tex;

uniform float exposure = 1.0;
const float gamma = 2.2;
out vec4 fragColor;

void main(){

    vec2 coord = (vposition.xy + vec2(1)) * 0.5;
    vec3 hdr = texture(color_tex, coord).rgb;
    float depth = texture(depth_tex, coord).r;

    if(exposure > 0){
        vec3 ldr = vec3(1) - exp(-hdr * exposure);
        vec3 mapped = pow(ldr, vec3(1.0 / gamma));
        fragColor = vec4(mapped, 1);
    }
    else {
        vec3 mapped = pow(hdr, vec3(1.0 / gamma));
        fragColor = vec4(mapped, 1);
    }

    gl_FragDepth = depth;

}
