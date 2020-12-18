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
#include <lagrange/ui/Model.h>
#include <lagrange/utils/la_assert.h>

namespace lagrange {
namespace ui {

Model::Model(const std::string& name /*= ""*/)
    : m_name(name)
    , m_visible(true)
    , m_transform(Eigen::Affine3f::Identity())
{
    // Propagate selection callback
    m_element_selection.get_persistent().add_callback<Selection<unsigned int>::OnChange>(
        [&](Selection<unsigned int>&) {
            m_callbacks.call<OnSelectionChange>(*this, true, m_element_selection.get_type());
        });
    m_element_selection.get_transient().add_callback<Selection<unsigned int>::OnChange>(
        [&](Selection<unsigned int>&) {
            m_callbacks.call<OnSelectionChange>(*this, false, m_element_selection.get_type());
        });
}


const std::string& Model::get_name() const
{
    return m_name;
}

void Model::set_name(const std::string& name)
{
    m_name = name;
}

bool Model::is_visible() const
{
    return m_visible;
}

void Model::set_visible(bool val)
{
    m_visible = val;
}


Material& Model::get_material(int material_id) const
{
    if (material_id == -1) {
        if (num_materials() == 1)
            return *m_materials.begin()->second; // return the only one
        else
            LA_ASSERT(false, "Access specific material");
    }

    return *m_materials.at(material_id);
}

std::unordered_map<int, Resource<Material>>& Model::get_materials()
{
    return m_materials;
}

const std::unordered_map<int, Resource<Material>>& Model::get_materials() const
{
    return m_materials;
}

bool Model::has_material(int material_id) const
{
    auto it = m_materials.find(material_id);
    if (it == m_materials.end()) return false;

    return it->second;
}

int Model::num_materials() const
{
    return static_cast<int>(m_materials.size());
}

bool Model::set_material(std::shared_ptr<Material> mat, int material_id)
{
    return set_material(Resource<Material>::create(mat), material_id);
}

bool Model::set_material(Resource<Material> mat, int material_id)
{
    auto it = m_materials.find(material_id);
    if (it != m_materials.end()) return false;

    auto default_it = m_materials.find(-1);
    if (material_id != -1 && default_it != m_materials.end()) {
        m_materials.erase(default_it);
    }

    m_materials[material_id] = std::move(mat);
    return true;
}

void Model::trigger_change()
{
    m_callbacks.call<OnChange>(*this);
}

ElementSelection& Model::get_selection()
{
    return m_element_selection;
}

const ElementSelection& Model::get_selection() const
{
    return m_element_selection;
}

std::vector<BaseObject*> Model::get_selection_subtree()
{
    std::vector<BaseObject*> arr;
    arr.reserve(1 + m_materials.size());
    arr.push_back(this);
    for (auto& mat : m_materials) arr.push_back(mat.second.get());
    return arr;
}

Model::~Model()
{
    m_callbacks.call<OnDestroy>(*this);
}

void Model::set_transform(const Eigen::Affine3f& T)
{
    m_transform = T;
}

void Model::apply_transform(const Eigen::Affine3f& T)
{
    set_transform(T * m_transform);
}


void Model::set_viewport_transform(const Camera::ViewportTransform& vt)
{
    m_viewport_transform = vt;
}

const Camera::ViewportTransform& Model::get_viewport_transform() const
{
    return m_viewport_transform;
}


Frustum Model::get_transformed_frustum(
    const Camera& cam, Eigen::Vector2f begin, Eigen::Vector2f end, bool& is_visible) const
{
    const auto& vt = get_viewport_transform();
    const auto model_begin = cam.inverse_viewport_transform(vt, begin);
    const auto model_end = cam.inverse_viewport_transform(vt, end);
    if (vt.clip) {
        if (!cam.intersects_region(model_begin, model_end)) {
            is_visible = false;
        }
    } else {
        is_visible = true;
    }
    return cam.get_frustum(model_begin, model_end);
}

Eigen::Vector2f Model::get_transformed_pixel(
    const Camera& cam, Eigen::Vector2f pixel, bool& is_visible) const
{
    auto model_pixel = cam.inverse_viewport_transform(get_viewport_transform(), pixel);
    if (get_viewport_transform().clip && !cam.is_pixel_in(model_pixel)) {
        is_visible = false;
    } else {
        is_visible = true;
    }
    return model_pixel;
}

Eigen::Affine3f Model::get_transform() const
{
    return m_transform;
}

Eigen::Affine3f Model::get_inverse_transform() const
{
    return m_transform.inverse();
}

} // namespace ui
} // namespace lagrange
