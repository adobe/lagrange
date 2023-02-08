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

namespace lagrange {

namespace winding {

///
/// Fast winding number computation for triangle soups.
///
class FastWindingNumber
{
public:
    ///
    /// Constructs an acceleration structure on a given mesh to speed up winding number queries.
    ///
    /// @note       Internally, point coordinates are converted to `float` and vertex indices are
    ///             converted to `int`.
    ///
    /// @param[in]  mesh    Triangle mesh used to initialize the fast winding number acceleration
    ///                     structure.
    ///
    /// @tparam     Scalar  Mesh scalar type.
    /// @tparam     Index   Mesh index type.
    ///
    template <typename Scalar, typename Index>
    FastWindingNumber(const SurfaceMesh<Scalar, Index>& mesh);

    ///
    /// Constructs a new instance.
    ///
    FastWindingNumber();

    ///
    /// Destroys the object.
    ///
    ~FastWindingNumber();

    ///
    /// Constructs a new instance.
    ///
    /// @param      other  Instance to move from.
    ///
    FastWindingNumber(FastWindingNumber&& other) noexcept;

    ///
    /// Assignment operator.
    ///
    /// @param      other  Instance to move from.
    ///
    /// @return     The result of the assignment.
    ///
    FastWindingNumber& operator=(FastWindingNumber&& other) noexcept;

    ///
    /// Constructs a new instance.
    ///
    /// @param[in]  other  Instance to copy from.
    ///
    FastWindingNumber(const FastWindingNumber& other) = delete;

    ///
    /// Assignment operator.
    ///
    /// @param[in]  other  Instance to copy from.
    ///
    /// @return     The result of the assignment.
    ///
    FastWindingNumber& operator=(const FastWindingNumber& other) = delete;

    ///
    /// Determines whether the specified query point is inside the volume.
    ///
    /// @param[in]  pos   Query position.
    ///
    /// @return     True if the specified point is inside, False otherwise.
    ///
    bool is_inside(const std::array<float, 3>& pos) const;

    ///
    /// Computes the solid angle at the query point.
    ///
    /// @param[in]  pos   Query position.
    ///
    /// @return     Solid angle at the query point.
    ///
    float solid_angle(const std::array<float, 3>& pos) const;

protected:
    /// Internal implementation.
    struct Impl;

    /// PIMPL to hide internal data structures.
    value_ptr<Impl> m_impl;
};

} // namespace winding
} // namespace lagrange
