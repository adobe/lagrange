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

#include <functional>
#include <string>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <entt/entt.hpp>
#include <lagrange/utils/warnon.h>
// clang-format on

namespace lagrange {
namespace ui {

using Registry = entt::registry;
using Entity = entt::entity;
inline constexpr entt::null_t NullEntity{};

using System = std::function<void(Registry& registry)>;
using StringID = entt::id_type;

/*
    Globally used enums
*/
enum class IndexingMode { VERTEX, EDGE, FACE, CORNER, INDEXED };
enum class PrimitiveType { POINTS, LINES, TRIANGLES };
enum class SelectionBehavior { SET, ADD, ERASE };

using namespace entt::literals;

inline StringID string_id(const std::string& str)
{
    return entt::hashed_string(str.c_str());
}
inline StringID string_id(const char* str)
{
    return entt::hashed_string(str);
}

template <typename T>
void component_clone(Registry* w, Entity src, Entity dst)
{
    w->emplace_or_replace<T>(dst, w->get<T>(src));
}

template <typename T>
void component_move(Registry* w, Entity src, Entity dst)
{
    w->emplace_or_replace<T>(dst, std::move(w->get<T>(src)));
    w->remove<T>(src);
}

template <typename T>
void component_add_default(Registry* w, Entity dst)
{
    w->emplace_or_replace<T>(dst);
}

/// Payload for sending entities through UI
struct PayloadEntity
{
    static const char* id() { return "PayloadEntity"; }
    Entity entity;
};


/// Payload for sending components through UI
struct PayloadComponent
{
    static const char* id() { return "PayloadComponent"; }
    entt::id_type component_hash;
    Entity entity;
};


template <typename Component>
void register_component(const std::string& display_name = entt::type_id<Component>().name().data())
{
    using namespace entt::literals;
    entt::meta<Component>()
        .template func<&component_clone<Component>>("component_clone"_hs)
        .template func<&component_move<Component>>("component_move"_hs)
        .template func<&component_add_default<Component>>("component_add_default"_hs)
        .prop("display_name"_hs, display_name);
}

template <typename Component, auto Func>
void register_component_widget()
{
    using namespace entt::literals;
    entt::meta<Component>().template func<Func>("show_widget"_hs);
}

template <typename Component>
void register_component_widget(const std::function<void(Registry*, Entity)>& fn)
{
    using namespace entt::literals;
    entt::meta<Component>().prop("show_widget_lambda"_hs, fn);
}

inline void show_widget(Registry& w, Entity e, const entt::meta_type& meta_type)
{
    using namespace entt::literals;
    auto fn = meta_type.func("show_widget"_hs);
    if (fn) {
        fn.invoke({}, &w, e);
    } else {
        auto lambda = meta_type.prop("show_widget_lambda"_hs);
        if (lambda) {
            lambda.value().cast<std::function<void(Registry*, Entity)>>()(&w, e);
        }
    }
}

template <typename Resolvable>
void show_widget(Registry& r, Entity e, const Resolvable& resolvable)
{
    show_widget(r, e, entt::resolve(resolvable));
}


} // namespace ui
} // namespace lagrange
