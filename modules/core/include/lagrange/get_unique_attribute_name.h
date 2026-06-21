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

#include <lagrange/SurfaceMesh.h>

#include <string>
#include <string_view>

namespace lagrange {

///
/// @defgroup group-surfacemesh-utils Mesh utilities
/// @ingroup group-surfacemesh
///
/// Various attribute and mesh processing utilities
///
/// @{

///
/// Options for generating unique attribute names.
///
struct UniqueAttributeNameOptions
{
    /// Separator between the base name and the counter. Default is ".".
    std::string separator = ".";

    /// Postfix to append after the counter. Default is empty.
    std::string postfix = "";

    /// Maximum number of attempts to find a unique name. Default is 1000.
    int max_increment = 1000;

    /// Whether to emit a warning when a collision is detected and a suffix is added.
    /// Default is true.
    bool emit_warning = true;
};

///
/// Returns a unique attribute name by appending a suffix if necessary.
///
/// If the input name does not exist in the mesh, it is returned as is. If it already exists,
/// a suffix of the form `{separator}{count}{postfix}` is appended until a unique name is found.
/// If no unique name can be found after `options.max_increment` attempts, an error is thrown.
///
/// @param      mesh     The mesh to check for existing attribute names.
/// @param      name     The desired attribute name.
/// @param      options  Options for generating the unique name.
///
/// @tparam     Scalar   Mesh scalar type.
/// @tparam     Index    Mesh index type.
///
/// @return     A unique attribute name.
///
template <typename Scalar, typename Index>
std::string get_unique_attribute_name(
    const SurfaceMesh<Scalar, Index>& mesh,
    std::string_view name,
    const UniqueAttributeNameOptions& options = {});

/// @}

} // namespace lagrange
