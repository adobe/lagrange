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
#include <lagrange/utils/assert.h>
#include <lagrange/utils/strings.h>
#include <nlohmann/json.hpp>


#include <iomanip>

namespace lagrange {
namespace ui {

void Keybinds::update()
{
    const auto in_context = [&](const std::string& name) -> std::pair<bool, int> {
        int cur = static_cast<int>(name.length()) - 1;
        int end = static_cast<int>(name.length());
        int stack_pos = static_cast<int>(m_context_stack.size()) - 1;
        int token = 0;

        if (name.length() == 0) return {false, stack_pos};
        if (name.back() == '.') return {false, stack_pos};

        while (cur >= -1) {
            // Token encountered, check against top of stack
            if (cur == -1 || name[cur] == '.') {
                if (token == 0) {
                    end = cur;
                    cur--;
                    token++;
                    continue;
                }

                if (stack_pos < 0) {
                    // Ran out of stack
                    return {false, stack_pos};
                }

                const auto& ctx_name = m_context_stack[stack_pos];
                const auto sub = name.substr(cur + 1, end - (cur + 1));
                if (ctx_name != sub) {
                    return {false, stack_pos};
                }

                end = cur;
                stack_pos--;
            } else {
                // do nothing
            }
            cur--;
        }

        return {true, stack_pos};
    };

    std::vector<Keybind*> active_keybinds;
    // Keybind* active = nullptr;
    int active_stack_pos = std::numeric_limits<int>::max();

    // Perform state transition
    // and collect currently active (pressed or down) keybinds
    for (auto& it : m_mapping) {
        for (auto& keybind : it.second) {
            bool is_down = ImGui::IsKeyDown(keybind.button);
            for (auto i = 0; i < keybind.modifier_count; i++) {
                is_down &= ImGui::IsKeyDown(keybind.modifiers[i]);
            }

            bool is_in_context = false;
            int context_stack_pos = std::numeric_limits<int>::max();
            if (is_down) {
                auto res = in_context(it.first);
                is_in_context = res.first;
                context_stack_pos = res.second;
            }

            // Find an actively pressed keybind that matches the current context the most (lower
            // context stack pos)
            if (is_down && is_in_context && context_stack_pos <= active_stack_pos) {
                // Reset active keybinds if in lower context
                if (context_stack_pos < active_stack_pos) {
                    active_keybinds.clear();
                    active_stack_pos = context_stack_pos;
                }
                active_keybinds.push_back(&keybind);
            }

            //Transition form pressed/down to released
            if ((keybind.current_state == KeyState::DOWN ||
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

    // Transition active keybind (none->pressed or pressed->down)
    for (auto active : active_keybinds) {
        auto& keybind = *active;
        if (keybind.current_state == KeyState::NONE) {
            keybind.previous_state = keybind.current_state;
            keybind.current_state = KeyState::PRESSED;
        } else if (keybind.current_state == KeyState::PRESSED) {
            keybind.previous_state = keybind.current_state;
            keybind.current_state = KeyState::DOWN;
        }
    }
}

void Keybinds::push_context(const std::string& context)
{
    m_context_stack.push_back(context);
}

void Keybinds::pop_context()
{
    m_context_stack.pop_back();
}

void Keybinds::reset_context()
{
    m_context_stack.clear();
}

void Keybinds::add(
    const std::string& action,
    ImGuiKey button,
    const std::vector<ImGuiKey>& modifiers /*= {}*/)
{
    m_mapping[action].emplace_back(button, modifiers);
}


void Keybinds::add(const std::string& action, const Keybind& keybind)
{
    m_mapping[action].push_back(keybind);
}

bool Keybinds::has(
    const std::string& action,
    ImGuiKey button,
    const std::vector<ImGuiKey>& modifiers /*= {}*/) const
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


Keybinds::Keybind::Keybind(ImGuiKey btn, const std::vector<ImGuiKey>& modifier_keys)
    : button(btn)
    , modifier_count((int)std::min(modifiers.size(), modifier_keys.size()))
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

            const ImGuiKey button = (ImGuiKey)keybind["button"].get<int>();


            std::vector<ImGuiKey> mods;
            for (auto mod : keybind["modifiers"]) {
                mods.push_back((ImGuiKey)mod.get<int>());
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
    la_runtime_assert(it != m_mapping.end(), "Action '" + action + "' keybind not registered");

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

std::string Keybinds::to_string(ImGuiKey key)
{
    return ImGui::GetKeyName(key);
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

} // namespace ui
} // namespace lagrange
