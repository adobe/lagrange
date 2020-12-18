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
#include <memory>
#include <unordered_map>
#include <unordered_set>

#include <lagrange/ui/Options.h>

namespace lagrange {
namespace ui {

class RenderResources;
class RenderResourceBuilder;

class RenderPassBase {
public:

    RenderPassBase(const std::string& name);
    virtual ~RenderPassBase();

    virtual void setup(RenderResourceBuilder& builder) = 0;
    virtual void execute() = 0;

    OptionSet& get_options();
    const OptionSet& get_options() const;

    const std::string& get_name() const;

    bool is_enabled() const;
    void set_enabled(bool value);

    /*
        One shot behaviour - this render pass is
        Executed only once after pipeline is initialized
    */
    bool is_one_shot() const;
    void set_one_shot(bool value);

    /*
        Tags for UI
    */
    void add_tag(const std::string& tag);
    void add_tags(const std::vector<std::string>& tags);
    bool remove_tag(const std::string& tag);
    const std::unordered_set<std::string> & get_tags() const;
    bool has_tag(const std::string & tag) const;

protected:
    const std::string m_name;
    std::unordered_set<std::string> m_tags;
    OptionSet m_options;
    bool m_enabled;
    bool m_one_shot;
   
};

/*
    RenderPass consists of two phases:
        setup()
            called before rendering,
            fills custom PassData with data used 
            in execute phase or used by subsequent render
            passes. Optionally, can add options to OptionSet
            that will be used in the execute phase and can be
            controlled via UI.
            The goal is to set up any render resources needed or
            get handles to existing ones.
        execute()
            performs drawing operations, 
            uses resources defined in PassData,
            optionally controllable by OptionSet configuration
*/

template <typename PassData>
class RenderPass : public RenderPassBase {
public:

    using SetupFunc = std::function<
        void(PassData&, OptionSet&, RenderResourceBuilder &)
    >;

    using ExecuteFunc = std::function<
        void(const PassData&, const OptionSet&)
    >;

    using CleanupFunc = std::function<void(PassData&, OptionSet&)>;

    RenderPass( const std::string & name, 
        SetupFunc&& setup, 
        ExecuteFunc&& execute,
        CleanupFunc && cleanup
    ) : RenderPassBase(name),
        m_setup(std::move(setup)), 
        m_execute(std::move(execute))
    , m_cleanup(std::move(cleanup))
    {
    }

    RenderPass(const std::string& name,
        SetupFunc&& setup,
        ExecuteFunc&& execute,
        const CleanupFunc& cleanup = nullptr)
    : RenderPassBase(name)
    , m_setup(std::move(setup))
    , m_execute(std::move(execute))
    , m_cleanup(cleanup)
    {
    }

    virtual ~RenderPass()
    {
        if (m_cleanup) m_cleanup(m_data, m_options);
    }

    void setup(RenderResourceBuilder & builder) final {
        m_setup(m_data, m_options, builder);
        m_options.trigger_change();
    }

    void execute() final {
        m_execute(m_data, m_options);
    }

    const PassData& get_data() const{
        return m_data;
    }

    PassData& get_data(){
        return m_data;
    }

public:
    SetupFunc m_setup;
    ExecuteFunc m_execute;
    CleanupFunc m_cleanup;

    PassData m_data;

};

}
}
