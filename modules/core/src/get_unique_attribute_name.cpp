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

#include <lagrange/get_unique_attribute_name.h>

#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/utils/Error.h>
#include <lagrange/utils/fmt/format.h>

namespace lagrange {

template <typename Scalar, typename Index>
std::string get_unique_attribute_name(
    const SurfaceMesh<Scalar, Index>& mesh,
    std::string_view name,
    const UniqueAttributeNameOptions& options)
{
    if (!mesh.has_attribute(name)) {
        return std::string(name);
    } else {
        std::string new_name;
        for (int cnt = 0; cnt < options.max_increment; ++cnt) {
            new_name = format("{}{}{}{}", name, options.separator, cnt, options.postfix);
            if (!mesh.has_attribute(new_name)) {
                if (options.emit_warning) {
                    logger().warn(
                        "Attribute '{}' already exists. Using '{}' instead.",
                        name,
                        new_name);
                }
                return new_name;
            }
        }
        throw Error(format("Could not assign a unique attribute name for: {}", name));
    }
}

#define LA_X_get_unique_attribute_name(_, Scalar, Index)        \
    template LA_CORE_API std::string get_unique_attribute_name( \
        const SurfaceMesh<Scalar, Index>&,                      \
        std::string_view,                                       \
        const UniqueAttributeNameOptions&);
LA_SURFACE_MESH_X(get_unique_attribute_name, 0)

} // namespace lagrange
