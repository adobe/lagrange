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
#define LIGHT_TYPE_SPOT 0
#define LIGHT_TYPE_POINT 1
#define LIGHT_TYPE_DIRECTIONAL 2

uniform float shadow_square_samples = 8;
uniform float shadow_cube_samples = 4.0;

struct SpotLight {
    vec3 position;
    vec3 intensity;
    vec3 direction;
    float cone_angle;
    mat4 PV;
    sampler2D shadow_map;
    float shadow_near;
    float shadow_far;
};


struct PointLight {
    vec3 position;
    vec3 intensity;
    samplerCube shadow_map;
    float shadow_near;
    float shadow_far;
};

struct DirectionalLight {
    vec3 direction;
    vec3 intensity;
    vec3 sampling_pos;
    mat4 PV;
    sampler2D shadow_map;
    float shadow_near;
    float shadow_far;
};

// Contains all information about light hitting a surface:
// light directions (out, in, half), radiance (attenuated and shadowed)
struct LightAtSurface {
    vec3 lightOut;
    vec3 lightIn;
    vec3 lightHalf;
    float cos_out;
    vec3 radiance;
};


float calculate_spot_factor(vec3 pos, vec3 lightDir, vec3 spotDir, float angle){
    float coneAngle = acos(dot(-lightDir,spotDir));
    if(coneAngle > angle){
        return 0.0;
    }
    else {
        float d = (coneAngle / angle);
        return (1.0 - d*d*d);
    }   
    return 0.0;
}


float recoverShadowDepth(float depth, float near, float far){
    return (depth * (far - near)) - near;
}

float getShadowSquare(vec3 pos, mat4 PV, sampler2D shadowMap, float near, float far){
    
    //Project to light space
    vec4 lightSpace = PV * (vec4(pos,1));

    //Clip space [-1,1]
    vec3 coords = lightSpace.xyz / lightSpace.w; 

    //NDC space [0,1]
    coords = coords * 0.5 + 0.5; 

    //Clamp light space z position
    coords.z = min(1, coords.z);
    coords.z = max(0, coords.z);

    float currentDepth = coords.z;    

    float bias = 0.000; 
    float shadow = 0.0;
    
    float samples = shadow_square_samples;
    vec2 texelSize = 2.0 / textureSize(shadowMap,0);
    for(float x = -texelSize.x; x < texelSize.x; x += texelSize.x / (samples * 0.5))
    {
        for(float y = -texelSize.y; y < texelSize.y; y += texelSize.y / (samples * 0.5))
        {          
                float closestDepth = texture(shadowMap,coords.xy + vec2(x,y)).x;                
                if(currentDepth - bias > closestDepth)
                    shadow += 1.0;
        }
    }
    shadow /= (samples * samples);

    return 1.0 - shadow;
}

float getShadowCube(
    vec3 pos, 
    vec3 lightPos, 
    samplerCube shadowMap,
    float near, 
    float far){
    

    vec3 toL = pos - lightPos;
    float curD = length(toL);
    
    
    float shadow = 0.0;
    float bias = 0.01; 
    float samples = shadow_cube_samples;

    float offset = 0.2 * curD / (far - near);
    float step = offset / (shadow_cube_samples * 0.5);
    int total_samples = 0;

    for(float x = -offset; x <= offset; x += step)
    {
        for(float y = -offset; y <= offset; y += step)
        {
            for(float z = -offset; z <= offset; z += step)
            {
                float map_value = texture(shadowMap, toL + vec3(x, y, z)).x;
                total_samples++;

                //Surfaces beyond far value should be lit
                if(map_value >= 1){
                    continue;
                }

                float closestDepth = map_value * far;
                if(curD - bias > closestDepth)
                    shadow += 1.0;                     
            }
        }
    }

    shadow /= total_samples;;
    
    return 1.0f - shadow;
}
