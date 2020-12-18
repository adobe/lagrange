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

out VARYING {
    vec3 pos;   
} vs_out;

uniform mat4 PV;


void main()
{       
    vs_out.pos = in_pos;
    gl_Position = (PV * vec4(vs_out.pos,1.0)).xyww;
}

#pragma FRAGMENT
layout(location = 0) out vec4 fragColor;

in VARYING {
    vec3 pos;
} fs_in;


uniform samplerCube texCubemap;
uniform float mip_level = 0.0f;

void main(){

    vec3 p = normalize(fs_in.pos);
     
    fragColor = textureLod(texCubemap, p.xyz, mip_level);    

    //gamma correction
    const float gamma = 2.2;
    fragColor.xyz = pow(fragColor.xyz, vec3(1.0 / gamma));
    
}
