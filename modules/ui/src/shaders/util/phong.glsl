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


#include "lights.glsl"


vec3 phong(
    const in vec3 lightDir, 
    const in vec3 viewDir, 
    const in vec3 normal    
){

    
    float diffuseMagnitude = max(dot(normal,lightDir),0.0);
    float ambientMagnitude = 1;  
    
    vec3 mat_ambient =  material.ambient.has_texture ? texture(material.ambient.texture, fs_in.uv).xyz : material.ambient.value.xyz;
    vec3 mat_diffuse =  material.diffuse.has_texture ? texture(material.diffuse.texture, fs_in.uv).xyz : material.diffuse.value.xyz;
    vec3 mat_specular =  material.specular.has_texture ? texture(material.specular.texture, fs_in.uv).xyz : material.specular.value.xyz;

    mat_ambient = vec3(0.15);
    //Ambient   
    vec3 ambient = mat_ambient * ambientMagnitude;

    //Diffuse
    vec3 diffuse = clamp(mat_diffuse * diffuseMagnitude,0,1);   

    //Specular
    vec3 specular = vec3(0);    
    if (dot(normal, lightDir) >= 0.0){ 
        float specularMagnitude = 
            pow(
                max(0.0, dot(reflect(-lightDir, normal),viewDir)), 
                material.shininess
            );

        specular = mat_specular * specularMagnitude;
    }

    return ambient + (diffuse + specular);
}

float lightOmniAttenuation(vec3 pos, vec3 lightPos, float intensity, float attenuation){    
    float d = length(lightPos - pos);   
    //return 1.0 / (1.0 + )
    return intensity * clamp(1.0 - d / attenuation, 0.0,1.0);
}

float lightSpotAttenuation(vec3 pos, vec3 lightDir, vec3 spotDir, float angle,  float intensity){
    float coneAngle = degrees(acos(dot(-lightDir,spotDir)));
    if(coneAngle > angle){
        return 0;
    }
    else {
        float d = (coneAngle / angle);
        return (1.0 - d*d*d) * intensity;
    }   
}

