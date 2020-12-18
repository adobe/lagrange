/*
 * Copyright 2020 Adobe. All rights reserved.
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

#include <memory>
#include <string>

#include <lagrange/common.h>

namespace lagrange {

class ExactPredicates
{
public:
    static std::unique_ptr<ExactPredicates> create(const std::string& engine);

public:
    ExactPredicates() = default;
    virtual ~ExactPredicates() = default;

public:
    virtual short orient2D(double p1[2], double p2[2], double p3[2]) const = 0;

    virtual short orient3D(double p1[3], double p2[3], double p3[3], double p4[3]) const = 0;

    virtual short incircle(double p1[2], double p2[2], double p3[2], double p4[2]) const = 0;

    virtual short insphere(double p1[3], double p2[3], double p3[3], double p4[3], double p5[3])
        const = 0;
};
} // namespace lagrange
