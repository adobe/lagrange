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

#include <lagrange/ui/Entity.h>
#include <lagrange/ui/api.h>

namespace lagrange {
namespace ui {

struct ToolbarPanel
{
    static constexpr float toolbar_width = 45.0f;

    bool _dummy;
};


LA_UI_API Entity add_toolbar_panel(Registry& r, const std::string& name = "Toolbar");


} // namespace ui
} // namespace lagrange
