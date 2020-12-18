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
#include <memory>

#include <lagrange/ui/Emitter.h>
#include <lagrange/ui/Texture.h>
#include <lagrange/ui/Resource.h>

#include <lagrange/fs/filesystem.h>

namespace lagrange {
namespace ui {

class IBL : public Emitter{

public:

    Emitter::Type get_type() const;

    /*
        Supports .ibl files
    */
    IBL(const fs::path& file_path);

    IBL(const std::string & name, Resource<Texture> bg_texture);

    Texture& get_background() { return *m_background; }
    Texture& get_diffuse() { return *m_diffuse; }
    Texture& get_specular() { return *m_specular; }

    const Texture& get_background() const { return *m_background; }
    const Texture& get_diffuse() const { return *m_diffuse; }
    const Texture& get_specular() const { return *m_specular; }

    Texture& get_background_rect() { return *m_background_rect; }
    const Texture& get_background_rect() const { return *m_background_rect; }

    bool save_to(const fs::path& file_path);

    const std::string& get_name() const { return m_name; }

private:

    void generate_textures(const Texture& input_tex);

    Resource<Texture> get_bg_texture_ibl_file(
        const fs::path& ibl_file_path
    );

    Resource<Texture> m_background_rect;
    Resource<Texture> m_background;
    Resource<Texture> m_diffuse;
    Resource<Texture> m_specular;
    std::string m_name;
    fs::path m_file_path;
};

}
}
