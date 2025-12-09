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

namespace lagrange::primitive {

/// @addtogroup module-primitive
/// @{

///
/// Semantic labels are used to classify different parts of a primitive mesh.
///
enum class SemanticLabel : uint8_t {
    /// Side of the primitive, typically the lateral surface.
    Side = 0,

    /// Top of the primitive, typically the upper cap.
    Top = 1,

    /// Bottom of the primitive, typically the lower cap.
    Bottom = 2,

    /// Bevel of the primitive, typically the rounded edges.
    Bevel = 3,

    /// Cross section of the primitive, typically a slice through the middle.
    CrossSection = 4,

    /// Unknown or unclassified part of the primitive.
    Unknown = 5
};

/// @}

} // namespace lagrange::primitive
