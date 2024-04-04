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
#include <lagrange/ui/api.h>

#include <vector>

namespace lagrange {
namespace ui {

/// @brief Container for `System`s
/// @code
///     Systems S;
///     //Add systems to the Init stage
///     StringID first_system = S.add(Systems::Stage::Init, [](Registry & r){});
///     StringID second_system = S.add(Systems::Stage::Init, [](Registry & r){});
///
///     Registry r;
///     //Run the systems in the Init stage
///     S.run(Systems::Stage::Init, r);
/// @endcode
class LA_UI_API Systems
{
public:
    enum class Stage : int { Init = 0, Interface, Simulation, Render, Post, _SIZE };

    /// @brief Runs a stage. Executes each system in this stage in order.
    /// @param stage Which stage
    /// @param registry Registry to pass to system execution
    void run(Stage stage, Registry& registry);

    /// @brief Adds a system which will be executed at a given stage
    /// @param stage Which `Stage` should the system be executed in
    /// @param system `System` function to execute
    /// @param id Optional identifier. Generated otherwise.
    /// @return If `id` wasn't given, this function will return a generated `StringID`
    ///         If `id` was given, it will return the `id`, or `0` if the system with id already exists
    StringID add(Stage stage, const System& system, StringID id = 0);

    /// @brief Enables or disables given system
    /// @param id id of the system, previously return by `add()`
    /// @param value enable or disable
    /// @return false if system exists
    bool enable(StringID id, bool value);

    /// @brief Places system with `system_id` after the system with `after_id`
    /// Note: Does not handle cycles nor topological ordering, only moves `system_id` in execution order.
    /// @param system StringID
    /// @param after StringID
    /// @return true on success, false if `system_id` or `after_id` do not exist
    bool succeeds(StringID system_id, StringID after_id);

    /// @brief Removes system identifier with `id`
    /// @param id
    /// @return true if system existed and was removed
    bool remove(StringID id);

private:
    struct SystemItem
    {
        System system;
        Stage stage;
        StringID id;
        bool enabled = true;
    };

    StringID new_id();
    StringID m_id_counter = 1;

    std::vector<SystemItem> m_items;
};


} // namespace ui
} // namespace lagrange
