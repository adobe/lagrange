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
#include <assert.h>
#include <lagrange/ui/Camera.h>
#include <lagrange/utils/utils.h>
#include <lagrange/Logger.h>
#include <cmath>

namespace lagrange {
namespace ui {

Camera::Camera(Type type /* = Type::PERSPECTIVE*/)
    : m_type(type)
{
    m_P = Eigen::Matrix4f::Identity();
    m_V = Eigen::Matrix4f::Identity();
    m_Pinv = Eigen::Matrix4f::Identity();
    m_Vinv = Eigen::Matrix4f::Identity();

    m_pos = Eigen::Vector3f(1, 0, 0);
    m_up = Eigen::Vector3f(0, 1, 0);
    m_lookat = Eigen::Vector3f::Zero();

    m_ortho_viewport = Eigen::Vector4f(0.0f, 1.0f, 1.0f, 0.0f);
}

Camera Camera::default_camera(float width, float height, Type type /* = Type::PERSPECTIVE*/)
{
    Camera c(type);
    c.set_up(Eigen::Vector3f(0, 1, 0));
    c.set_position(Eigen::Vector3f(-4.11f, 0.569f, -0.195f));
    c.set_lookat(Eigen::Vector3f::Zero());
    c.set_window_dimensions(width, height);
    c.set_fov(3.14f / 4.0f);
    c.set_planes(0.0125f, 128.0f);

    return c;
}

void Camera::set_type(Type type)
{
    if (m_type == type) return;

    m_callbacks.set_enabled(false);

    // Convert perspective to orthographic
    if (get_type() == Type::PERSPECTIVE && type == Type::ORTHOGRAPHIC) {
        float depth = (get_position() - get_lookat()).norm();
        float h = depth * 2.0f * std::atan(get_fov());
        float w = h * m_aspectRatio;
        set_ortho_viewport(Eigen::Vector4f(w / -2.0f, w / 2.0f, h / -2.0f, h / 2.0f));
    }

    m_type = type;


    update_view();
    update_perspective();

    m_callbacks.set_enabled(true);
    m_callbacks.call<OnChange>(*this);
}

void Camera::set_position(const Eigen::Vector3f& pos)
{
    if (!std::isfinite(pos.x()) || !std::isfinite(pos.y()) || !std::isfinite(pos.z())) return;
    m_pos = pos.cwiseMin(1e15f).cwiseMax(-1e15f);
    update_view();
}

void Camera::set_up(const Eigen::Vector3f& up)
{
    m_up = up;
    update_view();
}

Eigen::Vector3f Camera::get_position() const
{
    return m_pos;
}
Eigen::Vector3f Camera::get_direction() const
{
    return (m_lookat - m_pos).normalized();
}
Eigen::Vector3f Camera::get_up() const
{
    return m_up;
}

float Camera::get_far_plane() const
{
    return m_zfar;
}
float Camera::get_near_plane() const
{
    return m_znear;
}

void Camera::set_window_dimensions(float width, float height)
{
    m_windowWidth = width;
    m_windowHeight = height;
    set_aspect_ratio(m_windowWidth, m_windowHeight);
}

void Camera::set_aspect_ratio(float width, float height)
{
    m_aspectRatio = float(width) / float(height);
    if (get_type() == Type::ORTHOGRAPHIC) {
        auto v = get_ortho_viewport();
        float w = (v.y() - v.x());
        float h = w / m_aspectRatio;
        set_ortho_viewport(Eigen::Vector4f(w / -2.0f, w / 2.0f, h / -2.0f, h / 2.0f));
    }
    update_perspective();
}
void Camera::set_fov(float fov)
{
    m_fov = fov;
    update_perspective();
}

void Camera::set_planes(float znear, float zfar)
{
    m_znear = znear;
    m_zfar = zfar;
    update_perspective();
}

Eigen::Projective3f Camera::get_perspective() const
{
    return m_P;
}
Eigen::Matrix4f Camera::get_view() const
{
    return m_V;
}
Eigen::Matrix4f Camera::get_PV() const
{
    return m_P * m_V;
}
float Camera::get_window_width() const
{
    return m_windowWidth * m_retina_scale;
}
float Camera::get_window_height() const
{
    return m_windowHeight * m_retina_scale;
}

Eigen::Vector2f Camera::get_window_size() const
{
    return Eigen::Vector2f(get_window_width(), get_window_height());
}

void Camera::update_view()
{
    m_V = look_at(m_pos, m_lookat, m_up);
    m_Vinv = m_V.inverse();
    assert(std::isfinite(m_V(0)));
    m_callbacks.call<OnChange>(*this);
}

void Camera::update_perspective()
{
    if (m_type == Type::PERSPECTIVE) {
        m_P = perspective(m_fov, m_aspectRatio, m_znear, m_zfar);
    } else if (m_type == Type::ORTHOGRAPHIC) {
        m_P = ortho(m_ortho_viewport.x(),
            m_ortho_viewport.y(),
            m_ortho_viewport.z(),
            m_ortho_viewport.w(),
            m_znear,
            m_zfar);
    }

    m_Pinv = m_P.inverse();
    assert(std::isfinite(m_P.matrix()(0)));
    m_callbacks.call<OnChange>(*this);
}

Camera::Ray Camera::cast_ray(const Eigen::Vector2f& coord) const
{
    if (m_type == Type::PERSPECTIVE) {
        Eigen::Vector4f rayNorm =
            Eigen::Vector4f(coord.x() / m_windowWidth, (coord.y() / m_windowHeight), 0.0f, 0.0f) *
                2.0f -
            Eigen::Vector4f(1.0f, 1.0f, 0.0f, 0.0f);

        rayNorm.z() = -1.0f;
        rayNorm.w() = 1.0f;

        Eigen::Vector4f rayEye = m_Pinv * rayNorm;
        rayEye.z() = -1.0f;
        rayEye.w() = 0.0f;

        Eigen::Vector4f rayWorld = m_Vinv * rayEye;

        Eigen::Vector3f ray = rayWorld.head<3>().normalized();

        return {get_position(), ray};
    } else {
        Eigen::Vector3f pos = unproject(coord, 0.0f);
        return {pos, get_direction()};
    }
}

Eigen::Vector2f Camera::project(const Eigen::Vector3f& pos) const
{
    Eigen::Vector4f v = Eigen::Vector4f(pos.x(), pos.y(), pos.z(), 1.0f);
    // Project
    v = m_P * m_V * v;
    //Viewport //Perspective division
    Eigen::Vector2f s = Eigen::Vector2f(v.x() / v.w(), v.y() / v.w());
    s.x() = (s.x() + 1.0f) / 2.0f;
    s.y() = 1.0f - ((s.y() + 1.0f) / 2.0f);

    s.x() *= m_windowWidth;
    s.y() *= m_windowHeight;

    return s;
}

Eigen::Vector3f Camera::unproject(const Eigen::Vector2f& screen, float z) const
{
    Eigen::Vector4f Vp = Eigen::Vector4f(0, 0, m_windowWidth, m_windowHeight);
    return unproject_point(Eigen::Vector3f(screen.x(), screen.y(), z), m_V, m_P.matrix(), Vp);
}

bool Camera::get_ray_to_screen(const Eigen::Vector3f& rayOrigin,
    const Eigen::Vector3f& rayDir,
    Eigen::Vector2f* beginOut,
    Eigen::Vector2f* endOut) const
{
    Eigen::Vector2f begin = project(rayOrigin);
    Eigen::Vector2f end = project(rayOrigin + rayDir * m_zfar);

    Eigen::Vector2f bounds[2];
    bounds[0] = Eigen::Vector2f::Zero();
    bounds[1] = Eigen::Vector2f(m_windowWidth, m_windowHeight);
    begin = begin.cwiseMax(bounds[0]).cwiseMin(bounds[1]);
    end = end.cwiseMax(bounds[0]).cwiseMin(bounds[1]);

    // Out of screen
    if (begin.x() == end.x() && begin.y() == end.y()) return false;

    *beginOut = begin;
    *endOut = end;

    return true;
}

void Camera::rotate_around_lookat(float angleRad)
{
    const Eigen::Vector3f at_to_eye = m_pos - m_lookat;
    const Eigen::Vector3f rotated_at_to_eye = Eigen::AngleAxisf(angleRad, m_up) * at_to_eye;
    set_position(m_lookat + rotated_at_to_eye);
}

void Camera::rotate_tumble(float yaw_delta, float pitch_delta)
{
    m_callbacks.set_enabled(false);

    if (get_up().y() < 0.0f) {
        yaw_delta = -yaw_delta;
    }

    const float dist = (get_position() - get_lookat()).norm();
    const Eigen::Vector3f look = (get_position() - get_lookat()).normalized();

    float yaw = std::atan2(look.z(), look.x()); // azimuth
    float pitch = std::acos(look.y()); // inclination

    // Map pitch to [0,2pi] ([0,pi] when up vector points up, [pi,2pi] when down)
    if (get_up().y() < 0.0f) {
        pitch = two_pi() - pitch;
    }

    yaw -= yaw_delta;
    pitch -= pitch_delta;

    // Remap yaw when camera is inverted (rotate in circle by half circle)
    if (get_up().y() < 0.0f) {
        yaw = yaw + pi();
        // Modulo two pi
        if (yaw > two_pi()) {
            yaw -= two_pi();
        }
    }

    const Eigen::Vector3f sphere_pos = Eigen::Vector3f(
        std::sin(pitch) * std::cos(yaw), std::cos(pitch), std::sin(pitch) * std::sin(yaw));
    const Eigen::Vector3f new_pos = get_lookat() + dist * sphere_pos;
    const Eigen::Vector3f new_side = Eigen::Vector3f(-std::sin(yaw), 0, std::cos(yaw));
    const Eigen::Vector3f new_up = (-sphere_pos).cross(new_side);

    set_position(new_pos);
    set_up(new_up);

    m_callbacks.set_enabled(true);
    m_callbacks.call<OnChange>(*this);
}


void Camera::rotate_turntable(float yaw_delta, float pitch_delta, Eigen::Vector3f primary_axis)
{
    if (primary_axis.x() != 0 || primary_axis.y() != 0 || primary_axis.z() != 0) {
        set_up(primary_axis);
    }

    const Eigen::Vector3f look = (get_position() - get_lookat()).normalized();
    const Eigen::Vector3f side = look.cross(get_up());

    const Eigen::AngleAxisf R_yaw = Eigen::AngleAxisf(yaw_delta, get_up());
    const Eigen::AngleAxisf R_pitch = Eigen::AngleAxisf(pitch_delta, side);

    const Eigen::Vector3f pos = get_position();
    const Eigen::Vector3f new_pos = (R_yaw * R_pitch * pos);

    set_position(new_pos);
}


void Camera::rotate_arcball(const Eigen::Vector3f& camera_pos_start,
    const Eigen::Vector3f& camera_up_start,
    const Eigen::Vector2f& mouse_start,
    const Eigen::Vector2f& mouse_current)
{
    // No change of camera
    if (mouse_start.x() == mouse_current.x() && mouse_start.y() == mouse_current.y()) return;

    m_callbacks.set_enabled(false);

    const auto map_to_sphere = [&](const Eigen::Vector2f& pos) -> Eigen::Vector3f {
        Eigen::Vector3f p = Eigen::Vector3f::Zero();
        // Map to fullscreen ellipse
        p.x() = (2 * pos.x() / get_window_width() - 1.0f);
        p.y() = (2 * (pos.y() / get_window_height()) - 1.0f);

        float lensq = p.x() * p.x() + p.y() * p.y();

        if (lensq <= 1.0f) {
            p.z() = std::sqrt(1 - lensq);
        } else {
            p = p.normalized();
        }
        return p;
    };

    const auto decompose =
        [](const Eigen::Matrix4f& m, Eigen::Vector3f& T, Eigen::Matrix3f& R, Eigen::Vector3f& S) {
            T = m.col(3).head<3>();
            S = Eigen::Vector3f((m.col(0).norm()), (m.col(1).norm()), (m.col(2).norm()));
            R.col(0) = m.col(0).head<3>() * (1.0f / S(0));
            R.col(1) = m.col(1).head<3>() * (1.0f / S(1));
            R.col(2) = m.col(2).head<3>() * (1.0f / S(2));
        };


    //Calculate points on sphere and rotation axis/angle
    const Eigen::Vector3f p0 = map_to_sphere(mouse_start);
    const Eigen::Vector3f p1 = map_to_sphere(mouse_current);

    // Axis is in default coord system
    const Eigen::Vector3f axis = p0.cross(p1).normalized();
    const float angle = vector_angle(p0, p1);

    // Initial rotation
    const Eigen::Matrix4f r_0 = look_at(camera_pos_start, m_lookat, camera_up_start);
    // Rotate axis to current frame and rotate around it
    const Eigen::Vector3f rotated_axis = (r_0.inverse().block<3, 3>(0, 0) * axis);
    Eigen::Matrix4f r_arc = Eigen::Matrix4f::Identity();
    r_arc.block<3, 3>(0, 0) = Eigen::AngleAxisf(angle, rotated_axis).matrix();

    // Get inverse new view matrix
    const Eigen::Matrix4f r = (r_0 * r_arc).inverse().matrix();

    Eigen::Vector3f new_pos, new_scale;
    Eigen::Matrix3f r_decomp;

    // Decompose new position and new rotation
    decompose(r, new_pos, r_decomp, new_scale);

    // Rotate default up axis
    const Eigen::Vector3f up = r_decomp * Eigen::Vector3f(0, 1, 0);

    // Set new camera properties
    set_position(new_pos);
    set_up(up);

    m_callbacks.set_enabled(true);
    m_callbacks.call<OnChange>(*this);
}


void Camera::zoom(float delta)
{
    if (delta == 0.0f) return;
    delta = std::min(std::max(delta, -0.25f), 0.25f);
    
    if (get_type() == Camera::Type::PERSPECTIVE) {

        const float fov = lagrange::to_degrees(get_fov());
        
        float max_step_deg = 2.0f;
        if (fov > 100.0f) max_step_deg *= std::exp(-((fov - 100.0f) / 70.0f) * 8.0f);

        float new_fov = 
            std::max(std::min(fov * (1.0f - 0.5f * delta), fov + max_step_deg), fov - max_step_deg);
        
        new_fov = std::max(1.0e-5f, new_fov);
        new_fov = std::min(170.0f, new_fov);
        set_fov(lagrange::to_radians(new_fov));

    } else {
        dolly(delta);
    }
    
}

void Camera::dolly(float delta) {
    if (delta == 0.0f) return;
    delta = std::min(std::max(delta, -0.25f), 0.25f);

    if (get_type() == Camera::Type::PERSPECTIVE) {
        set_position(((1.0f - 1.0f * delta) * (get_position() - get_lookat())) + get_lookat());
    } else if (get_type() == Camera::Type::ORTHOGRAPHIC) {
        const auto new_v = (get_ortho_viewport() * (1.0f - delta)).eval();
        if((new_v.array().cwiseAbs() > 1e-5f).all()){
            set_ortho_viewport(new_v);
        }
    }

    update_view();
    update_perspective();

}

void Camera::move_forward(float delta)
{
    Eigen::Vector3f v = get_direction() * delta;
    m_lookat = get_lookat() + v;
    m_pos = get_position() + v;
    update_view();
}

void Camera::move_right(float delta)
{
    const Eigen::Vector3f right = get_direction().cross(get_up()).normalized();
    Eigen::Vector3f v = right * delta;
    m_pos = get_position() + v;
    m_lookat = get_lookat() + v;
    update_view();
}

void Camera::move_up(float delta)
{
    const Eigen::Vector3f v = get_up() * delta;
    m_pos = get_position() + v;
    m_lookat = get_lookat() + v;
    update_view();
}

void Camera::set_ortho_viewport(Eigen::Vector4f viewport)
{
    if (std::isnan(viewport.x()) || std::isnan(viewport.y()) || std::isnan(viewport.z()) ||
        std::isnan(viewport.w()))
        return;
    m_ortho_viewport = viewport;
    update_perspective();
}

Eigen::Vector4f Camera::get_ortho_viewport() const
{
    return m_ortho_viewport;
}

Camera Camera::transformed(const ViewportTransform& vt) const
{
    Camera cam = *this;
    cam.m_callbacks.clear<OnChange>();


    if (vt.clip) {
        Eigen::Vector2f orig = cam.get_window_origin();
        cam.set_window_origin(orig.x() + cam.get_window_width() * vt.translate.x(),
            orig.y() + cam.get_window_height() * vt.translate.y());
        cam.set_window_dimensions(
            cam.get_window_width() * vt.scale.x(), cam.get_window_height() * vt.scale.y());
    } else {
        Eigen::Translation3f half = Eigen::Translation3f(Eigen::Vector3f(1.0f, 1.0f, 0));

        Eigen::Vector2f offset = vt.translate * 2.0f - Eigen::Vector2f::Ones();
        Eigen::Translation3f V_T = Eigen::Translation3f(Eigen::Vector3f(offset.x(), offset.y(), 0));
        Eigen::Matrix3f V_S = Eigen::Scaling(vt.scale.x(), vt.scale.y(), 1.0f);
        cam.m_P = V_T * V_S * half * cam.m_P;
    }

    return cam;
}

Eigen::Vector2f Camera::inverse_viewport_transform(
    const ViewportTransform& vt, Eigen::Vector2f& pixel) const
{
    const Eigen::Vector2f vt_origin = vt.translate.cwiseProduct(get_window_size());
    return (pixel - vt_origin)
        .cwiseProduct(Eigen::Vector2f(1.0f / vt.scale.x(), 1.0f / vt.scale.y()));
}

bool Camera::is_pixel_in(const Eigen::Vector2f& p) const
{
    return p.x() >= 0.0f && p.y() >= 0.0f && p.x() < float(get_window_width()) &&
           p.y() < float(get_window_height());
}

bool Camera::intersects_region(const Eigen::Vector2f& begin, const Eigen::Vector2f& end) const
{
    Eigen::Vector2f min = begin.cwiseMin(end);
    Eigen::Vector2f max = begin.cwiseMax(end);
    if (max.x() < 0.0f || min.x() > get_window_width()) return false;
    if (max.y() < 0.0f || min.y() > get_window_height()) return false;
    return true;
}

bool Camera::is_orthogonal_direction(Dir dir) const
{
    auto config = get_orthogonal_direction(dir);
    return (config.first.array() == get_position().array()).all();
}

void Camera::set_orthogonal_direction(Dir dir)
{
    auto config = get_orthogonal_direction(dir);
    set_position_up(config.first, config.second);
}

std::pair<Eigen::Vector3f, Eigen::Vector3f> Camera::get_orthogonal_direction(Dir dir) const
{
    const float d = (get_lookat() - get_position()).norm();
    switch (dir) {
    case Dir::TOP: return {Eigen::Vector3f(0, 1, 0) * d + get_lookat(), Eigen::Vector3f(1, 0, 0)};
    case Dir::BOTTOM: return {Eigen::Vector3f(0, -1, 0) * d + get_lookat(), Eigen::Vector3f(-1, 0, 0)};
    case Dir::LEFT: return {Eigen::Vector3f(0, 0, -1) * d + get_lookat(), Eigen::Vector3f(0, 1, 0)};
    case Dir::RIGHT: return {Eigen::Vector3f(0, 0, 1) * d + get_lookat(), Eigen::Vector3f(0, 1, 0)};
    case Dir::FRONT: return {Eigen::Vector3f(-1, 0, 0) * d + get_lookat(), Eigen::Vector3f(0, 1, 0)};
    case Dir::BACK: return {Eigen::Vector3f(1, 0, 0) * d + get_lookat(), Eigen::Vector3f(0, 1, 0)};
    default: break;
    }
    return {};
}

void Camera::set_lookat(const Eigen::Vector3f& dir)
{
    m_lookat = dir;
    update_view();
}

void Camera::set_position_up(const Eigen::Vector3f& pos, const Eigen::Vector3f& up)
{
    m_pos = pos;
    m_up = up;
    update_view();
}

Frustum Camera::get_frustum() const
{
    return get_frustum(Eigen::Vector2f(0), Eigen::Vector2f(get_window_size()));
}

Frustum Camera::get_frustum(Eigen::Vector2f min, Eigen::Vector2f max) const
{
    auto ray_bottom_left = cast_ray(min);
    auto ray_top_left = cast_ray({min.x(), max.y()});
    auto ray_top_right = cast_ray(max);
    auto ray_bottom_right = cast_ray({max.x(), min.y()});
    auto ray_center = cast_ray((min + max) * 0.5f);

    const Eigen::Vector3f side = get_direction().cross(get_up());
    const Eigen::Vector3f c_near = ray_center.origin + ray_center.dir * get_near();
    const Eigen::Vector3f c_far = ray_center.origin + ray_center.dir * get_far();

    Frustum f;
    f.vertices[FRUSTUM_FAR_LEFT_TOP] = ray_top_left.origin + ray_top_left.dir * get_far();
    f.vertices[FRUSTUM_FAR_RIGHT_TOP] = ray_top_right.origin + ray_top_right.dir * get_far();
    f.vertices[FRUSTUM_FAR_LEFT_BOTTOM] = ray_bottom_left.origin + ray_bottom_left.dir * get_far();
    f.vertices[FRUSTUM_FAR_RIGHT_BOTTOM] =
        ray_bottom_right.origin + ray_bottom_right.dir * get_far();

    f.vertices[FRUSTUM_NEAR_LEFT_TOP] = ray_top_left.origin + ray_top_left.dir * get_near();
    f.vertices[FRUSTUM_NEAR_RIGHT_TOP] = ray_top_right.origin + ray_top_right.dir * get_near();
    f.vertices[FRUSTUM_NEAR_LEFT_BOTTOM] =
        ray_bottom_left.origin + ray_bottom_left.dir * get_near();
    f.vertices[FRUSTUM_NEAR_RIGHT_BOTTOM] =
        ray_bottom_right.origin + ray_bottom_right.dir * get_near();

    const Eigen::Vector3f n_right =
        (f.vertices[FRUSTUM_FAR_RIGHT_BOTTOM] - f.vertices[FRUSTUM_NEAR_RIGHT_BOTTOM])
            .cross(get_up());
    const Eigen::Vector3f n_left =
        get_up().cross(f.vertices[FRUSTUM_FAR_LEFT_BOTTOM] - f.vertices[FRUSTUM_NEAR_LEFT_BOTTOM]);
    const Eigen::Vector3f n_top =
        side.cross(f.vertices[FRUSTUM_FAR_LEFT_TOP] - f.vertices[FRUSTUM_NEAR_LEFT_TOP]);
    const Eigen::Vector3f n_bottom =
        (f.vertices[FRUSTUM_FAR_LEFT_BOTTOM] - f.vertices[FRUSTUM_NEAR_LEFT_BOTTOM]).cross(side);

    const float sgn = -1.0f; // outward
    f.planes[FRUSTUM_NEAR] = Frustum::Plane(-sgn * get_direction(), c_near);
    f.planes[FRUSTUM_FAR] = Frustum::Plane(sgn * get_direction(), c_far);
    f.planes[FRUSTUM_LEFT] = Frustum::Plane(sgn * n_left, f.vertices[FRUSTUM_NEAR_LEFT_BOTTOM]);
    f.planes[FRUSTUM_RIGHT] = Frustum::Plane(sgn * n_right, f.vertices[FRUSTUM_NEAR_RIGHT_BOTTOM]);
    f.planes[FRUSTUM_TOP] = Frustum::Plane(sgn * n_top, f.vertices[FRUSTUM_NEAR_LEFT_TOP]);
    f.planes[FRUSTUM_BOTTOM] = Frustum::Plane(sgn * n_bottom, f.vertices[FRUSTUM_NEAR_LEFT_BOTTOM]);
    return f;
}

} // namespace ui
} // namespace lagrange
