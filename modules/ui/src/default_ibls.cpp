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
#include <lagrange/ui/default_ibls.h>
#include <lagrange/ui/Shader.h>
#include <lagrange/Logger.h>

#include "ibls/studio003.h"
#include "ibls/studio011.h"
#include "ibls/studio030.h"
#include "ibls/studio032.h"
#include "ibls/studio033.h"

namespace lagrange {
namespace ui {

std::unique_ptr<lagrange::ui::IBL> create_default_ibl(const std::string& name)
{
    Texture::Params p = Texture::Params::rgb();

    try {
        if (name == "studio003")
            return std::make_unique<IBL>(
                "studio003", Resource<Texture>::create(ibl_studio003, ibl_studio003_len, p));
        if (name == "studio011")
            return std::make_unique<IBL>(
                "studio011", Resource<Texture>::create(ibl_studio011, ibl_studio011_len, p));
        if (name == "studio030")
            return std::make_unique<IBL>(
                "studio030", Resource<Texture>::create(ibl_studio030, ibl_studio030_len, p));
        if (name == "studio032")
            return std::make_unique<IBL>(
                "studio032", Resource<Texture>::create(ibl_studio032, ibl_studio032_len, p));
        if (name == "studio033")
            return std::make_unique<IBL>(
                "studio033", Resource<Texture>::create(ibl_studio033, ibl_studio033_len, p));
    }
    catch (ShaderException& ex) {
        logger().error("{} in {}", ex.what() , ex.get_desc());
    }

    return nullptr;
}

} // namespace ui
} // namespace lagrange
