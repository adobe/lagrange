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
uniform sampler2D tex;
uniform vec4 outline_color;
uniform vec4 fill_color;
out vec4 fragColor;

void main(){

    
    vec2 coord = (vposition.xy + vec2(1)) * 0.5;
    ivec2 screen_size = textureSize(tex,0);

    vec2 dx = vec2(1.0 / screen_size.x, 0);
    vec2 dy = vec2(0, 1.0 / screen_size.y);
    

    float t = texture(tex,coord).x; 
    float tup = texture(tex,coord -dy).x;   
    float tdown = texture(tex,coord + dy ).x;
    float tleft = texture(tex,coord - dx).x;
    float tright = texture(tex,coord + dx).x;
    
    
    float val = 0;
    val = abs(tup - tdown) + abs(tdown - tup);
    val += abs(tleft - tright) + abs(tright - tleft);

    bool has_diff = (t != tup) || (t != tdown) || (t != tleft) || (t != tright);

    if(has_diff){
        fragColor = outline_color;
        if(!(t > 0)){
            fragColor.a = 0.5;
        }
    }
    else if(t > 0 && fill_color.a > 0){
        fragColor = fill_color;
    }
    else{
        discard;
    }

    

    
}
