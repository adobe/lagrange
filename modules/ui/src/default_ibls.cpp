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


#include "ibls/studio003.h"
#include "ibls/studio011.h"
#include "ibls/studio030.h"
#include "ibls/studio032.h"
#include "ibls/studio033.h"

namespace lagrange {
namespace ui {

IBL generate_default_ibl(const std::string& name, size_t resolution)
{
    Texture::Params p = Texture::Params::rgb();


    if (name == "studio003")
        return generate_ibl(
            std::make_shared<Texture>(ibl_studio003, ibl_studio003_len, p),
            resolution);
    if (name == "studio011")
        return generate_ibl(
            std::make_shared<Texture>(ibl_studio011, ibl_studio011_len, p),
            resolution);
    if (name == "studio030")
        return generate_ibl(
            std::make_shared<Texture>(ibl_studio030, ibl_studio030_len, p),
            resolution);
    if (name == "studio032")
        return generate_ibl(
            std::make_shared<Texture>(ibl_studio032, ibl_studio032_len, p),
            resolution);
    if (name == "studio033")
        return generate_ibl(
            std::make_shared<Texture>(ibl_studio033, ibl_studio033_len, p),
            resolution);

    return IBL{};
}

} // namespace ui
} // namespace lagrange
