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

#include <lagrange/ui/Entity.h>
#include <lagrange/utils/strings.h>
#include <string>
#include <unordered_set>


namespace lagrange {
namespace ui {


struct Name : public std::string
{
};


struct GlobalTime
{
    /// Time from start in seconds
    double t = 0.0;
    /// Time from last frame in seconds
    double dt = 0.0;
};


// TODO move to utils:

inline std::string get_name(const Registry& r, Entity e)
{
    if (!r.valid(e)) return lagrange::string_format("Invalid Entity (ID={})", e);
    if (!r.all_of<Name>(e)) return lagrange::string_format("Unnamed Entity (ID={})", e);
    return r.get<Name>(e);
}

inline bool set_name(Registry& r, Entity e, const std::string& name)
{
    if (!r.valid(e)) return false;
    r.emplace_or_replace<Name>(e, name);
    return true;
}

inline const GlobalTime& get_time(const Registry& r)
{
    return r.ctx<GlobalTime>();
}


} // namespace ui
} // namespace lagrange