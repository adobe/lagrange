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
#include <lagrange/ui/components/Transform.h>
#include <lagrange/ui/default_entities.h>
#include <lagrange/ui/utils/io.h>
#include <lagrange/ui/utils/layer.h>
#include <lagrange/ui/utils/lights.h>
#include <lagrange/ui/utils/treenode.h>

#include <cmath>

namespace lagrange {
namespace ui {


Entity add_point_light(
    Registry& r,
    Eigen::Vector3f intensity /*= Eigen::Vector3f::Ones()*/,
    Eigen::Vector3f position /*= Eigen::Vector3f::Zero()*/)
{
    auto e = ui::create_scene_node(r, "Point Light");

    LightComponent lc;
    lc.type = LightComponent::Type::POINT;
    lc.intensity = intensity;
    ui::set_transform(r, e, Eigen::Translation3f(position));

    r.emplace<LightComponent>(e, lc);

    return e;
}

Entity add_directional_light(
    Registry& r,
    Eigen::Vector3f intensity /*= Eigen::Vector3f::Ones()*/,
    Eigen::Vector3f direction /*= Eigen::Vector3f::Zero()*/)
{
    if (direction.isZero()) {
        lagrange::logger().warn("add_directional_light, direction is null vector");
    }

    auto e = ui::create_scene_node(r, "Directional Light");

    LightComponent lc;
    lc.type = LightComponent::Type::DIRECTIONAL;
    lc.intensity = intensity;

    const auto initial_dir = get_canonical_light_direction();
    if (initial_dir != direction) {
        const auto axis = initial_dir.cross(direction);
        const auto axis_norm = axis.norm();
        const auto angle = std::atan2(axis_norm, initial_dir.dot(direction));

        Eigen::Matrix4f M = Eigen::Matrix4f::Identity();
        M.block<3, 3>(0, 0) = Eigen::AngleAxisf(angle, axis * 1.0 / axis_norm).toRotationMatrix();
        ui::set_transform(r, e, M);
    }

    r.emplace<LightComponent>(e, lc);

    return e;
}

lagrange::ui::Entity add_spot_light(
    Registry& r,
    Eigen::Vector3f intensity /*= Eigen::Vector3f::Ones()*/,
    Eigen::Vector3f position /*= Eigen::Vector3f::Ones()*/,
    Eigen::Vector3f direction /*= -Eigen::Vector3f::Ones()*/,
    float cone_angle /*= pi() / 4.0f*/)
{
    auto e = ui::create_scene_node(r, "Spot Light");


    LightComponent lc;
    lc.type = LightComponent::Type::SPOT;
    lc.intensity = intensity;
    lc.cone_angle = cone_angle;


    const auto initial_dir = get_canonical_light_direction();
    if (initial_dir != direction) {
        const auto axis = initial_dir.cross(direction);
        const auto axis_norm = axis.norm();
        const auto angle = std::atan2(axis_norm, initial_dir.dot(direction));

        Eigen::Matrix4f M = Eigen::Matrix4f::Identity();
        M.block<3, 3>(0, 0) = Eigen::AngleAxisf(angle, axis * 1.0 / axis_norm).toRotationMatrix();
        ui::set_transform(r, e, M);
    }

    ui::apply_transform(r, e, Eigen::Translation3f(position));

    r.emplace<LightComponent>(e, lc);

    return e;
}

std::pair<Eigen::Vector3f, Eigen::Vector3f> get_light_position_and_direction(
    const Registry& r,
    Entity e)
{
    Eigen::Vector3f pos = Eigen::Vector3f(0, 0, 0);
    Eigen::Vector3f dir = get_canonical_light_direction();

    if (r.all_of<Transform>(e)) {
        const auto& T = r.get<Transform>(e);
        pos = T.global * pos;
        const auto tmp = T.global * Eigen::Vector4f(dir.x(), dir.y(), dir.z(), 0);
        dir = Eigen::Vector3f(tmp.x(), tmp.y(), tmp.z()).normalized();
    }
    return {pos, dir};
}

void clear_lights(Registry& r)
{
    auto v = r.view<LightComponent>();
    r.destroy(v.begin(), v.end());
}

} // namespace ui
} // namespace lagrange
