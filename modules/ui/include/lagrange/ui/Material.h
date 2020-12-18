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

#include <lagrange/ui/BaseObject.h>
#include <lagrange/ui/Color.h>
#include <lagrange/ui/Resource.h>
#include <lagrange/ui/Texture.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace lagrange {
namespace ui {

class Texture;

/*
    Currently supports only
        MATERIAL_ADOBE_STANDARD
    Individual material maps:
        "baseColor"
        "normal" (must be texture)
        "opacity"
        "roughness"
        "metallic"
        "glow" (unused)
        "translucence" (unused)
        "indexOfRefraction" (unused)
        "density" (unused)
        "interiorColor" (unused)
        "height" (unused)
        "heightScale" (unused)
    Set value by
        material["baseColor"].value = Color(1,0,0,1); //red
        material["opacity"].value = Color(0.75f,0,0,0); // 75% opacity
    Set texture by
        material["normal"].texture = std::make_shared<Texture>("normal_texture.jpg");

*/

class Material : public BaseObject {
public:
    enum Type { MATERIAL_ADOBE_STANDARD, MATERIAL_PHONG, MATERIAL_CUSTOM };

    // Creates empty custom material
    Material();

    static Material create_default(Type type = Type::MATERIAL_ADOBE_STANDARD);
    static std::shared_ptr<Material> create_default_shared(
        Type type = Type::MATERIAL_ADOBE_STANDARD);

    struct Map {
        Resource<Texture> texture;
        Color value; // Used if there is no texture
    };

    const Map& operator[](const std::string& name) const;

    Map& operator[](const std::string& name);

    bool has_map(const std::string& name) const;

    bool is_valid() const;

    bool convert_to(Type new_type);

    Type get_type() const { return m_type; }

    void set_name(const std::string& name) { m_name = name; }
    std::string get_name() const { return m_name; }

    ~Material();

    std::unordered_map<std::string, Map>& get_maps() { return m_maps; }

private:
    Type m_type;
    std::unordered_map<std::string, Map> m_maps;
    std::string m_name;
};

using MaterialLibrary = std::unordered_map<std::string, std::shared_ptr<Material>>;

} // namespace ui
} // namespace lagrange
