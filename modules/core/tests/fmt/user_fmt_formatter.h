/*
 * Copyright 2024 Adobe. All rights reserved.
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

#pragma message("Using user-provided fmt::formatter<> for Eigen types")

template <typename T>
struct fmt::formatter<T, std::enable_if_t<std::is_base_of<Eigen::DenseBase<T>, T>::value, char>>
    : fmt::nested_formatter<typename T::Scalar>
{
    auto format(T const& a, format_context& ctx) const
    {
        return this->write_padded(ctx, [&](auto out) {
            for (Eigen::Index ir = 0; ir < a.rows(); ir++) {
                for (Eigen::Index ic = 0; ic < a.cols(); ic++) {
                    out = fmt::format_to(out, "{} ", this->nested(a(ir, ic)));
                }
                out = fmt::format_to(out, "\n");
            }
            return out;
        });
    }
};
