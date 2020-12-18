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
#include <lagrange/ui/Scene.h>

namespace lagrange {
namespace ui {

Scene::Scene() {}

Scene::~Scene() {}

bool Scene::remove_model(Model* model)
{
    auto it = std::find(m_models.begin(), m_models.end(), model);
    if (it == m_models.end()) return false;

    m_callbacks.call<OnModelRemove>(*model);

    m_models.erase(it);
    remove(model);
    return true;
}

bool Scene::remove_emitter(Emitter* emitter)
{
    auto it = std::find(m_emitters.begin(), m_emitters.end(), emitter);
    if (it == m_emitters.end()) return false;

    m_callbacks.call<OnEmitterRemove>(*emitter);

    m_emitters.erase(it);
    remove(emitter);
    return true;
}

void Scene::clear_models()
{
    for (auto model : m_models) {
        m_callbacks.call<OnModelRemove>(*model);
        remove(model);
    }
    m_models.clear();
}

void Scene::clear_emitters()
{
    for (auto emitter : m_emitters) {
        m_callbacks.call<OnEmitterRemove>(*emitter);
        remove(emitter);
    }
    m_emitters.clear();
}

void Scene::clear_emitters(Emitter::Type type)
{
    for (auto emitter : m_emitters) {
        if (emitter->get_type() != type) continue;

        m_callbacks.call<OnEmitterRemove>(*emitter);
        remove(emitter);
    }
    m_emitters.clear();
}

void Scene::clear()
{
    clear_models();
    clear_emitters();
}

const std::vector<Model*>& Scene::get_models() const
{
    return m_models;
}

std::unordered_map<DataGUID, std::vector<Model*>> Scene::get_instances() const
{
    std::unordered_map<DataGUID, std::vector<Model*>> result;

    for (auto& model : m_models) {
        result[model->get_data_guid()].push_back(model);
    }

    return result;
}


void Scene::update(double dt)
{
    m_callbacks.call<OnUpdate>(*this, dt);
}


Model* Scene::get_model_at(Eigen::Vector3f ray_origin, Eigen::Vector3f ray_dir)
{
    float min_t = std::numeric_limits<float>::max();
    Model* min_model = nullptr;
    for (auto& model : m_models) {
        if (!model->is_selectable()) continue;

        float bbtmin = std::numeric_limits<float>::max();
        if (model->intersects(ray_origin, ray_dir, bbtmin)) {
            if (bbtmin < min_t) {
                min_t = bbtmin;
                min_model = model;
            }
        }
    }

    return min_model;
}

Model* Scene::get_model_at(const Camera& camera, Eigen::Vector2f pixel)
{
    float min_t = std::numeric_limits<float>::max();
    Model* min_model = nullptr;
    for (auto& model : m_models) {
        if (!model->is_selectable()) continue;

        auto model_pixel =
            camera.inverse_viewport_transform(model->get_viewport_transform(), pixel);

        if (model->get_viewport_transform().clip) {
            if (!camera.is_pixel_in(model_pixel)) continue;
        }

        auto ray = camera.cast_ray(model_pixel);
        float bbtmin = std::numeric_limits<float>::max();
        if (model->intersects(ray.origin, ray.dir, bbtmin)) {
            if (bbtmin < min_t) {
                min_t = bbtmin;
                min_model = model;
            }
        }
    }
    return min_model;
}

std::vector<Model*> Scene::get_models_in_frustum(const Frustum& f)
{
    std::vector<Model*> result;
    for (auto& model : m_models) {
        if (!model->is_selectable()) continue;

        if (model->intersects(f)) result.push_back(model);
    }
    return result;
}

std::vector<Model*> Scene::get_models_in_region(const Camera& camera,
    Eigen::Vector2f begin,
    Eigen::Vector2f end)
{
    std::vector<Model*> result;


    for (auto& model : m_models) {
        if (!model->is_selectable()) continue;

        const auto model_begin =
            camera.inverse_viewport_transform(model->get_viewport_transform(), begin);
        const auto model_end =
            camera.inverse_viewport_transform(model->get_viewport_transform(), end);

        if (model->get_viewport_transform().clip) {
            if (!camera.intersects_region(model_begin, model_end)) continue;
        }

        const auto f = camera.get_frustum(model_begin, model_end);

        if (model->intersects(f)) result.push_back(model);
    }
    return result;
}

AABB Scene::get_bounds() const {
    Eigen::AlignedBox3f bb;
    for (const auto model : m_models) {
        if (model->is_visualizable())
            bb.extend(model->get_bounds());
    }
    return bb;
}

float Scene::get_nearest_bounds_distance(const Eigen::Vector3f& from) const
{
    float min_dst = std::numeric_limits<float>::max();

    for (const auto model : m_models) {
        const auto& bb = model->get_bounds();
        Eigen::Vector3f dmin = (from - bb.min()).cwiseAbs();
        Eigen::Vector3f dmax = (from - bb.max()).cwiseAbs();
        Eigen::Vector3f dminmin = (dmin).cwiseMin(dmax);
        min_dst = std::min(
            min_dst, dminmin.minCoeff());
    }

    return min_dst;
}

float Scene::get_furthest_bounds_distance(const Eigen::Vector3f& from) const
{
    float max_dst = std::numeric_limits<float>::min();

    for (const auto model : m_models) {
        if (!model->is_visualizable()) continue;
        const auto& bb = model->get_bounds();
        Eigen::Vector3f dmin = (from - bb.min()).cwiseAbs();
        Eigen::Vector3f dmax = (from - bb.max()).cwiseAbs();
        Eigen::Vector3f dmaxmax = dmin.cwiseMax(dmax);
        max_dst = std::max(max_dst, dmaxmax.maxCoeff());
    }

    return max_dst;
}

bool Scene::remove(BaseObject* object)
{
    auto it = std::find_if(m_objects.begin(),
        m_objects.end(),
        [=](const std::shared_ptr<BaseObject>& ptr) { return ptr.get() == object; });
    if (it == m_objects.end()) return false;

    m_objects.erase(it);
    return true;
}

const std::vector<Emitter*>& Scene::get_emitters() const
{
    return m_emitters;
}


} // namespace ui
} // namespace lagrange
