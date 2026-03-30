/*
 * Copyright 2025 Adobe. All rights reserved.
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

#include <string_view>

namespace lagrange {

///
/// @addtogroup group-surfacemesh-utils
/// @{
///

struct UVMeshOptions
{
    /// Supported element types for UV mesh extraction.
    enum class ElementTypes {
        IndexedOrVertex, ///< Only indexed/vertex attributes (zero-copy, no allocation).
        All, ///< Also supports corner attributes (may allocate an index buffer).
    };

    /// Input UV attribute name.
    ///
    /// The attribute must be a UV attribute of type `UVScalar` with 2 channels. Supported element
    /// types depend on the `element_types` option. If empty, the first matching UV attribute will be
    /// used.
    std::string_view uv_attribute_name = "";

    /// Supported element types for UV attribute lookup.
    ///
    /// By default, only indexed and vertex attributes are supported (zero-copy). Set to
    /// ElementTypes::All to also support corner attributes, which may allocate an index buffer.
    ElementTypes element_types = ElementTypes::IndexedOrVertex;
};

/**
 * Extract a UV mesh reference from an input mesh.
 *
 * This method will create a new mesh with the same topology as the input mesh, but with vertex
 * positions set to the corresponding UV coordinates. Modification of UV mesh vertices will be
 * reflected in the input mesh.
 *
 * @param      mesh      Input mesh.
 * @param      options   Options to control UV mesh extraction.
 *
 * @tparam     Scalar    Mesh scalar type.
 * @tparam     Index     Mesh index type.
 * @tparam     UVScalar  Target UV attribute value type.
 *
 * @return     The extracted UV mesh reference.
 *
 * @see        @ref UVMeshOptions
 */
template <typename Scalar, typename Index, typename UVScalar = Scalar>
SurfaceMesh<UVScalar, Index> uv_mesh_ref(
    SurfaceMesh<Scalar, Index>& mesh,
    const UVMeshOptions& options = {});

/**
 * Extract a UV mesh view from an input mesh.
 *
 * This method will create a new mesh with the same topology as the input mesh, but with vertex
 * positions set to the corresponding UV coordinates. The output UV mesh cannot be modified.
 *
 * @param      mesh      Input mesh.
 * @param      options   Options to control UV mesh extraction.
 *
 * @tparam     Scalar    Mesh scalar type.
 * @tparam     Index     Mesh index type.
 * @tparam     UVScalar  Target UV attribute value type.
 *
 * @return     The extracted UV mesh reference.
 *
 * @see        @ref UVMeshOptions
 */
template <typename Scalar, typename Index, typename UVScalar = Scalar>
SurfaceMesh<UVScalar, Index> uv_mesh_view(
    const SurfaceMesh<Scalar, Index>& mesh,
    const UVMeshOptions& options = {});

/// @}

} // namespace lagrange
