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
out vec4 fragColor;

uniform sampler2D color_tex;
uniform sampler2D depth_tex;
uniform float alpha_multiplier;


uniform vec4 uniform_color = vec4(1,1,1,1);

void main(){

    vec2 coord = (vposition.xy + vec2(1)) * 0.5;
    vec3 color = texture(color_tex, coord).rgb;
    float depth = texture(depth_tex, coord).r;

    ivec2 screen_size = textureSize(color_tex,0);
    vec2 dx = vec2(1.0 / screen_size.x, 0);
    vec2 dy = vec2(0, 1.0 / screen_size.y);
    vec3 t = texture(color_tex,coord).rgb;  
    vec3 tup = texture(color_tex,coord -dy).rgb;    
    vec3 tdown = texture(color_tex,coord + dy ).rgb;
    vec3 tleft = texture(color_tex,coord - dx).rgb;
    vec3 tright = texture(color_tex,coord + dx).rgb;

    bool has_diff = (t != tup) || (t != tdown) || (t != tleft) || (t != tright);
    
    fragColor = vec4(0,0,0,0);
    

    if(has_diff){
        fragColor = uniform_color;
        fragColor.a *= alpha_multiplier;
    }
    else{
        discard;
    }
    
    gl_FragDepth = depth;

}
