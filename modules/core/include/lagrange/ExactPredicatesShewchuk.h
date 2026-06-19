/*
 * Copyright 2016 Adobe. All rights reserved.
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

#include <lagrange/ExactPredicates.h>

namespace lagrange {

///
/// @addtogroup module-core
/// @{
///

class LA_CORE_API ExactPredicatesShewchuk : public ExactPredicates
{
public:
    ExactPredicatesShewchuk();

public:
    ///
    /// @copydoc ExactPredicates::orient2D
    ///
    virtual short orient2D(const double p1[2], const double p2[2], const double p3[2]) const;

    ///
    /// @copydoc ExactPredicates::orient3D
    ///
    virtual short
    orient3D(const double p1[3], const double p2[3], const double p3[3], const double p4[3]) const;

    ///
    /// @copydoc ExactPredicates::incircle
    ///
    virtual short
    incircle(const double p1[2], const double p2[2], const double p3[2], const double p4[2]) const;

    ///
    /// @copydoc ExactPredicates::insphere
    ///
    virtual short insphere(
        const double p1[3],
        const double p2[3],
        const double p3[3],
        const double p4[3],
        const double p5[3]) const;
};

/// @}

} // namespace lagrange
