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

#include <vector>
#include <string>
#include <atomic>

namespace lagrange {
namespace ui {

class BaseObject {
public:

    BaseObject(const std::string& name = "Unnamed Object");
    virtual ~BaseObject();

    virtual std::vector<BaseObject*> get_selection_subtree();

    const std::string& get_name();

    bool is_selectable() const;
    void set_selectable(bool value);

    //Allow visualizations besides default ones
    bool is_visualizable() const;
    void set_visualizable(bool value);

    

protected:
    bool m_selectable = true;
    bool m_visualizable = true;
    bool m_is_ground = false;
    std::string m_name;

};


} // namespace ui
} // namespace lagrange
