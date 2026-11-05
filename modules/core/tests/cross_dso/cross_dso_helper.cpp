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
// Native MODULE / Emscripten SIDE_MODULE loaded by test_cross_dso_down_cast.cpp.
#include <lagrange/experimental/Array.h>

#include <Eigen/Core>

#if defined(__GNUC__) || defined(__clang__)
    #define LAGRANGE_TEST_CROSS_DSO_EXPORT extern "C" __attribute__((visibility("default")))
#else
    #define LAGRANGE_TEST_CROSS_DSO_EXPORT extern "C"
#endif

using CrossDsoArrayImpl = lagrange::experimental::EigenArray<Eigen::MatrixXd>;

LAGRANGE_TEST_CROSS_DSO_EXPORT lagrange::experimental::ArrayBase*
lagrange_test_create_cross_dso_array()
{
    Eigen::MatrixXd data(2, 2);
    data << 1, 2, 3, 4;
    return new CrossDsoArrayImpl(data);
}

LAGRANGE_TEST_CROSS_DSO_EXPORT void lagrange_test_destroy_cross_dso_array(
    lagrange::experimental::ArrayBase* array)
{
    delete array;
}
