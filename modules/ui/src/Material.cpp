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
#include <lagrange/ui/Material.h>
#include <lagrange/utils/la_assert.h>

namespace lagrange {
namespace ui {

Material::~Material()
{}

Material::Material() :
    m_type(MATERIAL_CUSTOM)
{}

Material Material::create_default(Material::Type type)
{
    Material m;

    switch (type) {
    case MATERIAL_ADOBE_STANDARD:
        m["baseColor"].value = Color{ 186.0f / 255.0f, 186.0f / 255.0f, 186.0f / 255.0f };
        m["glow"].value = Color{ 0.0f };
        m["opacity"].value = Color{ 1.0f };
        m["roughness"].value = Color{ 0.6f };
        m["metallic"].value = Color{ 0.397f };
        m["translucence"].value = Color{ 0.0f };
        m["indexOfRefraction"].value = Color{ 1.6f };
        m["density"].value = Color{ 1.0f };
        m["interiorColor"].value = Color{ 1.0f, 1.0f, 1.0f };
        m["height"].value = Color{ 0.0f };
        m["heightScale"].value = Color{ 1.0f };
        m["normal"].value = Color{ 0.0f,0.0f,0.0f,0.0f };
        break;
    case MATERIAL_PHONG:
        m["ambient"].value = Color{ 0.1f, 0.1f, 0.1f };
        m["diffuse"].value = Color{ 0.814847f, 0.814847f, 0.814847f };
        m["specular"].value = Color{ 0.814847f, 0.814847f, 0.814847f };
        m["shininess"].value = Color{ 1.0f };
        m["bump"].value = Color{ 0.0f,0.0f,0.0f };
        m["displacement"].value = Color{ 0.0f,0.0f,0.0f };
        m["opacity"].value = Color{ 0.0f,0.0f,0.0f };
        break;
    default:
        break;
    }

    m.convert_to(type);
    return m;
}

std::shared_ptr<Material> Material::create_default_shared(Type type /*= Type:::MATERIAL_ADOBE_STANDARD*/)
{
    return std::make_shared<Material>(create_default(type));
}

const Material::Map& Material::operator[](const std::string& name) const
{
    return m_maps.at(name);
}

Material::Map& Material::operator[](const std::string& name)
{
    return m_maps[name];
}

bool Material::has_map(const std::string& name) const
{
    return m_maps.find(name) != m_maps.end();
}

bool Material::is_valid() const
{
    bool ok = true;
 
    switch (m_type)
    {
       
    case MATERIAL_ADOBE_STANDARD:        
        ok &= has_map("translucence");
        ok &= has_map("interiorColor");
        ok &= has_map("indexOfRefraction");
        ok &= has_map("metallic");
        ok &= has_map("baseColor");
        ok &= has_map("roughness");
        ok &= has_map("density");
        ok &= has_map("glow");
        ok &= has_map("opacity");
        ok &= has_map("normal");
        ok &= has_map("height");
        ok &= has_map("heightScale");
        break;
    case MATERIAL_PHONG:
        ok &= has_map("ambient");
        ok &= has_map("diffuse");
        ok &= has_map("specular");
        ok &= has_map("shininess");
        ok &= has_map("bump");
        ok &= has_map("displacement");
        ok &= has_map("opacity");
        break;
    case MATERIAL_CUSTOM:
        break;
    }

    return ok;
}

bool Material::convert_to(Type new_type)
{
    if (new_type == m_type)
        return true;

    //Custom to any -> check validity
    if (m_type == MATERIAL_CUSTOM) {
        m_type = new_type;
        return is_valid();
    }

    //Any to custom, always ok
    if (new_type == MATERIAL_CUSTOM) {
        m_type = new_type;
        return true;
    }

    //Standard to phong
    if (m_type == MATERIAL_ADOBE_STANDARD && new_type == MATERIAL_PHONG) {
        LA_ASSERT("Not implemented");        
    }

    //Phong to standard
    if (m_type == MATERIAL_PHONG && new_type == MATERIAL_ADOBE_STANDARD) {
        LA_ASSERT("Not implemented");
    }

    return false;
}

}
}
