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

#include <lagrange/ui/api.h>
#include <lagrange/ui/Entity.h>

namespace lagrange {
namespace ui {

class Tools;

struct ElementObject
{
};

struct ElementVertex
{
};

struct ElementFace
{
};
struct ElementEdge
{
};

struct SelectToolTag
{
};

struct TranslateToolTag
{
};

struct RotateToolTag
{
};

struct ScaleToolTag
{
};

// For reflection info
struct DefaultTools
{
    std::vector<entt::id_type> element_types;
    std::vector<entt::id_type> tool_types;
};


LA_UI_API void register_default_tools(Tools& tools);
LA_UI_API void select(Registry& r, const std::function<void(Registry&)>& selection_system);

} // namespace ui
} // namespace lagrange
