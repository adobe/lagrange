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

#include <openvdb/Grid.h>

namespace lagrange::volume {

// clang-format off

/// Alias for OpenVDB's common grid types.
template <typename T>
using Grid = openvdb::Grid<
    // The following type is the same as
    //
    // @code
    // typename openvdb::tree::Tree4<Scalar, 5, 4, 3>::Type
    // @endcode
    //
    // However, the `::Type` indirection prevents the compiler from deducing the grid scalar type
    // automatically, so we use this direct alias. We ensure that the types are the same with a
    // static_assert in our .cpp files.
    openvdb::tree::Tree<
        openvdb::tree::RootNode<
            openvdb::tree::InternalNode<
                openvdb::tree::InternalNode<
                    openvdb::tree::LeafNode<T, 3>,
                    4>,
                5
            >
        >
    >
>;

// clang-format on

} // namespace lagrange::volume
