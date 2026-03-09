/*
 * Copyright 2026 Adobe. All rights reserved.
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

#include <lagrange/Logger.h>
#include <lagrange/SurfaceMesh.h>
#include <lagrange/utils/Error.h>

#include <string_view>

namespace lagrange::internal {

///
/// Returns a unique attribute name by appending a suffix if necessary. If the input name does not
/// exist in the mesh, it is returned as is. If it already exists, a suffix of the form ".0", ".1",
/// etc. is appended until a unique name is found. If a unique name cannot be found after 1000
/// attempts, an error is thrown.
///
/// @param      mesh    The mesh to check for existing attribute names.
/// @param      name    The desired attribute name.
///
/// @tparam     Scalar  Mesh scalar type.
/// @tparam     Index   Mesh index type.
///
/// @return     A unique attribute name.
///
template <typename Scalar, typename Index>
std::string get_unique_attribute_name(const SurfaceMesh<Scalar, Index>& mesh, std::string_view name)
{
    if (!mesh.has_attribute(name)) {
        return std::string(name);
    } else {
        std::string new_name;
        for (int cnt = 0; cnt < 1000; ++cnt) {
            new_name = fmt::format("{}.{}", name, cnt);
            if (!mesh.has_attribute(new_name)) {
                logger().warn("Attribute '{}' already exists. Using '{}' instead.", name, new_name);
                return new_name;
            }
        }
        throw Error(fmt::format("Could not assign a unique attribute name for: {}", name));
    }
}

} // namespace lagrange::internal
