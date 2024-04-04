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
#include <lagrange/ui/types/GLContext.h>

#include <imgui.h>

#include <array>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace lagrange {
namespace ui {

///
/// Stores keybinds for actions
///
/// Actions are identified using strings
/// Use syntax "context.optional_category.action", e.g. "viewport.camera.pan"
/// Use "global" context for keybinds to be available everywhere
///
class LA_UI_API Keybinds
{
public:
    enum class KeyState { NONE, PRESSED, DOWN, RELEASED };

    Keybinds() {}

    ///
    /// Key/mouse shortcut
    ///
    /// Stores main button, modifiers and current and previous state
    ///
    struct LA_UI_API Keybind
    {
        Keybind(ImGuiKey btn, const std::vector<ImGuiKey>& modifier_keys = {});
        ImGuiKey button = ImGuiKey_None;
        int modifier_count = 0;
        KeyState previous_state = KeyState::NONE;
        KeyState current_state = KeyState::NONE;
        std::array<ImGuiKey, 6> modifiers = {
            ImGuiKey_None,
            ImGuiKey_None,
            ImGuiKey_None,
            ImGuiKey_None,
            ImGuiKey_None,
            ImGuiKey_None,
        };
    };


    /// Internal map type
    using MapType = std::map<std::string, std::vector<Keybind>>;

    /// @brief Updating key states
    ///
    /// Updates keybinds state based on key states
    ///
    void update();

    /// @brief Changes current context
    /// @param context context name
    void push_context(const std::string& context);

    /// @brief Pops the last pushed context
    /// @param context context name
    void pop_context();

    void reset_context();


    /// @brief Adds a key binding for given action
    ///
    /// Registers action if it doesn't exist
    ///
    /// @param action string identifier
    /// @param button which button
    /// @param modifiers modifier buttons
    void
    add(const std::string& action, ImGuiKey button, const std::vector<ImGuiKey>& modifiers = {});


    /// @brief Checks if an exact keybinding exists for given action
    ///
    /// @param action string identifier
    /// @param button which button
    /// @param modifiers modifier buttons
    bool has(
        const std::string& action,
        ImGuiKey button,
        const std::vector<ImGuiKey>& modifiers = {})
        const;


    /// @brief Adds a key binding for given action
    ///
    /// Registers action if it doesn't exist
    ///
    /// @param action string identifier
    /// @param keybind
    void add(const std::string& action, const Keybind& keybind);


    /// @brief All keybinds for all actions
    /// @return const MapType reference
    const MapType& get() const;


    /// @brief Removes all key bindings for given action
    /// @param action
    /// @return true if any keybinds were removed
    bool remove(const std::string& action);


    /// @brief Unregisters action and removes all its key binds
    /// @param action
    /// @return true if action existed
    bool unregister_action(const std::string& action);


    /// @brief Register an action with no keybinds
    /// @param action string identifier
    /// @return true if action did not exist before
    bool register_action(const std::string& action);


    /// @brief Returns true if action was just pressed
    /// @param action string identifier
    /// @return true if pressed
    bool is_pressed(const std::string& action) const;

    /// @brief Returns true if key was just pressed
    /// @param key_code ImGui key code
    /// @return true if pressed
    inline bool is_pressed(ImGuiKey key_code) const { return ImGui::IsKeyPressed(key_code); }

    /// @brief Returns true if action is held down
    /// @param action string identifier
    /// @return true if down or pressed
    bool is_down(const std::string& action) const;

    /// @brief Returns true if key is held down
    /// @param key_code ImGui key code
    /// @return true if down or pressed
    inline bool is_down(ImGuiKey key_code) const { return ImGui::IsKeyDown(key_code); }

    /// @brief Returns true if action was just released
    /// @param action string identifier
    /// @return true if pressed
    bool is_released(const std::string& action) const;

    /// @brief Returns true if key was just released
    /// @param key_code ImGui key code
    /// @return true if pressed
    inline bool is_released(ImGuiKey key_code) const { return ImGui::IsKeyReleased(key_code); }

    /// @brief Saves to output stream using JSON
    /// @param out any std output stream
    /// @return successfully saved
    bool save(std::ostream& out) const;


    /// @brief Loads JSON from input stream
    /// @param in any std input stream
    /// @param append if true, will append keybinds from file to current keybinds
    /// @return succesfully loaded
    bool load(std::istream& in, bool append = false);

    /// @brief Toggles processing of keybinds
    ///
    /// Use when creating new keybinds
    ///
    /// @param enabled
    void enable(bool enabled);

    /// Is keybind processing enabled
    bool is_enabled() const;


    /// Converts keybind to string
    static std::string to_string(const Keybind& keybind);

    /// Converts ImGui key to string
    static std::string to_string(ImGuiKey key);

    /// Creates a string with keybinds for given action, up to a limit
    std::string to_string(const std::string& action, int limit = 1) const;

private:
    MapType m_mapping;
    bool m_enabled = true;
    std::vector<std::string> m_context_stack;

    bool is_action_in_state(const std::string& action, KeyState state) const;
};

} // namespace ui
} // namespace lagrange
