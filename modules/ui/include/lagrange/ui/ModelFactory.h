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

#include <lagrange/io/load_mesh_ext.h>

#include <lagrange/ui/Callbacks.h>
#include <lagrange/ui/Material.h>
#include <lagrange/ui/MeshModel.h>
#include <lagrange/ui/Model.h>

#include <lagrange/ui/MDL.h>
#include <lagrange/ui/Material.h>
#include <lagrange/ui/Texture.h>
#include <lagrange/ui/default_resources.h>


namespace lagrange {
namespace ui {

/*
    Static factory for loading/saving/creating models
*/
class ModelFactory
{
public:
    using OnModelLoad = UI_CALLBACK(void(Model&));
    using OnModelSave = UI_CALLBACK(void(Model&));

    template <typename CallbackType>
    static void add_callback(typename CallbackType::FunType&& fun)
    {
        m_callbacks.add<CallbackType>(CallbackType{fun});
    }

    template <typename MeshType>
    static std::vector<std::unique_ptr<MeshModel<MeshType>>> load_obj(
        const fs::path& file_path, bool normalize)
    {
        io::MeshLoaderParams params;
        params.normalize = normalize;
        return load_obj<MeshType>(file_path, params);
    }


    static io::MeshLoaderParams default_ui_meshloaderparams()
    {
        io::MeshLoaderParams p;
        p.normalize = true;
        return p;
    }

    template <typename MeshType>
    static std::vector<std::unique_ptr<MeshModel<MeshType>>> load_obj(const fs::path& file_path,
        const io::MeshLoaderParams& params = ModelFactory::default_ui_meshloaderparams())
    {
        using V = typename MeshType::VertexArray;
        using F = typename MeshType::FacetArray;
        auto obj = Resource<ObjResult<V, F>>::create(file_path.string(), params);

        std::vector<std::unique_ptr<MeshModel<MeshType>>> result;

        for (size_t i = 0; i < obj->meshes.size(); i++) {

            auto mesh = obj->meshes[i];
            const auto& mats = obj->mesh_to_material[i];

            auto mm = std::make_unique<MeshModel<MeshType>>(mesh, string_format("{}[{}]", file_path.stem(), i));

            for (auto & mat_and_id : mats) {
                mm->set_material(mat_and_id.first.data()->data(), mat_and_id.second);
            }

            m_callbacks.call<OnModelLoad>(*mm);

            result.push_back(std::move(mm));

        }

        return result;
    }

    static std::shared_ptr<Material> convert_material(
        const fs::path& base_dir, const tinyobj::material_t& tinymat);

    template <typename T>
    struct proxy_false : std::false_type
    {
    };


    template <class MeshType>
    static std::unique_ptr<MeshModel<MeshType>> make(MeshType&& m,
        const std::string& name = "Unnamed",
        std::shared_ptr<Material> material = Material::create_default_shared())
    {
        auto model =
            std::make_unique<MeshModel<MeshType>>(std::make_shared<MeshType>(std::move(m)), name);

        model->set_material(material, -1);
        return model;
    }

    template <class MeshType>
    static std::unique_ptr<MeshModel<MeshType>> make(std::unique_ptr<MeshType>&& m,
        const std::string& name = "Unnamed",
        std::shared_ptr<Material> material = Material::create_default_shared())
    {
        auto model = std::make_unique<MeshModel<MeshType>>(to_shared_ptr(std::move(m)), name);
        model->set_material(material, -1);
        if (material->get_name() == "") material->set_name(name);
        return model;
    }

    template <class MeshType>
    static std::unique_ptr<MeshModel<MeshType>> make(const std::unique_ptr<MeshType>& m,
        const std::string& name = "Unnamed",
        std::shared_ptr<Material> material = Material::create_default_shared())
    {
        static_assert(
            proxy_false<MeshType>::value, "Use std::move or use std::shared_ptr to make model.");
        return nullptr;
    }

    template <class MeshType>
    static std::unique_ptr<MeshModel<MeshType>> make(std::unique_ptr<MeshType>& m,
        const std::string& name = "Unnamed",
        std::shared_ptr<Material> material = Material::create_default_shared())
    {
        static_assert(
            proxy_false<MeshType>::value, "Use std::move or use std::shared_ptr to make model.");
        return nullptr;
    }

    template <class MeshType>
    static std::unique_ptr<MeshModel<MeshType>> make(std::shared_ptr<MeshType> m,
        const std::string& name = "Unnamed",
        std::shared_ptr<Material> material = Material::create_default_shared())
    {
        auto model = std::make_unique<MeshModel<MeshType>>(std::move(m), name);
        model->set_material(material, -1);
        if (material->get_name() == "") material->set_name(name);
        return model;
    }

    [[deprecated("Use Viewer::enable_ground() and Viewer::get_ground() instead for infinite "
                 "ground plane and grid")]]
    static std::unique_ptr<MeshModel<QuadMesh3Df>>
    make_ground_plane(float height = 0.0f,
        float extent = 9999.0f,
        const std::string& name = "Ground Plane",
        std::shared_ptr<Material> material = Material::create_default_shared())
    {
        Quads F(1, 4);
        F << 0, 1, 2, 3;

        Vertices3Df V(4, 3);
        V.row(0) << -1.0f, 0, -1.0f;
        V.row(1) << -1.0f, 0, 1.0f;
        V.row(2) << 1.0f, 0, 1.0f;
        V.row(3) << 1.0f, 0, -1.0f;

        Eigen::Matrix<float, Eigen::Dynamic, 2, Eigen::RowMajor> UV(4, 2);

        UV.row(0) = Eigen::Vector2f{0, 0};
        UV.row(1) = Eigen::Vector2f{1, 0};
        UV.row(2) = Eigen::Vector2f{1, 1};
        UV.row(3) = Eigen::Vector2f{0, 1};

        auto lg_mesh = lagrange::create_mesh(std::move(V), std::move(F));
        lg_mesh->initialize_uv(UV, lg_mesh->get_facets());

        auto model = make(std::move(lg_mesh), name, material);
        model->set_selectable(false);
        model->set_visualizable(false);
        model->set_transform(Eigen::Translation3f(0,height,0) * Eigen::Scaling(extent,1.0f, extent));
        return model;
    }

private:
    static Callbacks<OnModelLoad, OnModelSave> m_callbacks;
};


} // namespace ui
} // namespace lagrange
