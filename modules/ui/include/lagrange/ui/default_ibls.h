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
#pragma once

#include <lagrange/ui/IBL.h>

namespace lagrange {
namespace ui {

///
///    Creates pre-packaged IBL
///
///    Name can be:
///        studio003
///
///        studio011
///
///        studio030
///
///        studio032
///
///        studio033
///
///    Source:
///
///        https://www.deviantart.com/zbyg/art/HDRi-Pack-1-97402522
///
///        https://www.deviantart.com/zbyg/art/HDRi-Pack-2-103458406
///
///        https://www.deviantart.com/zbyg/art/HDRi-Pack-3-112847728
///
///        CC 3.0 License
std::unique_ptr<IBL> create_default_ibl(const std::string & name = "studio011");

}
}
