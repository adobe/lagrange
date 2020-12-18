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
#include <lagrange/ui/BaseObject.h>

namespace lagrange {
namespace ui {

 BaseObject::BaseObject(const std::string& name /*= "Unnamed Object"*/)
: m_name(name)
{
}

BaseObject::~BaseObject() {}

std::vector<BaseObject*> BaseObject::get_selection_subtree() {
    return {this};
}

const std::string& BaseObject::get_name()
{
    return m_name;
}

bool BaseObject::is_selectable() const
{
    return m_selectable;
}

void BaseObject::set_selectable(bool value)
{
    m_selectable = value;
}

bool BaseObject::is_visualizable() const
{
    return m_visualizable;
}

void BaseObject::set_visualizable(bool value)
{
    m_visualizable = value;
}


}
}
