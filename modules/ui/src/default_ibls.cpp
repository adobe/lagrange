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
#include <lagrange/Logger.h>
#include <lagrange/ui/default_ibls.h>
#include <lagrange/ui/utils/ibl.h>


#include "ibls/studio011.h"


namespace lagrange {
namespace ui {

IBL generate_default_ibl(size_t resolution)
{
    Texture::Params p = Texture::Params::rgb();
    p.sRGB = false;
    auto ibl =
        generate_ibl(std::make_shared<Texture>(ibl_studio011, ibl_studio011_len, p), resolution);
    ibl.blur = 2.0f;
    return ibl;
}


} // namespace ui
} // namespace lagrange
