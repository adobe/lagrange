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
#include <string>
#include <unordered_map>

#include <lagrange/ui/Material.h>
#include <lagrange/fs/filesystem.h>

namespace lagrange {
namespace ui {

struct MDLImpl;

class MDL {

public:

    MDL();

    ~MDL();

    static MDL & get_instance();

    struct Library
    {
        std::unordered_map<std::string, std::shared_ptr<Material>> materials;
    };
    
    
    Library load_materials(
        const fs::path& base_dir, 
        const std::string & module_name
    );

private:
    static std::unique_ptr<MDL> m_instance;
    std::unique_ptr<MDLImpl> m_impl;
};

}
}
