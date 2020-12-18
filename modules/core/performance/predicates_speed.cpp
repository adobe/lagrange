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
#include <cmath>
#include <iostream>

#include <lagrange/ExactPredicates.h>
#include <lagrange/common.h>
#include <lagrange/utils/timing.h>

void test_orient2D(lagrange::ExactPredicates& predicates)
{
    const size_t N = (size_t)1e6;
    double p1[2] = {-1e-12, 0.0};
    double p2[2] = {1e-12, 0.0};
    double p3[2];
    lagrange::timestamp_type start, finish;
    lagrange::get_timestamp(&start);
    for (size_t i = 0; i < N + 1; i++) {
        p3[0] = cos(M_PI * 2 * double(i) / double(N));
        p3[1] = sin(M_PI * 2 * double(i) / double(N));
        predicates.orient2D(p1, p2, p3);
    }
    lagrange::get_timestamp(&finish);
    double duration = lagrange::timestamp_diff_in_seconds(start, finish);
    std::cout << "orient2D:" << std::endl;
    std::cout << "Total running time: " << duration << " secs." << std::endl;
    std::cout << "Average: " << duration / N * 1e6 << " µs per call" << std::endl;
}

void test_orient3D(lagrange::ExactPredicates& predicates)
{
    const size_t N = (size_t)1e6;
    double p1[3] = {-1e-12, 0.0, 1e-10};
    double p2[3] = {1e-12, 0.0, -1e-10};
    double p3[3] = {0.0, 1e-16, 0.0};
    double p4[3];
    lagrange::timestamp_type start, finish;
    lagrange::get_timestamp(&start);
    for (size_t i = 0; i < N + 1; i++) {
        p4[0] = cos(M_PI * 2 * double(i) / double(N));
        p4[1] = sin(M_PI * 2 * double(i) / double(N));
        p4[2] = 1e-16;
        predicates.orient3D(p1, p2, p3, p4);
    }
    lagrange::get_timestamp(&finish);
    double duration = lagrange::timestamp_diff_in_seconds(start, finish);
    std::cout << "orient3D:" << std::endl;
    std::cout << "Total running time: " << duration << " secs." << std::endl;
    std::cout << "Average: " << duration / N * 1e6 << " µs per call" << std::endl;
}

void test_incircle(lagrange::ExactPredicates& predicates)
{
    const size_t N = (size_t)1e6;
    double p1[2] = {-1e-12, 0.0};
    double p2[2] = {1e-12, 0.0};
    double p3[2];
    double p4[2] = {1e-16, -1e-15};
    lagrange::timestamp_type start, finish;
    lagrange::get_timestamp(&start);
    for (size_t i = 0; i < N + 1; i++) {
        p3[0] = cos(M_PI * 2 * double(i) / double(N));
        p3[1] = sin(M_PI * 2 * double(i) / double(N));
        predicates.incircle(p1, p2, p3, p4);
    }
    lagrange::get_timestamp(&finish);
    double duration = lagrange::timestamp_diff_in_seconds(start, finish);
    std::cout << "incircle:" << std::endl;
    std::cout << "Total running time: " << duration << " secs." << std::endl;
    std::cout << "Average: " << duration / N * 1e6 << " µs per call" << std::endl;
}

void test_insphere(lagrange::ExactPredicates& predicates)
{
    const size_t N = (size_t)1e6;
    double p1[3] = {-1e-12, 0.0, 1e-10};
    double p2[3] = {1e-12, 0.0, -1e-10};
    double p3[3] = {0.0, 1e-16, 0.0};
    double p4[3];
    double p5[3] = {2e-16, 1e-3, 1e-9};
    lagrange::timestamp_type start, finish;
    lagrange::get_timestamp(&start);
    for (size_t i = 0; i < N + 1; i++) {
        p4[0] = cos(M_PI * 2 * double(i) / double(N)) * 1e-6;
        p4[1] = sin(M_PI * 2 * double(i) / double(N)) * 1e-6;
        p4[2] = 1e-16;
        predicates.insphere(p1, p2, p3, p4, p5);
    }
    lagrange::get_timestamp(&finish);
    double duration = lagrange::timestamp_diff_in_seconds(start, finish);
    std::cout << "insphere:" << std::endl;
    std::cout << "Total running time: " << duration << " secs." << std::endl;
    std::cout << "Average: " << duration / N * 1e6 << " µs per call" << std::endl;
}

int main()
{
    auto predicates = lagrange::ExactPredicates::create("shewchuk");

    test_orient2D(*predicates);
    test_orient3D(*predicates);
    test_incircle(*predicates);
    test_insphere(*predicates);

    return 0;
}
