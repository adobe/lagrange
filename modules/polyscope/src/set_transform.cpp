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
#include <lagrange/polyscope/set_transform.h>

#include <lagrange/polyscope/api.h>

namespace lagrange::polyscope {

template <typename Scalar>
void set_transform(
    ::polyscope::Structure& structure,
    const Eigen::Transform<Scalar, 3, Eigen::Affine>& transform)
{
    const auto& mat = transform.matrix();
    glm::mat4x4 m;
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            m[col][row] = static_cast<float>(mat(row, col));
        }
    }
    structure.setTransform(m);
}

template LA_POLYSCOPE_API void set_transform<float>(
    ::polyscope::Structure& structure,
    const Eigen::Transform<float, 3, Eigen::Affine>& transform);
template LA_POLYSCOPE_API void set_transform<double>(
    ::polyscope::Structure& structure,
    const Eigen::Transform<double, 3, Eigen::Affine>& transform);

} // namespace lagrange::polyscope
