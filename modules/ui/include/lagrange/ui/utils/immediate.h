/*
 * Copyright 2021 Adobe. All rights reserved.
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

#include <lagrange/ui/Entity.h>
#include <Eigen/Eigen>


namespace lagrange {
namespace ui {


void render_points(Registry& r, const std::vector<Eigen::Vector3f>& points);
void render_point(Registry& r, const Eigen::Vector3f& point);
void render_lines(Registry& r, const std::vector<Eigen::Vector3f>& lines);
void upload_immediate_system(Registry& r);
/*void reset_immediate_system(Registry& r);*/


} // namespace ui
} // namespace lagrange