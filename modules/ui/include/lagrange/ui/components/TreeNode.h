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

#include <lagrange/ui/Entity.h>

namespace lagrange {
namespace ui {

// Reference:
//https://skypjack.github.io/2019-06-25-ecs-baf-part-4/

struct TreeNode
{
    Entity parent = NullEntity;
    Entity first_child = NullEntity;
    Entity next_sibling = NullEntity;
    Entity prev_sibling = NullEntity;
    size_t num_children = 0;
};


} // namespace ui
} // namespace lagrange
