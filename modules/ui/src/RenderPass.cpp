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
#include <lagrange/ui/RenderPass.h>

namespace lagrange {
namespace ui {

RenderPassBase::RenderPassBase(const std::string& name) : m_name(name), m_enabled(true), m_one_shot(false)
{}

RenderPassBase::~RenderPassBase()
{}

const OptionSet& RenderPassBase::get_options() const
{
    return m_options;
}

OptionSet& RenderPassBase::get_options()
{
    return m_options;
}

const std::string& RenderPassBase::get_name() const
{
    return m_name;
}

bool RenderPassBase::is_enabled() const
{
    return m_enabled;
}

void RenderPassBase::set_enabled(bool value)
{
    m_enabled = value;
}

bool RenderPassBase::is_one_shot() const
{
    return m_one_shot;
}

void RenderPassBase::set_one_shot(bool value)
{
    m_one_shot = value;
    if (value)
        add_tag("oneshot");
    else
        remove_tag("oneshot");
}

void RenderPassBase::add_tag(const std::string& tag)
{
    m_tags.insert(tag);
}

void RenderPassBase::add_tags(const std::vector<std::string>& tags)
{
    for (auto t : tags) add_tag(t);
}

bool RenderPassBase::remove_tag(const std::string& tag)
{
    return m_tags.erase(tag) > 0;
}

const std::unordered_set<std::string>& RenderPassBase::get_tags() const
{
    return m_tags;
}

bool RenderPassBase::has_tag(const std::string& tag) const
{
    return m_tags.find(tag) != m_tags.end();
}

}
}
