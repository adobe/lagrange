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
#include <lagrange/ui/RenderPipeline.h>
#include <lagrange/ui/RenderResourceBuilder.h>
#include <lagrange/utils/la_assert.h>

namespace lagrange {
namespace ui {

RenderPassBase* RenderPipeline::add_pass(std::unique_ptr<RenderPassBase> pass)
{
    assert(pass);
    m_name_to_pass[pass->get_name()] = pass.get();
    m_passes.push_back(std::move(pass));
    reset();
    return m_passes.back().get();
}

bool RenderPipeline::remove(RenderPassBase* pass)
{
    auto it = std::find_if(m_passes.begin(), m_passes.end(), [=](auto& ptr) {
        return ptr.get() == pass;
    });
    if (it == m_passes.end()) return false;

    m_passes.erase(it);
    reset();
    return true;
}

void RenderPipeline::initialize()
{
    RenderResourceBuilder builder;
    for (auto& pass : m_passes) {
        builder.set_pass(pass.get());
        pass->setup(builder);
    }

    m_initialized = true;
    m_resource_deps = builder.compile();
}

void RenderPipeline::execute()
{
    if (!m_initialized) initialize();

    if (m_custom_execution) {
        m_custom_execution(*this);
        return;
    }

    for (auto& pass : m_passes) {
        if (!pass->is_enabled()) continue;

        if (pass->is_one_shot() && was_one_shot_executed(pass.get())) continue;

        pass->execute();

        if (pass->is_one_shot()) set_one_shot_executed(pass.get());
    }
}

const std::vector<std::unique_ptr<RenderPassBase>>& RenderPipeline::get_passes() const
{
    return m_passes;
}

std::vector<std::unique_ptr<RenderPassBase>>& RenderPipeline::get_passes()
{
    return m_passes;
}

const RenderPassBase* RenderPipeline::get_pass(const std::string& name) const
{
    auto it = m_name_to_pass.find(name);
    if (it == m_name_to_pass.end()) return nullptr;
    return it->second;
}

RenderPassBase* RenderPipeline::get_pass(const std::string& name)
{
    auto it = m_name_to_pass.find(name);
    if (it == m_name_to_pass.end()) return nullptr;
    return it->second;
}

void RenderPipeline::reset()
{
    m_resource_deps = {};
    m_one_shot_passes.clear();
    m_initialized = false;
}

std::unordered_set<RenderPassBase*> RenderPipeline::get_dependencies(
    const std::set<RenderPassBase*>& selection)
{
    // Compute mapping from writer/reader to resources
    std::unordered_map<RenderPassBase*, std::vector<std::shared_ptr<BaseResourceData>>>
        writer_to_resource;
    std::unordered_map<RenderPassBase*, std::vector<std::shared_ptr<BaseResourceData>>>
        reader_to_resource;
    for (auto& id : m_resource_deps.resources) {
        for (auto writer : m_resource_deps.writes[id]) {
            writer_to_resource[writer].push_back(id);
        }
        for (auto reader : m_resource_deps.reads[id]) {
            reader_to_resource[reader].push_back(id);
        }
    }

    // Find all resources that are "read"
    std::unordered_set<std::shared_ptr<BaseResourceData>> needed_resources;
    for (auto& id : m_resource_deps.resources) {
        for (auto reader : m_resource_deps.reads[id]) {
            if (selection.count(reinterpret_cast<RenderPassBase*>(reader)) > 0) {
                needed_resources.insert(id);
                break;
            }
        }
    }

    std::unordered_set<RenderPassBase*> needed_writers;
    std::function<void(std::shared_ptr<BaseResourceData>)> recursive_writer_find;
    recursive_writer_find = [&](const std::shared_ptr<BaseResourceData>& id) {
        for (auto writer : m_resource_deps.writes[id]) {
            // Visited or not
            if (needed_writers.count(writer) > 0) continue;
            needed_writers.insert(writer);

            for (auto it : reader_to_resource[writer]) {
                recursive_writer_find(it);
            }
        }
    };

    // Recursively find all writers to needed reads
    for (auto res : needed_resources) {
        recursive_writer_find(res);
    }

    std::unordered_set<RenderPassBase*> pass_dependencies;

    for (auto& pass : m_passes) {
        if (needed_writers.count(pass.get())) pass_dependencies.insert(pass.get());
    }

    return pass_dependencies;
}

std::unordered_set<RenderPassBase*> RenderPipeline::enable_with_dependencies(
    const std::set<RenderPassBase*>& selection)
{
    const auto pass_dependencies = get_dependencies(selection);

    // Enable passes if in selection or are a dependency
    for (auto& pass : m_passes) {
        pass->set_enabled(selection.count(pass.get()) > 0 || pass_dependencies.count(pass.get()));
    }
    return pass_dependencies;
}

void RenderPipeline::set_custom_execution(std::function<void(RenderPipeline&)> fn)
{
    m_custom_execution = std::move(fn);
}

void RenderPipeline::set_one_shot_executed(RenderPassBase* pass)
{
    LA_ASSERT(pass && pass->is_one_shot(), "Pass is not one shot");
    m_one_shot_passes.insert(pass);
}

bool RenderPipeline::was_one_shot_executed(RenderPassBase* pass) const
{
    LA_ASSERT(pass && pass->is_one_shot(), "Pass is not one shot");
    return (m_one_shot_passes.find(pass) != m_one_shot_passes.end());
}

} // namespace ui
} // namespace lagrange
