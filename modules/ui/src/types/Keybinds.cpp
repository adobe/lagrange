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
#include <imgui.h>
#include <lagrange/Logger.h>
#include <lagrange/ui/types/Keybinds.h>
#include <lagrange/utils/la_assert.h>
#include <lagrange/utils/strings.h>
#include <nlohmann/json.hpp>


#include <iomanip>

namespace lagrange {
namespace ui {

Keybinds::Keybinds()
{
    for (auto i = 0; i < Keybinds::keymap_size; i++) {
        m_key_map[i] = false;
    }
}

void Keybinds::update(const std::string& context)
{
    std::vector<Keybind*> active;

    const auto in_context = [&](const std::string& name) {
        return starts_with(name, context) || starts_with(name, "global");
    };

    // Perform state transition
    // and collect currently active (pressed or down) keybinds
    for (auto& it : m_mapping) {
        for (auto& keybind : it.second) {
            bool is_down = m_key_map[keybind.button];
            for (auto i = 0; i < keybind.modifier_count; i++) {
                is_down &= m_key_map[keybind.modifiers[i]];
            }

            // Transition from none to pressed (and down)
            // Only if is in context
            if (keybind.current_state == KeyState::NONE && is_down && in_context(it.first)) {
                keybind.previous_state = keybind.current_state;
                keybind.current_state = KeyState::PRESSED;
            }
            // Transition from pressed to only down
            else if (keybind.current_state == KeyState::PRESSED && is_down) {
                keybind.previous_state = keybind.current_state;
                keybind.current_state = KeyState::DOWN;
            }
            //Transition form pressed/down to released
            else if (
                (keybind.current_state == KeyState::DOWN ||
                 keybind.current_state == KeyState::PRESSED) &&
                !is_down) {
                keybind.previous_state = keybind.current_state;
                keybind.current_state = KeyState::RELEASED;
            }
            // Transition from released to none
            else if (keybind.current_state == KeyState::RELEASED && !is_down) {
                keybind.previous_state = keybind.current_state;
                keybind.current_state = KeyState::NONE;
            }
        }
    }
}

void Keybinds::add(
    const std::string& action,
    int button,
    const std::vector<int>& modifiers /*= {}*/)
{
    m_mapping[action].emplace_back(button, modifiers);
}


void Keybinds::add(const std::string& action, const Keybind& keybind)
{
    m_mapping[action].push_back(keybind);
}

bool Keybinds::has(
    const std::string& action,
    int button,
    const std::vector<int>& modifiers /*= {}*/) const
{
    const auto& map_it = m_mapping.find(action);
    if (map_it == m_mapping.end()) return false;

    for (const auto& keybind : map_it->second) {
        if (keybind.button == button) {
            bool mod_match = true;
            for (const auto& mod : modifiers) {
                mod_match &=
                    (std::find(modifiers.begin(), modifiers.end(), mod) != modifiers.end());
            }
            if (mod_match) return true;
        }
    }

    return false;
}

const Keybinds::MapType& Keybinds::get() const
{
    return m_mapping;
}

bool Keybinds::remove(const std::string& action)
{
    auto it = m_mapping.find(action);
    if (it == m_mapping.end()) return false;

    bool has_some = it->second.size() > 0;
    m_mapping[action].clear();
    return has_some;
}

bool Keybinds::unregister_action(const std::string& action)
{
    return m_mapping.erase(action) > 0;
}

bool Keybinds::register_action(const std::string& action)
{
    auto it = m_mapping.find(action);
    if (it != m_mapping.end()) return false;

    m_mapping[action] = {};
    return true;
}

bool Keybinds::is_pressed(const std::string& action) const
{
    return is_action_in_state(action, KeyState::PRESSED);
}

bool Keybinds::is_down(const std::string& action) const
{
    return is_action_in_state(action, KeyState::DOWN) ||
           is_action_in_state(action, KeyState::PRESSED);
}

bool Keybinds::is_released(const std::string& action) const
{
    return is_action_in_state(action, KeyState::RELEASED);
}


Keybinds::Keybind::Keybind(int btn, const std::vector<int>& modifier_keys)
    : button(btn)
    , modifier_count(std::min(6, int(modifier_keys.size())))
{
    for (auto i = 0; i < modifier_count; i++) {
        modifiers[i] = modifier_keys[i];
    }
}

bool Keybinds::save(std::ostream& out) const
{
    if (!out.good()) return false;

    using namespace nlohmann;
    json j;
    for (auto& it : m_mapping) {
        auto arr = json::array();
        for (auto& keybind : it.second) {
            json c;
            c["button"] = keybind.button;
            c["modifiers"] = json::array();
            for (auto i = 0; i < keybind.modifier_count; i++) {
                c["modifiers"].push_back(keybind.modifiers[i]);
            }

            arr.push_back(std::move(c));
        }
        j[it.first] = std::move(arr);
    }

    out << std::setw(4) << j;

    return out.good();
}

bool Keybinds::load(std::istream& in, bool append)
{
    if (!in.good()) return false;

    if (!append) {
        m_mapping.clear();
    }

    using namespace nlohmann;

    json j;
    in >> j;

    if (j.empty()) return false;

    for (auto& action : j.items()) {
        for (auto& keybind : action.value()) {
            if (!keybind.contains("button")) continue;
            if (!keybind.contains("modifiers")) continue;

            const auto button = keybind["button"].get<int>();


            std::vector<int> mods;
            for (auto mod : keybind["modifiers"]) {
                mods.push_back(mod.get<int>());
            }

            add(action.key(), button, mods);
        }

        if (action.value().empty()) {
            register_action(action.key());
        }
    }

    return true;
}

void Keybinds::enable(bool enabled)
{
    m_enabled = enabled;
}

bool Keybinds::is_enabled() const
{
    return m_enabled;
}


bool Keybinds::is_action_in_state(const std::string& action, KeyState state) const
{
    if (!m_enabled) return false;

    auto it = m_mapping.find(action);
    LA_ASSERT(it != m_mapping.end(), "Action '" + action + "' keybind not registered");

    for (auto& keybind : it->second) {
        if (keybind.current_state == state) return true;
    }

    return false;
}


std::string Keybinds::to_string(const Keybind& keybind)
{
    std::string result;

    for (auto i = 0; i < keybind.modifier_count; i++) {
        result += to_string(keybind.modifiers[i]) + " + ";
    }
    result += to_string(keybind.button);
    return result;
}

std::string Keybinds::to_string(int key)
{
    if (key < 0) return "None";

    if (key > GLFW_KEY_SPACE && key <= GLFW_KEY_GRAVE_ACCENT) {
        return std::string(1, char(key));
    }

    if (key >= GLFW_KEY_F1 && key <= GLFW_KEY_F25) {
        return "F" + std::string(1, char(key - GLFW_KEY_F1));
    }

    if (key >= GLFW_MOUSE_BUTTON_4 && key <= GLFW_MOUSE_BUTTON_8) {
        return "Mouse Button " + std::string(1, char(key - GLFW_MOUSE_BUTTON_4 + 4));
    }

    switch (key) {
    case GLFW_KEY_LEFT_ALT: return "Alt";
    case GLFW_KEY_LEFT_CONTROL: return "Control";
    case GLFW_KEY_LEFT_SHIFT: return "Shift";
    case GLFW_KEY_LEFT_SUPER: return "Super";
    case GLFW_KEY_RIGHT_ALT: return "Right Alt";
    case GLFW_KEY_RIGHT_CONTROL: return "Right Control";
    case GLFW_KEY_RIGHT_SHIFT: return "Right Shift";
    case GLFW_KEY_RIGHT_SUPER: return "Right Super";
    case GLFW_KEY_SPACE: return "Space";
    case GLFW_KEY_BACKSPACE: return "Backspace";
    case GLFW_KEY_INSERT: return "Insert";
    case GLFW_KEY_DELETE: return "Delete";
    case GLFW_KEY_RIGHT: return "Right";
    case GLFW_KEY_LEFT: return "Left";
    case GLFW_KEY_UP: return "Up";
    case GLFW_KEY_DOWN: return "Down";
    case GLFW_KEY_PAGE_UP: return "Page Up";
    case GLFW_KEY_PAGE_DOWN: return "Page Down";
    case GLFW_KEY_HOME: return "Home";
    case GLFW_KEY_END: return "End";
    case GLFW_MOUSE_BUTTON_LEFT: return "Left Mouse Button";
    case GLFW_MOUSE_BUTTON_MIDDLE: return "Middle Mouse Button";
    case GLFW_MOUSE_BUTTON_RIGHT: return "Right Mouse Button";
    }


    return std::to_string(key);
}

std::string Keybinds::to_string(const std::string& action, int limit /*= 1*/) const
{
    auto it = m_mapping.find(action);
    if (it == m_mapping.end()) return "";

    std::string result;

    const int n = std::min(limit, int(it->second.size()));
    for (auto i = 0; i < n; i++) {
        result += to_string(it->second[i]);
        if (i < n - 1) result += ", ";
    }

    return result;
}

void Keybinds::set_key_state(int key, int action)
{
    if (action == GLFW_REPEAT) return;
    if (key == GLFW_KEY_UNKNOWN) return;

    bool is_down = (action == GLFW_PRESS);

    if (m_key_map[key] != is_down) {
        m_key_down_num += (is_down) ? 1 : -1;
        m_key_map[key] = is_down;
    }
}

} // namespace ui
} // namespace lagrange
