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


struct Map {
    vec4 value;
    sampler2D texture;
    bool has_texture;
};

struct MaterialPhong {
    Map diffuse;
    Map ambient;
    Map specular;
    float shininess;
    float opacity;
};

struct MaterialAdobe {
    Map baseColor;
    //Map glow;
    //Map opacity;
    float opacity;
    Map roughness;
    Map metallic;
    //Map translucence;
    //float indexOfRefraction;
    //float density;
    //vec3 interiorColor;
    //Map height;
    //float heightScale;
    Map normal;
};
