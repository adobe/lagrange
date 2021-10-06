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
layout (location = 1) in vec4 in_value;

out VARYING {
    vec3 pos;
    vec4 value;
} vs_out;

void main()
{
    //Pos and normal to world space
    vs_out.pos = (M * vec4(in_pos, 1.0)).xyz;
    vs_out.value = in_value;

    //To clip space
    gl_Position = PV * vec4(vs_out.pos,1.0);
}

#pragma FRAGMENT

layout(location = 0) out vec4 fragColor;

in VARYING {
    vec3 pos;
    vec4 value;
} fs_in;


#include "surface/colormap.frag"

#pragma property opacity "Opacity" float(1,0,1)

void main(){

    fragColor = colormapped_fragment_color(fs_in.value);
    fragColor.a *= opacity;
}
