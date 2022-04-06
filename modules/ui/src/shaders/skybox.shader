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

layout (location = 0) in vec3 in_pos;


out vec3 vs_out_pos;

uniform mat4 PV;


void main()
{
    vs_out_pos = in_pos;
    gl_Position = (PV * vec4(vs_out_pos,1.0)).xyww;
}

#pragma FRAGMENT
out vec4 fragColor;

in vec3 vs_out_pos;


uniform samplerCube texCubemap;
#pragma property mip_level "MipLevel" float(1,0,16)

void main(){

    vec3 p = normalize(vs_out_pos);

    fragColor = textureLod(texCubemap, p.xyz, mip_level);

}
