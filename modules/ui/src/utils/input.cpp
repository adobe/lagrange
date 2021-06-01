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

#include <lagrange/ui/utils/input.h>

namespace lagrange {
namespace ui {


InputState& get_input(Registry& r)
{
    return r.ctx<InputState>();
}

const InputState& get_input(const Registry& r)
{
    return r.ctx<InputState>();
}

Keybinds& get_keybinds(Registry& r)
{
    return *get_input(r).keybinds;
}

const Keybinds& get_keybinds(const Registry& r)
{
    return *get_input(r).keybinds;
}

} // namespace ui
} // namespace lagrange