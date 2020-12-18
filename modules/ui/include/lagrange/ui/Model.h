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

#include <lagrange/utils/la_assert.h>
#include <lagrange/ui/AABB.h>
#include <lagrange/ui/BaseObject.h>
#include <lagrange/ui/Callbacks.h>
#include <lagrange/ui/Camera.h>
#include <lagrange/ui/FunctionUtils.h>
#include <lagrange/ui/Selection.h>
#include <lagrange/ui/ui_common.h>
#include <lagrange/ui/Resource.h>
#include <lagrange/ui/MeshBuffer.h>

#include <lagrange/Mesh.h>



#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace lagrange {
namespace ui {

class Renderable;
class Material;

template <typename T>
class MeshModel;

using SupportedMeshTypes = std::tuple<MeshModel<Mesh<Vertices3Df, Triangles>>,
    MeshModel<Mesh<Vertices3D, Triangles>>,
    MeshModel<Mesh<Vertices3Df, Quads>>,
    MeshModel<Mesh<Vertices3D, Quads>>,
    MeshModel<Mesh<Vertices2Df, Triangles>>,
    MeshModel<Mesh<Vertices2D, Triangles>>,
    MeshModel<Mesh<Vertices2Df, Quads>>,
    MeshModel<Mesh<Vertices2D, Quads>>,
    MeshModel<Mesh<Eigen::Matrix<float, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>,
        Eigen::Matrix<Triangles::Scalar, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>>>,
    MeshModel<Mesh<Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>,
        Eigen::Matrix<Triangles::Scalar, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>>>>;

using SupportedMeshTypes3D = std::tuple<MeshModel<Mesh<Vertices3Df, Triangles>>,
    MeshModel<Mesh<Vertices3D, Triangles>>,
    MeshModel<Mesh<Vertices3Df, Quads>>,
    MeshModel<Mesh<Vertices3D, Quads>>,
    MeshModel<Mesh<Eigen::Matrix<float, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>,
        Eigen::Matrix<Triangles::Scalar, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>>>,
    MeshModel<Mesh<Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>,
        Eigen::Matrix<Triangles::Scalar, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>>>>;

using SupportedMeshTypes3DTriangle = std::tuple<MeshModel<Mesh<Vertices3Df, Triangles>>,
    MeshModel<Mesh<Vertices3D, Triangles>>,
    MeshModel<Mesh<Eigen::Matrix<float, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>,
        Eigen::Matrix<Triangles::Scalar, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>>>,
    MeshModel<Mesh<Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>,
        Eigen::Matrix<Triangles::Scalar, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>>>>;

using SupportedMeshTypes3DQuad =
    std::tuple<MeshModel<Mesh<Vertices3Df, Quads>>, MeshModel<Mesh<Vertices3D, Quads>>>;

using SupportedMeshTypes2D = std::tuple<MeshModel<Mesh<Vertices2Df, Triangles>>,
    MeshModel<Mesh<Vertices2D, Triangles>>,
    MeshModel<Mesh<Vertices2Df, Quads>>,
    MeshModel<Mesh<Vertices2D, Quads>>,
    MeshModel<Mesh<Eigen::Matrix<float, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>,
        Eigen::Matrix<Triangles::Scalar, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>>>,
    MeshModel<Mesh<Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>,
        Eigen::Matrix<Triangles::Scalar, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>>>>;

struct DataGUID
{
    explicit DataGUID(void* value)
        : ptr(value)
    {}

    bool operator<(const DataGUID& r) const { return ptr < r.ptr; }
    bool operator==(const DataGUID& r) const { return ptr == r.ptr; }
    const void* value() const { return ptr; }

private:
    const void* ptr;
};


class Model : public BaseObject, public CallbacksBase<Model>
{
public:
    using OnChange = UI_CALLBACK(void(Model&));
    using OnSelectionChange = UI_CALLBACK(void(Model&, bool persistent, SelectionElementType type));
    using OnDestroy = UI_CALLBACK(void(Model&));

    Model(const std::string& file_path = "");
    virtual ~Model();

    ///
    /// @brief Get mesh of type MeshType
    ///
    /// \warning Temporary, will be redesigned
    ///
    template <typename MeshType>
    MeshType* mesh()
    {
        auto meshmodel = dynamic_cast<MeshModel<MeshType>*>(this);

        if (meshmodel) {
            if (!meshmodel->has_mesh()) return nullptr;
            return &meshmodel->get_mesh();
        }

        return nullptr;
    }

    ///
    /// @brief Get mesh of type MeshType
    ///
    /// \warning Temporary, will be redesigned
    ///
    template <typename MeshType>
    const MeshType* mesh() const
    {
        auto meshmodel = dynamic_cast<const MeshModel<MeshType>*>(this);

        if (meshmodel) {
            if (!meshmodel->has_mesh()) return nullptr;
            return &meshmodel->get_mesh();
        }
        return nullptr;
    }


    ///
    /// @brief Visit supported mesh types
    ///
    /// Example:
    /// \code
    ///         model.visit([](auto & mesh){ mesh.get_vertices(); })
    /// \endcode
    /// \warning Temporary, will be redesigned
    ///
    template <typename F>
    void visit_mesh(F&& fn)
    {
        visit_tuple<SupportedMeshTypes>([&](auto& mesh_model) {
            if (mesh_model.has_mesh()) fn(mesh_model.get_mesh());
        });
    }

    ///
    /// @brief Visit supported mesh types
    ///
    /// \code
    ///         model.visit([](const auto & mesh){ mesh.get_vertices(); })
    /// \endcode
    /// \warning Temporary, will be redesigned
    ///
    template <typename F>
    void visit_mesh(F&& fn) const
    {
        visit_tuple_const<SupportedMeshTypes>([&](const auto& mesh_model) {
            if (mesh_model.has_mesh()) fn(mesh_model.get_mesh());
        });
    }

    /**
     * Returns name of the model
     */
    const std::string& get_name() const;

    /**
     * Sets name of the model
     */
    void set_name(const std::string& name);

    /**
     *   Visibility for rendering & selection
     */
    bool is_visible() const;
    void set_visible(bool val);

    /**
     * Returns affine transform
     */
    Eigen::Affine3f get_transform() const;
    Eigen::Affine3f get_inverse_transform() const;

    /**
     * Sets affine transform
     */
    void set_transform(const Eigen::Affine3f& T);

    /**
     * Sets affine transform
     * Handles non-existence of implicit conversions to Affine3f
     */
    template <typename EigenTransform>
    void set_transform(const EigenTransform& T)
    {
        Eigen::Affine3f A;
        A = T;
        set_transform(A);
    }

    /**
     * Applies affine T transform to existing transform (T_new = T_old * T)
     */
    void apply_transform(const Eigen::Affine3f& T);

    /**
     * Applies affine T transform to existing transform (T_new = T_old * T)
     * Handles non-existence of implicit conversions to Affine3f
     */
    template <typename EigenTransform>
    void apply_transform(const EigenTransform & T){
        Eigen::Affine3f A;
        A = T;
        apply_transform(A);
    }

    /**
     * Get axis aligned bounding box of the model
     */
    virtual AABB get_bounds() const = 0;

    /**
     * Frustum intersection test
     */
    virtual bool intersects(const Frustum& f) = 0;

    /**
     * Ray intersection test and distance
     */
    virtual bool intersects(
        const Eigen::Vector3f& ray_origin, const Eigen::Vector3f& ray_dir, float& t_out) = 0;

    /**
     *  Get/Set a viewport transformation (scale/translation)
     *  Values can be between 0 and 1
     */
    void set_viewport_transform(const Camera::ViewportTransform& vt);
    const Camera::ViewportTransform& get_viewport_transform() const;

    /**
     * Returns frustum transformed by model's viewport transform
     */
    Frustum get_transformed_frustum(
        const Camera& cam, Eigen::Vector2f begin, Eigen::Vector2f end, bool& is_visible) const;

    /*
        Returns pixel transformed by model's viewport transform
    */
    Eigen::Vector2f get_transformed_pixel(const Camera& cam, Eigen::Vector2f pixel, bool& is_visible) const;

    /**
     * Gets current material
     * -1 gets default material
     */
    Material& get_material(int material_id = -1) const;

    std::unordered_map<int, Resource<Material>>& get_materials();
    const std::unordered_map<int, Resource<Material>>& get_materials() const;

    bool has_material(int material_id) const;
    int num_materials() const;

    /**
     * Set material
     * Returns false if material_id already exists
     */
    bool set_material(std::shared_ptr<Material> mat, int material_id);
    bool set_material(Resource<Material> mat, int material_id);

    /**
     *  Element (vertex/edge/face) selection
     */
    ElementSelection& get_selection();
    const ElementSelection& get_selection() const;

    virtual AABB get_selection_bounds() const = 0;

    /**
     *  Returns all base objects in the hierarchy below this model
     *  Currently used to get Model + all its materials
     */
    virtual std::vector<BaseObject*> get_selection_subtree() override;


    template <class F>
    bool visit(F f, bool require_type = false)
    {
        return visit_impl<void, typename util::AsFunction<F>::arg_type>(f, require_type);
    }

    template <class F>
    bool visit(F f, bool require_type = false) const
    {
        return visit_impl_const<void, const typename util::AsFunction<F>::arg_type>(
            f, require_type);
    }

    template <class Tuple, class F>
    bool visit_tuple(F f)
    {
        return visit_tuple_impl<F, Tuple, std::tuple_size<Tuple>::value>()(f, this);
    }

    template <class Tuple, class F>
    bool visit_tuple_const(F f) const
    {
        return visit_tuple_impl_const<F, Tuple, std::tuple_size<Tuple>::value>()(f, this);
    }

    template <class... Types, class F>
    bool visit_multiple(F& f, bool require_type = false)
    {
        return visit_multiple_impl<F, Types...>(f, require_type);
    }

    template <class... Types, class F>
    bool visit_multiple_const(F& f, bool require_type = false) const
    {
        return visit_multiple_impl_const<F, Types...>(f, require_type);
    }


    /*
        Returns globally unique identifier of CPU data
        If equal with other model's DataGUID, they are instance of the same data
        with (possibly) different transforms and materials
    */
    virtual DataGUID get_data_guid() const = 0;


    //TODO: this will be replace by a component
    //If not a mesh, return empty resource
    virtual Resource<MeshBuffer> get_buffer() const { return Resource<MeshBuffer>(); }

protected:
    virtual void trigger_change();


    template <class F, class Tuple, size_t I>
    struct visit_tuple_impl
    {
        bool operator()(F f, Model* model)
        {
            using current_type = typename std::tuple_element<I - 1, Tuple>::type;
            return model->visit_multiple<current_type>(f, false) ||
                   visit_tuple_impl<F, Tuple, I - 1>()(f, model);
        }
    };
    template <class F, class Tuple>
    struct visit_tuple_impl<F, Tuple, 0>
    {
        bool operator()(F LGUI_UNUSED(f), Model* LGUI_UNUSED(model)) { return false; }
    };

    template <class F, class Tuple, size_t I>
    struct visit_tuple_impl_const
    {
        bool operator()(F f, const Model* model)
        {
            using current_type = typename std::tuple_element<I - 1, Tuple>::type;
            return model->visit_multiple_const<current_type>(f, false) ||
                   visit_tuple_impl_const<F, Tuple, I - 1>()(f, model);
        }
    };
    template <class F, class Tuple>
    struct visit_tuple_impl_const<F, Tuple, 0>
    {
        bool operator()(F LGUI_UNUSED(f), const Model* LGUI_UNUSED(model)) { return false; }
    };

    template <class F, class Type>
    bool visit_multiple_impl(F f, bool require_type = false)
    {
        return visit_impl<void, Type>(f, require_type);
    }

    template <class F, class Type, class... Types>
    typename std::enable_if<sizeof...(Types) != 0, bool>::type visit_multiple_impl(
        F f, bool require_type = false)
    {
        return visit_impl<void, Type>(f, require_type) ||
               visit_multiple_impl<F, Types...>(f, require_type);
    }

    template <class F, class Type>
    bool visit_multiple_impl_const(F f, bool require_type = false) const
    {
        return visit_impl_const<void, Type>(f, require_type);
    }

    template <class F, class Type, class... Types>
    typename std::enable_if<sizeof...(Types) != 0, bool>::type visit_multiple_impl_const(
        F f, bool require_type = false) const
    {
        return visit_impl_const<void, Type>(f, require_type) ||
               visit_multiple_impl_const<F, Types...>(f, require_type);
    }

    template <class ReturnType, class T>
    bool visit_impl(std::function<ReturnType(T&)> fn, bool require_type)
    {
        auto* ptr = dynamic_cast<typename std::remove_reference<T>::type*>(this);
        if (ptr) {
            fn(*ptr);
            return true;
        }

        LA_ASSERT(!require_type, "Wrong type");
        return false;
    }

    template <class ReturnType, class T>
    bool visit_impl_const(std::function<ReturnType(const T&)> fn, bool require_type) const
    {
        auto* ptr = dynamic_cast<const typename std::remove_reference<T>::type*>(this);
        if (ptr) {
            fn(*ptr);
            return true;
        }

        LA_ASSERT(!require_type, "Wrong type");
        return false;
    }


    Callbacks<OnChange, OnDestroy, OnSelectionChange> m_callbacks;

    std::unordered_map<int, Resource<Material>> m_materials;

    std::string m_name;
    bool m_visible;

    Eigen::Affine3f m_transform;
    Camera::ViewportTransform m_viewport_transform;

    ElementSelection m_element_selection;

    friend CallbacksBase<Model>;
};

} // namespace ui
} // namespace lagrange


namespace std {
template <>
struct hash<lagrange::ui::DataGUID>
{
    size_t operator()(const lagrange::ui::DataGUID& x) const
    {
        return hash<const void*>()(x.value());
    }
};
} // namespace std
