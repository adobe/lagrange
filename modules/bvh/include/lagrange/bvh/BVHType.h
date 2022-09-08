/*
 * Copyright 2019 Adobe. All rights reserved.
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

#include <string>

namespace lagrange {
namespace bvh {

enum class BVHType { //
    NANOFLANN,
    IGL,
    UNKNOWN
};

inline std::string bvhtype_to_string(const BVHType in)
{
    switch (in) {
    case BVHType::NANOFLANN: return "NANOFLANN";
    case BVHType::IGL: return "IGL";
    case BVHType::UNKNOWN:
    default: return "UNKNOWN";
    }
}

inline BVHType string_to_bvhtype(const std::string in)
{
    if (in == "NANOFLANN") {
        return BVHType::NANOFLANN;
    } else if (in == "IGL") {
        return BVHType::IGL;
    } else {
        return BVHType::UNKNOWN;
    }
}

} // namespace bvh
} // namespace lagrange
