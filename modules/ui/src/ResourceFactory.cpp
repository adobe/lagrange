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
#include <lagrange/ui/ResourceFactory.h>

namespace lagrange {
namespace ui {


std::unordered_map<
    ResourceFactory::KeyType,
    std::unique_ptr<ResourceFactory::RealizeFunctionBase>,
    utils::pair_hash>
    ResourceFactory::m_deferred_realize_functions;

std::unordered_map<
    ResourceFactory::KeyType,
    std::unique_ptr<ResourceFactory::RealizeFunctionBase>,
    utils::pair_hash>
    ResourceFactory::m_direct_realize_functions;

} // namespace ui
} // namespace lagrange
