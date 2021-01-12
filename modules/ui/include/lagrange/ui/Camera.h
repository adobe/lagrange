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
#include <lagrange/ui/Frustum.h>

namespace lagrange {
namespace ui {

///
/// @brief Camera class
///
/// Camera can be either Type::PERSPECTIVE or
/// Type::ORTHOGRAPHIC
///
/// Camera is defined by:
///     position,
///     lookat point or direction,
///     up vector,
///     field of view or ortho_viewport,
///     window dimensions,
///     and far/near plane
///
/// Note: Caches view and perspective matrices and their inverses.
///

class Camera : public CallbacksBase<Camera>
{
public:
    ///
    /// @brief Camera Mode
    ///
    enum class Type { PERSPECTIVE, ORTHOGRAPHIC };

    enum class RotationMode {
        TUMBLE, /// Local view x,y axis
        TURNTABLE, /// Global y, local x axis
        ARCBALL /// Sphere projection
    };

    using OnChange = UI_CALLBACK(void(Camera&));

    Camera(Type type = Type::PERSPECTIVE);
    virtual ~Camera() = default;

    ///
    /// @brief Initializes default view
    ///
    /// @param width window width
    /// @param height window height
    /// @param type perspective or orthographic
    /// @return Camera
    ///
    static Camera default_camera(float width, float height, Type type = Type::PERSPECTIVE);

    void set_type(Type type);
    Type get_type() const { return m_type; }

    void set_position(const Eigen::Vector3f& pos);
    void set_lookat(const Eigen::Vector3f& dir);
    void set_position_up(const Eigen::Vector3f& pos, const Eigen::Vector3f& up);
    Eigen::Vector3f get_lookat() const { return m_lookat; }
    void set_up(const Eigen::Vector3f& up);

    Eigen::Vector3f get_position() const;
    Eigen::Vector3f get_direction() const;
    Eigen::Vector3f get_up() const;

    float get_far_plane() const;
    float get_near_plane() const;

    ///
    /// @brief Set the window dimensions
    ///
    /// @param width in pixels
    /// @param height in pixels
    ///
    void set_window_dimensions(float width, float height);
    void set_aspect_ratio(float width, float height);

    ///
    /// @brief Set the Field of View
    ///
    /// @param fov in degrees
    ///
    void set_fov(float fov);
    float get_fov() const { return m_fov; }

    void set_planes(float znear, float zfar);
    float get_near() const { return m_znear; }
    float get_far() const { return m_zfar; }

    ///
    /// @brief Perspective matrix
    ///
    /// @return Eigen::Matrix4f
    ///
    Eigen::Projective3f get_perspective() const;

    ///
    /// @brief View matrix
    ///
    /// @return Eigen::Matrix4f
    ///
    Eigen::Matrix4f get_view() const;

    ///
    /// @brief Projection*View matrix
    ///
    /// @return Eigen::Matrix4f
    ///
    Eigen::Matrix4f get_PV() const;

    Eigen::Matrix4f get_view_inverse() const { return m_Vinv; }
    Eigen::Projective3f get_perspective_inverse() const { return m_Pinv; }

    float get_window_width() const;
    float get_window_height() const;
    Eigen::Vector2f get_window_size() const;


    struct Ray
    {
        Eigen::Vector3f origin;
        Eigen::Vector3f dir;
    };

    ///
    /// @brief shoots a ray from coord pixel
    ///
    /// @param coord pixel coordinate
    /// @return Ray
    ///
    Ray cast_ray(const Eigen::Vector2f& coord) const;

    ///
    /// @brief Projects 3D point to 2D pixel coordinates
    ///
    /// @param pos 3D world position
    /// @return Eigen::Vector2f
    ///
    Eigen::Vector2f project(const Eigen::Vector3f& pos) const;

    ///
    /// @brief Unprojects 2D pixels to 3D point
    ///
    /// @param screen pixel coordinate
    /// @param z depth of the 3D point (distance from camera)
    /// @return Eigen::Vector3f 3D point in world space
    ///
    Eigen::Vector3f unproject(const Eigen::Vector2f& screen, float z = 0.0f) const;

    ///
    /// @brief Projects a ray back to screen coordinates
    ///
    /// @param[in] rayOrigin
    /// @param[in] rayDir
    /// @param[out] beginOut start point of 2D line
    /// @param[out] endOut end point of 2D line
    /// @return bool is ray visible
    ///
    bool get_ray_to_screen(const Eigen::Vector3f& rayOrigin,
        const Eigen::Vector3f& rayDir,
        Eigen::Vector2f* beginOut,
        Eigen::Vector2f* endOut) const;


    void rotate_around_lookat(float angleRad);
    void rotate_tumble(float yaw_delta, float pitch_delta);


    ///
    /// @brief Rotates camera by yaw and pitch angles
    ///
    /// By default rotates around up() axis
    /// Rotates around primary_axis if specified
    ///
    /// @param yaw_delta
    /// @param pitch_delta
    /// @param primary_axis
    ///
    void rotate_turntable(
        float yaw_delta, float pitch_delta, Eigen::Vector3f primary_axis = Eigen::Vector3f::Zero());


    void rotate_arcball(const Eigen::Vector3f& camera_pos_start,
        const Eigen::Vector3f& camera_up_start,
        const Eigen::Vector2f& mouse_start,
        const Eigen::Vector2f& mouse_current);

    void zoom(float delta);
    void dolly(float delta);


    /// \deprecated Deprecated
    int get_retina_scale() const { return m_retina_scale; }

    /// \deprecated Deprecated
    void set_retina_scale(int value) { m_retina_scale = value; }

    void move_forward(float delta);
    void move_right(float delta);
    void move_up(float delta);

    ///
    /// @brief Set the orthographic viewport
    ///
    /// @param viewport Orthographic rectangle
    ///
    void set_ortho_viewport(Eigen::Vector4f viewport);

    Eigen::Vector4f get_ortho_viewport() const;

    void set_rotation_mode(RotationMode mode) { m_rotation_mode = mode; }
    RotationMode get_rotation_mode() const { return m_rotation_mode; }

    ///
    /// @brief Transform in the normalized coordinate space
    ///
    struct ViewportTransform
    {
        Eigen::Vector2f scale = Eigen::Vector2f::Ones();
        Eigen::Vector2f translate = Eigen::Vector2f::Zero();

        ///
        /// @brief Clip viewport
        ///
        /// If true, OpenGL viewport (device coords) will be altered
        /// If false, vertex transform (clip space coords) will be altered
        ///
        bool clip = false;
    };

    ////
    /// @brief Transform camera by ViewportTransform
    ///
    /// @param vt viewport transform
    /// @return Camera
    ///
    Camera transformed(const ViewportTransform& vt) const;

    ///
    /// @brief Map pixel from transformed viewport to original viewport
    ///
    /// @param vt viewport transfrom
    /// @param pixel pixel in transfromed viewport
    /// @return Eigen::Vector2f pixel in original viewport
    ///
    Eigen::Vector2f inverse_viewport_transform(
        const ViewportTransform& vt, Eigen::Vector2f& pixel) const;

    ///
    /// @brief Is pixel in camera
    ///
    bool is_pixel_in(const Eigen::Vector2f& p) const;

    ///
    /// @brief Does camera intersect pixel region
    ///
    bool intersects_region(const Eigen::Vector2f& begin, const Eigen::Vector2f& end) const;

    Eigen::Vector2f get_window_origin() const { return {m_window_origin_x, m_window_origin_y}; }
    void set_window_origin(float x, float y)
    {
        m_window_origin_x = x;
        m_window_origin_y = y;
    }

    /// Orthogonal view directions, preserve distance from pos to lookat
    enum class Dir { TOP, BOTTOM, LEFT, RIGHT, FRONT, BACK };

    ///
    /// @brief Is camera aligned to one of the six orthogonal Dir directions
    ///
    bool is_orthogonal_direction(Dir dir) const;

    ///
    /// @brief Aligns camera to one of the six orthogonal Dir directions
    ///
    void set_orthogonal_direction(Dir dir);

    /// Returns position and up direction
    std::pair<Eigen::Vector3f, Eigen::Vector3f> get_orthogonal_direction(Dir dir) const;

    ///
    /// @brief Get the Camera's Frustum
    ///
    /// @return Frustum planes
    ///
    Frustum get_frustum() const;

    ///
    /// @brief Get the Camera's Frustum of a region
    ///
    /// @return Frustum planes of a region
    ///
    Frustum get_frustum(Eigen::Vector2f min, Eigen::Vector2f max) const;

protected:
    void update_view();
    virtual void update_perspective();

    void _changed();

    Eigen::Projective3f m_P;
    Eigen::Matrix4f m_V;
    Eigen::Projective3f m_Pinv;
    Eigen::Matrix4f m_Vinv;

    Eigen::Vector3f m_pos;
    Eigen::Vector3f m_up;
    Eigen::Vector3f m_lookat;

    float m_aspectRatio = 1.0f;
    float m_fov = 3.14f / 4.0f; // in radians
    float m_znear = 0.0125f;
    float m_zfar = 128.0f;

    float m_windowWidth = 1.0f;
    float m_windowHeight = 1.0f;
    int m_retina_scale = 1;
    Type m_type;

    float m_window_origin_x = 0.0f;
    float m_window_origin_y = 0.0f;

    Eigen::Vector4f m_ortho_viewport;

    RotationMode m_rotation_mode = RotationMode::TUMBLE;

    Callbacks<OnChange> m_callbacks;

    friend CallbacksBase<Camera>;
};
} // namespace ui
} // namespace lagrange
