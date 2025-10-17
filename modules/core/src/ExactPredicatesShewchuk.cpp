/*
 * Copyright 2021 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/ExactPredicatesShewchuk.h>

#include <iostream>
#include <memory>
#include <mutex>

namespace lagrange {

// Not exposed in the headers, to prevent accidentally calling without initialization.
void exactinit();
double orient2d(const double* pa, const double* pb, const double* pc);
double orient3d(const double* pa, const double* pb, const double* pc, const double* pd);
double incircle(const double* pa, const double* pb, const double* pc, const double* pd);
double
insphere(const double* pa, const double* pb, const double* pc, const double* pd, const double* pe);

ExactPredicatesShewchuk::ExactPredicatesShewchuk()
{
    // Make sure that exact_init() is only called once, even if multiple threads are
    // attempting to call it around the same time.
    // We can alternatively use Meyer's singleton.
    static std::once_flag once_flag;
    std::call_once(once_flag, []() { lagrange::exactinit(); });
}

short ExactPredicatesShewchuk::orient2D(double p1[2], double p2[2], double p3[2]) const
{
    auto r = ::lagrange::orient2d(p1, p2, p3);
    return (r == 0) ? 0 : ((r > 0) ? 1 : -1);
}

short ExactPredicatesShewchuk::orient3D(double p1[3], double p2[3], double p3[3], double p4[3])
    const
{
    auto r = ::lagrange::orient3d(p1, p2, p3, p4);
    return (r == 0) ? 0 : ((r > 0) ? 1 : -1);
}

short ExactPredicatesShewchuk::incircle(double p1[2], double p2[2], double p3[2], double p4[2])
    const
{
    auto r = ::lagrange::incircle(p1, p2, p3, p4);
    return (r == 0) ? 0 : ((r > 0) ? 1 : -1);
}

short ExactPredicatesShewchuk::insphere(
    double p1[3],
    double p2[3],
    double p3[3],
    double p4[3],
    double p5[3]) const
{
    auto r = ::lagrange::insphere(p1, p2, p3, p4, p5);
    return (r == 0) ? 0 : ((r > 0) ? 1 : -1);
}

} // namespace lagrange
