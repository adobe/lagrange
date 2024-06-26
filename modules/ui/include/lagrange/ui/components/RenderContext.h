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
#include <lagrange/ui/components/Viewport.h>

namespace lagrange {
namespace ui {


/*
    Context variable
*/

struct RenderContext
{
    Entity viewport = NullEntity;
    int polygon_offset_layer = 0;
};

inline RenderContext& get_render_context(Registry& r)
{
    return r.ctx().emplace<RenderContext>();
}

inline ViewportComponent& get_render_context_viewport(Registry& r)
{
    return r.get<ViewportComponent>(get_render_context(r).viewport);
}


} // namespace ui
} // namespace lagrange
