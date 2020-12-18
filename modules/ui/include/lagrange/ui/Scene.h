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
#include <lagrange/ui/Callbacks.h>
#include <lagrange/ui/Emitter.h>
#include <lagrange/ui/Model.h>

#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

namespace lagrange {
namespace ui {
class Camera;

class Scene : public CallbacksBase<Scene> {
public:
    using ModelContainer = std::vector<std::unique_ptr<Model>>;
    using EmitterContainer = std::vector<std::unique_ptr<Emitter>>;


    using OnModelAdd = UI_CALLBACK(void(Model&));
    using OnModelRemove = UI_CALLBACK(void(Model&));
    using OnEmitterAdd = UI_CALLBACK(void(Emitter&));
    using OnEmitterRemove = UI_CALLBACK(void(Emitter&));

    using OnUpdate = UI_CALLBACK(void(Scene&, double dt));

    Scene();
    ~Scene();

    template <typename T>
    T* add_model(std::unique_ptr<T> object)
    {
        return add_model(to_shared_ptr(std::move(object)));
    }

    template <typename T>
    T* add_model(std::shared_ptr<T> object)
    {
        auto ptr = add(object);
        m_models.push_back(ptr);
        m_callbacks.call<OnModelAdd>(*ptr);
        return ptr;
    }

    template <typename T>
    std::vector<T*> add_models(std::vector<std::unique_ptr<T>>&& new_models)
    {
        std::vector<T*> handles;
        for (std::unique_ptr<T>& uptr : new_models) {
            handles.push_back(add_model(std::move(uptr)));
        }
        return handles;
    }

    template <typename T>
    std::vector<T*> add_models(std::vector<std::unique_ptr<T>> & new_models)
    {
        return add_models(std::move(new_models));
    }


    template <typename T>
    T* add_emitter(std::unique_ptr<T> object)
    {
        return add_emitter(to_shared_ptr(std::move(object)));
    }

    template <typename T>
    T* add_emitter(std::shared_ptr<T> object)
    {
        auto ptr = add(object);
        m_emitters.push_back(ptr);
        return ptr;
    }


    bool remove_model(Model* model);
    bool remove_emitter(Emitter* emitter);

    void clear_models();
    void clear_emitters();
    void clear_emitters(Emitter::Type type);
    void clear();


    const std::vector<Model*>& get_models() const;
    const std::vector<Emitter*>& get_emitters() const;

    std::unordered_map<DataGUID, std::vector<Model*>> get_instances() const;

    void update(double dt);

    /*
        Raycasting in world space
    */
    Model* get_model_at(Eigen::Vector3f ray_origin, Eigen::Vector3f ray_dir);
    std::vector<Model*> get_models_in_frustum(const Frustum& f);

    /*
        Raycasting from screen space (applies model's viewport transforms)
    */
    Model* get_model_at(const Camera & camera, Eigen::Vector2f pixel);
    std::vector<Model*> get_models_in_region(const Camera& camera, Eigen::Vector2f begin, Eigen::Vector2f end);


    AABB get_bounds() const;

    float get_nearest_bounds_distance(const Eigen::Vector3f& from) const;
    float get_furthest_bounds_distance(const Eigen::Vector3f& from) const;

private:
    template <typename T>
    T* add(std::shared_ptr<T> object)
    {
        auto ptr = object.get();
        m_objects.push_back(object);
        return ptr;
    }

    bool remove(BaseObject* object);

    // To be replaced by scene tree in the future
    std::vector<std::shared_ptr<BaseObject>> m_objects;

    std::vector<Model*> m_models;
    std::vector<Emitter*> m_emitters;

    Callbacks<OnModelAdd, OnModelRemove, OnEmitterAdd, OnEmitterRemove, OnUpdate> m_callbacks;

    friend CallbacksBase<Scene>;
};

} // namespace ui
} // namespace lagrange
