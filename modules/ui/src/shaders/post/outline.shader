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

in vec3 in_pos;
in vec4 in_color;
out vec3 vposition;
out vec4 vcolor;

#pragma property in_color "Color" Color(0,0,0,1) [attribute]

void main() {
    gl_Position = vec4(in_pos,1);
    vposition = in_pos;
    vcolor = in_color;
}


#pragma FRAGMENT

in vec3 vposition;
in vec4 vcolor;
out vec4 fragColor;

#pragma property color_tex "Color" Texture2D
#pragma property depth_tex "Depth" Texture2D

#pragma property dpos "Line Width" float(1,0,10)

float len2(vec3 x){
    return dot(x,x);
}

void main(){

    vec2 coord = (vposition.xy + vec2(1)) * 0.5;
    float depth = texture(depth_tex, coord).r;


    ivec2 screen_size = textureSize(color_tex,0);
    vec2 dx = vec2(1.0 / screen_size.x, 0) * dpos;
    vec2 dy = vec2(0, 1.0 / screen_size.y) * dpos;
    vec3 t = texture(color_tex,coord).rgb;
    vec3 tup = texture(color_tex,coord -dy).rgb;
    vec3 tdown = texture(color_tex,coord + dy ).rgb;
    vec3 tleft = texture(color_tex,coord - dx).rgb;
    vec3 tright = texture(color_tex,coord + dx).rgb;

    float d = 0.0001;
    bool has_diff = len2(t - tup) > d || len2(t - tdown) > d || len2(t - tleft) > d || len2(t - tright) > d;

    fragColor = vec4(0,0,0,0);

    if(has_diff){
        fragColor = vcolor;
        fragColor.a = 1;
    }
    else{
        discard;
    }

    gl_FragDepth = depth;

}
