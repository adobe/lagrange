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

#include <memory>
#include <set>
#include <unordered_set>
#include <vector>

#include <lagrange/ui/RenderPass.h>
#include <lagrange/ui/RenderResourceBuilder.h>
#include <lagrange/ui/ui_common.h>

namespace lagrange {
namespace ui {

class RenderPipeline
{
public:
    template <typename PassData>
    RenderPass<PassData>* add_pass(
        const std::string& name,
        typename RenderPass<PassData>::SetupFunc&& setup,
        typename RenderPass<PassData>::ExecuteFunc&& execute)
    {
        auto pass =
            std::make_unique<RenderPass<PassData>>(name, std::move(setup), std::move(execute));

        auto ptr = pass.get();
        m_name_to_pass[pass->get_name()] = ptr;

        m_passes.push_back(std::move(pass));
        reset();
        return ptr;
    }

    RenderPassBase* add_pass(std::unique_ptr<RenderPassBase> pass);
    bool remove(RenderPassBase* pass);

    /// Execute the rendering pipeline
    ///
    /// Will execute all enabled passes in order.
    /// If set_custom_execution was used,
    /// it will use the provided function instead
    void execute();

    const std::vector<std::unique_ptr<RenderPassBase>>& get_passes() const;
    std::vector<std::unique_ptr<RenderPassBase>>& get_passes();

    const RenderPassBase* get_pass(const std::string& name) const;
    RenderPassBase* get_pass(const std::string& name);

    void reset();

    bool is_initialized() const { return m_initialized; }

    template <typename T>
    std::vector<Resource<T>> get_resources()
    {
        std::vector<Resource<T>> result;
        for (const auto& res : m_resource_deps.resources) {
            auto casted = std::dynamic_pointer_cast<ResourceData<T>>(res);
            if (casted) {
                result.push_back(casted);
            }
        }

        return result;
    }

    std::unordered_set<RenderPassBase*> get_dependencies(
        const std::set<RenderPassBase*>& selection);
    std::unordered_set<RenderPassBase*> enable_with_dependencies(
        const std::set<RenderPassBase*>& selection);

    /// Overrides default execution order
    void set_custom_execution(std::function<void(RenderPipeline&)> fn);

    /// Mark one shot pass as executed
    void set_one_shot_executed(RenderPassBase* pass);

    /// Query if one shot pass was executed
    bool was_one_shot_executed(RenderPassBase* pass) const;

private:
    void initialize();

    bool m_initialized = false;

    std::vector<std::unique_ptr<RenderPassBase>> m_passes;
    RenderResourceDependencies m_resource_deps;
    std::unordered_map<std::string, RenderPassBase*> m_name_to_pass;
    std::unordered_set<RenderPassBase*> m_one_shot_passes;
    std::function<void(RenderPipeline&)> m_custom_execution;
};

} // namespace ui
} // namespace lagrange
