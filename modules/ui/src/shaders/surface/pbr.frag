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
#include "util/pbr_shading.glsl"

void main(){
    fragColor = pbr(vs_out_pos, vs_out_normal, vs_out_uv, vs_out_color, vs_out_tangent, vs_out_bitangent);
    fragColor.a *= alpha_multiplier;
}
