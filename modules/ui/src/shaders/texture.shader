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

uniform sampler2D tex_2D;
uniform samplerCube tex_cube;
uniform int cubemap_face = 0;
uniform bool is_cubemap = false;

uniform float opacity = 1.0;
uniform float bias = 1.0;
uniform bool useTextureAlpha = false;

uniform bool useTextureLod = false;
uniform float textureLevel = 0.0;
uniform bool singleChannel = false;

uniform bool showOnlyAlpha = false;
uniform bool zeroIsTransparent = false;

out vec4 fragColor;

uniform bool normalize_values = false;
uniform float normalize_min = 0.0f;
uniform float normalize_range = 1.0f;

uniform bool is_depth = false;
uniform float depth_near;
uniform float depth_far;

float linearize_depth(float depth, float near, float far)
{
    return (2.0 * near) / (far + near - depth * (far - near));
}

void main(){

    
    vec2 coord = (vposition.xy + vec2(1)) * 0.5;

    vec4 t;
    if(useTextureLod){
        t = textureLod(tex_2D,coord,textureLevel);
    }
    else if(is_cubemap){
        vec3 cube_coord = vec3(0);
        switch(cubemap_face){
            case 0:
                cube_coord.yz = coord; cube_coord.x = 1.0f;
                break;
            case 1:
                cube_coord.yz = coord; cube_coord.x = -1.0f;
                break;
            case 2:
                cube_coord.xz = coord; cube_coord.y = 1.0f;
                break;
            case 3:
                cube_coord.xz = coord; cube_coord.y = -1.0f;
                break;
            case 4:
                cube_coord.xy = coord; cube_coord.z = 1.0f;
                break;
            case 5:
                cube_coord.xy = coord; cube_coord.z = -1.0f;
                break;
        }
        cube_coord = normalize(cube_coord);
        t = texture(tex_cube, cube_coord);  
    }
    else {
        t = texture(tex_2D,coord);  

    }



    fragColor = vec4(t.xyz*bias,opacity);

    if(is_depth){
        float d = linearize_depth(t.x, depth_near, depth_far);
        fragColor = vec4(vec3(d*bias),opacity);         
        return;
    }
    else if(singleChannel){
        
        if(zeroIsTransparent){
            fragColor = vec4(vec3(t.x,0,0),(t.x > 0) ? 0.4f : 0.0f);
        }
        else{
            fragColor = vec4(vec3(t.x*bias),opacity);   
        }       
    }


    if(useTextureAlpha)
        fragColor.a = t.a;

    if(showOnlyAlpha)
        fragColor = vec4(vec3(t.a*bias), opacity);

    if(normalize_values){
        fragColor.xyz = (fragColor.xyz - normalize_min) / normalize_range;
    }

    
}
