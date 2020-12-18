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

#include <lagrange/ui/Emitter.h>


namespace lagrange {
namespace ui {


class PointLight : public Emitter
{
public:
    Emitter::Type get_type() const override;

    PointLight(Eigen::Vector3f pos, Eigen::Vector3f intensity);

    Eigen::Vector3f get_position() const;
    void set_position(Eigen::Vector3f value);

    void set_attenuation(float value) { m_attenuation = value; }
    float get_attenuation() const { return m_attenuation; }

private:
    Eigen::Vector3f m_position;
    float m_attenuation;
};


class DirectionalLight : public Emitter
{
public:
    Emitter::Type get_type() const override;

    DirectionalLight(Eigen::Vector3f direction, Eigen::Vector3f intensity);

    Eigen::Vector3f get_direction() const;
    void set_direction(Eigen::Vector3f value);

    std::pair<Eigen::Vector3f, Eigen::Vector3f> get_perpendicular_plane() const;

private:
    Eigen::Vector3f m_direction;
};


class SpotLight : public Emitter
{
public:
    Emitter::Type get_type() const override;

    SpotLight(Eigen::Vector3f pos, Eigen::Vector3f direction, Eigen::Vector3f intensity);

    Eigen::Vector3f get_position() const;
    void set_position(Eigen::Vector3f value);

    Eigen::Vector3f get_direction() const;
    void set_direction(Eigen::Vector3f value);

    void set_attenuation(float value);
    float get_attenuation() const;

    void set_cone_angle(float value);

    float get_cone_angle() const;

    std::pair<Eigen::Vector3f, Eigen::Vector3f> get_perpendicular_plane() const;


private:
    Eigen::Vector3f m_position;
    Eigen::Vector3f m_direction;
    float m_attenuation;
    // In radians
    float m_cone_angle;
};


} // namespace ui
} // namespace lagrange
