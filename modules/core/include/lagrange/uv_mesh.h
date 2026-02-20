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

struct UVMeshOptions
{
    /// Input UV attribute name.
    ///
    /// The attribute must be a vertex or indexed attribute of type `Scalar`.
    /// If empty, the first UV attribute will be used.
    std::string_view uv_attribute_name = "";
};

/**
 * Extract a UV mesh reference from an input mesh.
 *
 * This method will create a new mesh with the same topology as the input mesh, but with vertex
 * positions set to the corresponding UV coordinates. Modification of UV mesh vertices will be
 * reflected in the input mesh.
 *
 * @warning    This method requires that the input UV attribute is an indexed or vertex attribute.
 *             Corner attributes are not supported by this function.
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
 * @warning    This method requires that the input UV attribute is an indexed or vertex attribute.
 *             Corner attributes are not supported by this function.
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

} // namespace lagrange
