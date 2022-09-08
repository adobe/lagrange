/*
 * Copyright 2021 Adobe. All rights reserved.
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
#include <string>


/// Events triggered by the default systems
/// Use `ui::on<Event>(registry, [](Event& e){})` to register a listener
/// Use `ui::publish<Event>(registry, args ...)` to trigger a custom event
/// See <lagrange/ui/utils/events.h> for more details

namespace lagrange {
namespace ui {


struct WindowResizeEvent
{
    int width, height;
};

struct WindowCloseEvent
{
};

struct WindowDropEvent
{
    int count;
    const char** paths;
};

/// Triggered when Transform component has changed
/// Only monitors global transform (not local)
/// Note: this check is only performed when at least one listener is registered. When enabled,
/// it is checked for all entities with Transform component.
struct TransformChangedEvent
{
    Entity entity;
};

/// Triggered when Camera component has changed in the default systems
struct CameraChangedEvent
{
    Entity entity;
};

/// Triggered when Selected component is added to an entity
struct SelectedEvent
{
    Entity entity;
};

/// Triggered when Selected component is removed from an entity
struct DeselectedEvent
{
    Entity entity;
};

/// Triggered when Hovered component is added to an entity
struct HoveredEvent
{
    Entity entity;
};

/// Triggered when Hovered component is removed from an entity
struct DehoveredEvent
{
    Entity entity;
};

/// Triggered when MeshRender component has changed
struct MeshRenderChangedEvent
{
    Entity entity;
};

/// Triggered when Light component has changed
struct LightComponentChangedEvent
{
    Entity entity;
};

/// Triggered when IBL has changed
struct IBLChangedEvent
{
    Entity entity;
    bool image_changed = false;
    bool parameters_changed = false;
};

/*
 *   TODO key/mouse events
 */

} // namespace ui
} // namespace lagrange