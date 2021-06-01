#pragma once

#include <entt/entt.hpp>
#include <functional>
#include <string>


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
    entt::meta<Component>().type();
    entt::meta<Component>().template func<&component_clone<Component>>("component_clone"_hs);
    entt::meta<Component>().template func<&component_move<Component>>("component_move"_hs);
    entt::meta<Component>().template func<&component_add_default<Component>>(
        "component_add_default"_hs);
    entt::meta<Component>().prop("display_name"_hs, display_name);
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
