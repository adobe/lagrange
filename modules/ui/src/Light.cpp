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
#include <lagrange/ui/Light.h>
#include <lagrange/ui/RenderUtils.h>


namespace lagrange {
namespace ui {


/*
    Point
*/

Emitter::Type PointLight::get_type() const
{
    return Emitter::Type::POINT;
}

PointLight::PointLight(Eigen::Vector3f pos, Eigen::Vector3f intensity)
: Emitter(intensity)
, m_position(pos)
, m_attenuation(1.0f)
{
}

Eigen::Vector3f PointLight::get_position() const
{
    return m_position;
}

void PointLight::set_position(Eigen::Vector3f value)
{
    m_position = value;
    m_callbacks.call<OnChange>(*this);
}


Emitter::Type SpotLight::get_type() const
{
    return Emitter::Type::SPOT;
}

SpotLight::SpotLight(Eigen::Vector3f pos, Eigen::Vector3f direction, Eigen::Vector3f intensity)
: Emitter(intensity)
, m_position(pos)
, m_direction(direction)
, m_cone_angle(pi() / 4.0f)
{
}

Eigen::Vector3f SpotLight::get_position() const
{
    return m_position;
}

void SpotLight::set_position(Eigen::Vector3f value)
{
    m_position = value;
    m_callbacks.call<OnChange>(*this);
}

Eigen::Vector3f SpotLight::get_direction() const
{
    return m_direction;
}

void SpotLight::set_direction(Eigen::Vector3f value)
{
    m_direction = value;
    m_callbacks.call<OnChange>(*this);
}

void SpotLight::set_attenuation(float value)
{
    m_attenuation = value;
    m_callbacks.call<OnChange>(*this);
}

float SpotLight::get_attenuation() const
{
    return m_attenuation;
}

void SpotLight::set_cone_angle(float value)
{
    m_cone_angle = value;
    m_callbacks.call<OnChange>(*this);
}

float SpotLight::get_cone_angle() const
{
    return m_cone_angle;
}

std::pair<Eigen::Vector3f, Eigen::Vector3f> SpotLight::get_perpendicular_plane() const
{
    return utils::render::compute_perpendicular_plane(get_direction());
}

Emitter::Type DirectionalLight::get_type() const
{
    return Emitter::Type::DIRECTIONAL;
}

DirectionalLight::DirectionalLight(Eigen::Vector3f direction, Eigen::Vector3f intensity)
: Emitter(intensity)
, m_direction(direction.normalized())
{
}

Eigen::Vector3f DirectionalLight::get_direction() const
{
    return m_direction;
}

void DirectionalLight::set_direction(Eigen::Vector3f value)
{
    m_direction = value.normalized();
    m_callbacks.call<OnChange>(*this);
}

std::pair<Eigen::Vector3f, Eigen::Vector3f> DirectionalLight::get_perpendicular_plane() const
{
    return utils::render::compute_perpendicular_plane(get_direction());
}

} // namespace ui
} // namespace lagrange
