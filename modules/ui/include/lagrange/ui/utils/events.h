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

#include <lagrange/ui/components/EventEmitter.h>

// Wrapper for entt::emitter class functionality
// The emitter is stored in registry's context variable
// These function's are a short-hand for adding/triggering events

namespace lagrange {
namespace ui {

inline EventEmitter& get_event_emitter(Registry& r)
{
    return r.ctx().emplace<EventEmitter>(EventEmitter());
}

/// @brief Register a listener for Event
/// @tparam Event
/// @param r Registry instance
/// @param listener function taking reference to Event as parameter
/// @return Connection instance, can be used to disconnect listener later
template <typename Event>
void on(Registry& r, std::function<void(Event&)> listener)
{
    get_event_emitter(r).on<Event>(
        [=](Event& event, EventEmitter& /*emitter*/) { listener(event); });
}


/// @brief Trigger an event of type Event
/// @tparam Event
/// @param r Registry instance
/// @param ...args Arguments to construct Event
template <typename Event, typename... Args>
void publish(Registry& r, Args&&... args)
{
    get_event_emitter(r).publish<Event>(Event{std::forward<Args>(args)...});
}

/// @brief Utility function for forwarding events that contain only an entity identifier
template <typename Event>
void forward_entity_event(Registry& r, Entity e)
{
    ui::publish<Event>(r, e);
}

/// @brief Enable/Disable on_construct and on_destroy events depending on whether
/// ConstructEvent and DestroyEvent have any listeners.
template <typename Component, typename ConstructEvent, typename DestroyEvent>
void toggle_component_event(Registry& r)
{
    if (get_event_emitter(r).contains<ConstructEvent>()) {
        r.on_construct<Component>().template connect<&forward_entity_event<ConstructEvent>>();
    } else {
        r.on_construct<Component>().template disconnect<&forward_entity_event<ConstructEvent>>();
    }

    if (get_event_emitter(r).contains<DestroyEvent>()) {
        r.on_destroy<Component>().template connect<&forward_entity_event<DestroyEvent>>();
    } else {
        r.on_destroy<Component>().template disconnect<&forward_entity_event<DestroyEvent>>();
    }
}

} // namespace ui
} // namespace lagrange
