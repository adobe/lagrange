/*
 * Copyright 2023 Adobe. All rights reserved.
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
#include <lagrange/utils/function_ref.h>

namespace lagrange {

///
/// @defgroup   group-surfacemesh-attr-utils Attributes utilities
/// @ingroup    group-surfacemesh
///
/// Various attribute processing utilities
///
/// @{

///
/// Weld an indexed attribute by combining all corners around a vertex with the same attribute
/// value.
///
/// @tparam Scalar    The mesh scalar type.
/// @tparam Index     The mesh index type.
///
/// @param mesh       The source mesh.
/// @param attr_id    The indexed attribute id.
///
template <typename Scalar, typename Index>
void weld_indexed_attribute(SurfaceMesh<Scalar, Index>& mesh, AttributeId attr_id);

/// @}

} // namespace lagrange
