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

#include "layout/default_fragment_layout.glsl"
#include "uniforms/lights.glsl"

#pragma property material_base_color "Base Color" Texture2D(0.7,0.7,0.7,1)
#pragma property material_normal "Normal" Texture2D [normal]



uniform bool has_ibl; // System property
uniform samplerCube ibl_diffuse;

void main(){

    vec4 baseColor_ = material_base_color_default_value;
    if(material_base_color_texture_bound)
        baseColor_ = texture(material_base_color, vs_out_uv);


    vec3 baseColor = baseColor_.xyz;

    fragColor = vec4(0,0,0,0);

    vec3 N = normalize(vs_out_normal);


    vec3 ambient_default = vec3(1.0);
    vec3 ibl_dir = N;
    vec3 irradiance_diffuse = ambient_default;
    if(has_ibl)
        irradiance_diffuse = texture(ibl_diffuse, ibl_dir).xyz;



    //Perturb normal in tangent space
    if(material_normal_texture_bound){
        vec3 T = normalize(vs_out_tangent);
        vec3 BT = normalize(vs_out_bitangent);
        mat3 TBN = mat3(T, BT, N);
        vec3 N_in_TBN = TBN * N;

        vec3 Ntex = texture(material_normal, vs_out_uv).xyz * 2.0f - vec3(1.0f);
        vec3 Ntex_in_world = TBN * Ntex;

        N = normalize(Ntex_in_world);
    }



    vec3 color = vec3(0);

    vec3 lightOut = normalize(cameraPos - vs_out_pos);
    float cos_out = max(0.0, dot(lightOut,N)); //n dot v




    for(int light_type = 0; light_type < 3; light_type++){

        int light_count = 0;
        switch(light_type){
            case 0: light_count = spot_lights_count; break;
            case 1: light_count = point_lights_count;break;
            case 2: light_count = directional_lights_count; break;
        };

        for(int i=0; i < light_count; i++){

            vec3 lightPos = vec3(0);
            vec3 intensity = vec3(0);
            vec3 lightIn = vec3(0);

            switch(light_type){
                case LIGHT_TYPE_SPOT:
                    lightPos = spot_lights[i].position;
                    lightIn = normalize(lightPos - vs_out_pos);
                    intensity = spot_lights[i].intensity;
                    break;
                case LIGHT_TYPE_POINT:
                    lightPos = point_lights[i].position;
                    lightIn = normalize(lightPos - vs_out_pos);
                    intensity = point_lights[i].intensity;
                    break;
                case LIGHT_TYPE_DIRECTIONAL:
                    lightIn = normalize(-directional_lights[i].direction);
                    intensity = directional_lights[i].intensity;
                    break;
            };


            vec3 lightHalf = normalize(lightIn + lightOut);
            float lightDistance = length(lightPos - vs_out_pos);

            float attenuation = 1.0f;
            float shadow_attenuation = 1.0f;

            //Compute attenuation + shadow
            /*if(light_type == LIGHT_TYPE_SPOT){
                shadow_attenuation = getShadowSquare(
                    vs_out_pos,
                    spot_lights[i].PV,
                    spot_lights[i].shadow_map,
                    spot_lights[i].shadow_near,
                    spot_lights[i].shadow_far
                );

                attenuation = 1.0 / (lightDistance*lightDistance);

                //Apply spot factor
                attenuation *= calculate_spot_factor(
                    lightPos, lightIn, spot_lights[i].direction, spot_lights[i].cone_angle
                );
            }
            else if(light_type == LIGHT_TYPE_DIRECTIONAL){
                shadow_attenuation = getShadowSquare(
                    vs_out_pos,
                    directional_lights[i].PV,
                    directional_lights[i].shadow_map,
                    directional_lights[i].shadow_near,
                    directional_lights[i].shadow_far
                );
            }
            else if(light_type == LIGHT_TYPE_POINT){
                shadow_attenuation = getShadowCube(
                    vs_out_pos,
                    lightPos,
                    point_lights[i].shadow_map,
                    point_lights[i].shadow_near,
                    point_lights[i].shadow_far
                );
                attenuation = 1.0 / (lightDistance*lightDistance);
            }*/

            vec3 radiance = attenuation * shadow_attenuation * intensity;

            float cos_in = max(0.0, dot(lightIn,N)); //n dot l

            vec3 reflect_dir = reflect(-lightIn,N);

            float spec = pow(max(dot(lightOut,reflect_dir),0),32);
            vec3 specular = vec3(spec);
            vec3 diffuse = cos_in * baseColor;
            color += (diffuse + specular) *  radiance;
        }
    }



    color += irradiance_diffuse * baseColor;

    //gamma correction
    const float gamma = 2.2;
    color = pow(color, vec3(1.0 / gamma));


    fragColor.xyz = color;
    fragColor.a = baseColor_.a;
    fragColor.a *= alpha_multiplier;

}
