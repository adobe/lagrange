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

uniform int num_samples = 4;

vec4 multisample(sampler2DMS s, vec2 uv)
{
    ivec2 texel = ivec2(floor(textureSize(s) * uv));

    vec4 color = vec4(0.0);
    for (int i = 0; i < num_samples; i++)
    {
        color += texelFetch(s, texel, i);
    }

    color /= float(num_samples);

    return color;
}
