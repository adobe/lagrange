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

#include <cassert>
#include <memory>
#include <unordered_map>
#include <vector>

#include <lagrange/ui/Resource.h>

namespace lagrange {
namespace ui {

class RenderPassBase;

/// Records reads and writes of resources
struct RenderResourceDependencies
{
    std::vector<std::shared_ptr<BaseResourceData>> resources;
    std::unordered_map<std::shared_ptr<BaseResourceData>, std::vector<RenderPassBase*>> reads;
    std::unordered_map<std::shared_ptr<BaseResourceData>, std::vector<RenderPassBase*>> writes;
};

class RenderResourceBuilder
{
public:
    /// Current render pass on stack reads this resource
    template <typename T>
    Resource<T> read(const Resource<T>& resource)
    {
        m_deps.reads[resource.data()].push_back(m_current_pass);
        return resource;
    }

    /// Current render pass on stack writes to this resource
    template <typename T>
    Resource<T> write(const Resource<T>& resource)
    {
        m_deps.writes[resource.data()].push_back(m_current_pass);
        return resource;
    }

    /// Creates a uniquely named resource
    /// If resouce already exists returns it instead
    template <typename T, typename... Args>
    Resource<T> create(const std::string& name, Args... args)
    {
        // Check if resource already exists
        auto it = m_name_to_resource.find(name);
        if (it != m_name_to_resource.end()) {
            auto casted = std::dynamic_pointer_cast<ResourceData<T>>(it->second);
            LA_ASSERT(casted, "Existing render resource of the same name has a different type");
            return Resource<T>(std::move(casted));
        }

        // Create deferred resource
        auto res = Resource<T>::create_deferred(std::forward<Args>(args)...);

        // Save to resources array for further inspection from UI
        m_deps.resources.push_back(res.data());
        m_name_to_resource.emplace(name, res.data());

        // A write is implicit
        write(res);
        return res;
    }

    /// Set current render pass
    void set_pass(RenderPassBase* pass);


    RenderResourceDependencies compile();

private:
    RenderPassBase* m_current_pass = nullptr;
    RenderResourceDependencies m_deps;
    std::unordered_map<std::string, std::shared_ptr<BaseResourceData>> m_name_to_resource;
};

} // namespace ui
} // namespace lagrange
