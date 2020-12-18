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

#include <array>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include <lagrange/ui/GLContext.h>

namespace lagrange {
namespace ui {

///
/// Stores keybinds for actions
///
/// Actions are identified using strings
/// Use syntax "context.optional_category.action", e.g. "viewport.camera.pan"
/// Use "global" context for keybinds to be available everywhere
///
class Keybinds
{
public:
    Keybinds();

    enum class KeyState { NONE, PRESSED, DOWN, RELEASED };
    static const int keymap_size = GLFW_KEY_LAST;

    ///
    /// Key/mouse shortcut
    ///
    /// Stores main button, modifiers and current and previous state
    ///
    struct Keybind
    {
        Keybind(int btn, const std::vector<int>& modifier_keys);
        int button = -1;
        int modifier_count = 0;
        KeyState previous_state = KeyState::NONE;
        KeyState current_state = KeyState::NONE;
        std::array<int, 6> modifiers = {
            -1,
            -1,
            -1,
            -1,
            -1,
            -1,
        };
    };


    /// Internal map type
    using MapType = std::map<std::string, std::vector<Keybind>>;

    /// @brief Updates states of keybinds
    ///
    /// Call at the beginning of every frame
    ///
    /// @param context
    void update(const std::string& context = "global");


    /// @brief Adds a key binding for given action
    ///
    /// Registers action if it doesn't exist
    ///
    /// @param action string identifier
    /// @param button which button
    /// @param modifiers modifier buttons
    void add(const std::string& action, int button, const std::vector<int>& modifiers = {});


    /// @brief Checks if an exact keybinding exists for given action
    ///
    /// @param action string identifier
    /// @param button which button
    /// @param modifiers modifier buttons
    bool has(const std::string& action, int button, const std::vector<int>& modifiers = {}) const;


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

    /// @brief Returns true if action is held down
    ///
    /// Returns true also when action was just pressed
    ///
    /// @param action string identifier
    /// @return true if down or pressed
    bool is_down(const std::string& action) const;

    /// @brief Returns true if action was just released
    /// @param action string identifier
    /// @return true if pressed
    bool is_released(const std::string& action) const;

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

    /// Converts GLFW key to string
    static std::string to_string(int key);

    /// Creates a string with keybinds for given action, up to a limit
    std::string to_string(const std::string& action, int limit = 1) const;

    /// Updates key map from glfw callback
    void set_key_state(int key, int action);

private:
    MapType m_mapping;
    bool m_enabled = true;
    bool m_key_map[keymap_size];
    int m_key_down_num = 0;

    bool is_action_in_state(const std::string& action, KeyState state) const;
};


///TODO serialization here

} // namespace ui
} // namespace lagrange
