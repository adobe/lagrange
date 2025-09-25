/*
 * Copyright 2023 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/scene/scene_utils.h>

#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/foreach_attribute.h>
#include <lagrange/scene/api.h>
#include <lagrange/utils/Error.h>
#include <lagrange/utils/assert.h>

namespace lagrange::scene::utils {

template <typename Scalar, typename Index>
void convert_texcoord_uv_st(SurfaceMesh<Scalar, Index>& mesh, AttributeId attribute_id)
{
    seq_foreach_named_attribute_write(mesh, [&](std::string_view name, auto&& attr) -> void {
        using AttributeType = std::decay_t<decltype(attr)>;
        AttributeId current_attribute_id = mesh.get_attribute_id(name);

        if (attribute_id == invalid<AttributeId>()) {
            // return if the attribute is not UV
            if (attr.get_usage() != AttributeUsage::UV) return;
        } else {
            // return if the attribute is not the specified one
            if (attribute_id != current_attribute_id) return;
        }

        Attribute<typename AttributeType::ValueType>* values = nullptr;
        if constexpr (AttributeType::IsIndexed) {
            values = &attr.values();
        } else {
            values = &attr;
        }

        la_runtime_assert(attr.get_num_channels() == 2);
        for (size_t i = 0; i < values->get_num_elements(); ++i) {
            auto vt = values->ref_row(i);
            vt[1] = 1 - vt[1];
        }
    });
}

namespace {

// https://learnopengl.com/Getting-started/Camera
// https://registry.khronos.org/OpenGL-Refpages/gl2.1/xhtml/gluLookAt.xml
Eigen::Affine3d
look_at(const Eigen::Vector3d& eye, const Eigen::Vector3d& center, const Eigen::Vector3d& local_up)
{
    const Eigen::Vector3d forward = (center - eye).normalized();
    const Eigen::Vector3d right = forward.cross(local_up).normalized();
    const Eigen::Vector3d upwards = right.cross(forward);

    Eigen::Affine3d V = Eigen::Affine3d::Identity();
    V.linear() << right, upwards, -forward;
    V.translation() << -right.dot(eye), -upwards.dot(eye), forward.dot(eye);
    return V;
}

// https://www.songho.ca/opengl/gl_projectionmatrix.html#fov
// https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#finite-perspective-projection
Eigen::Projective3d finite_perspective(double fovy, double aspect, double z_near, double z_far)
{
    const double tan_half_fovy = std::tan(fovy / 2.0);

    Eigen::Matrix4d result = Eigen::Matrix4d::Zero();

    result(0, 0) = 1.0 / (aspect * tan_half_fovy);
    result(1, 1) = 1.0 / (tan_half_fovy);
    result(2, 2) = -(z_far + z_near) / (z_far - z_near);
    result(3, 2) = -1.0;
    result(2, 3) = -(2.0 * z_far * z_near) / (z_far - z_near);

    return Eigen::Projective3d(result);
}

// https://www.songho.ca/opengl/gl_projectionmatrix.html#infinite
// https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#infinite-perspective-projection
Eigen::Projective3d infinite_perspective(double fovy, double aspect, double z_near)
{
    const double tan_half_fovy = std::tan(fovy / 2.0);

    Eigen::Matrix4d result = Eigen::Matrix4d::Zero();

    result(0, 0) = 1.0 / (aspect * tan_half_fovy);
    result(1, 1) = 1.0 / (tan_half_fovy);
    result(2, 2) = -1.0;
    result(3, 2) = -1.0;
    result(2, 3) = -2.0 * z_near;

    return Eigen::Projective3d(result);
}

// https://www.songho.ca/opengl/gl_projectionmatrix.html#ortho
// https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#orthographic-projection
Eigen::Projective3d
ortho(double left, double right, double bottom, double top, double z_near, double z_far)
{
    Eigen::Matrix4d result = Eigen::Matrix4d::Identity();
    result(0, 0) = 2.0 / (right - left);
    result(1, 1) = 2.0 / (top - bottom);
    result(2, 2) = -2.0 / (z_far - z_near);
    result(0, 3) = -(right + left) / (right - left);
    result(1, 3) = -(top + bottom) / (top - bottom);
    result(2, 3) = -(z_far + z_near) / (z_far - z_near);
    return Eigen::Projective3d(result);
}

} // namespace

Eigen::Affine3f camera_view_transform(const Camera& camera, const Eigen::Affine3f& world_from_local)
{
    Eigen::Affine3d camera_from_local = look_at(
        camera.position.cast<double>(),
        camera.look_at.cast<double>(),
        camera.up.cast<double>());
    return (camera_from_local * world_from_local.cast<double>().inverse()).cast<float>();
}

Eigen::Projective3f camera_projection_transform(const Camera& camera)
{
    const double near = camera.near_plane;

    if (camera.type == Camera::Type::Perspective) {
        if (camera.far_plane.has_value()) {
            const double far = camera.far_plane.value();
            return finite_perspective(camera.get_vertical_fov(), camera.aspect_ratio, near, far)
                .cast<float>();
        } else {
            return infinite_perspective(camera.get_vertical_fov(), camera.aspect_ratio, near)
                .cast<float>();
        }
    } else if (camera.type == Camera::Type::Orthographic) {
        const double w = camera.orthographic_width;
        const double h = w / camera.aspect_ratio;
        const double far = camera.far_plane.value();
        return ortho(w / -2.0, w / 2.0, h / -2.0, h / 2.0, near, far).cast<float>();
    } else {
        throw Error("Unrecognized camera type");
    }
}

#define LA_X_convert_texcoord_uv_st(_, S, I)           \
    template LA_SCENE_API void convert_texcoord_uv_st( \
        SurfaceMesh<S, I>& mesh,                       \
        AttributeId attribute_id);
LA_SURFACE_MESH_X(convert_texcoord_uv_st, 0)
#undef LA_X_convert_texcoord_uv_st

} // namespace lagrange::scene::utils
