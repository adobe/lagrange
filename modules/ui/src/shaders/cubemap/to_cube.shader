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

uniform sampler2D texRectangular;

void main(){

    vec3 p = normalize(vs_out_pos);

    vec2 UV = vec2(atan(p.z,p.x),asin(p.y));
    UV *= vec2(0.1591,0.3183);
    UV += vec2(0.5);

    fragColor.xyz = texture(texRectangular, UV).rgb;

}
