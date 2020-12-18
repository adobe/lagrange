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

// TODO: Move this file to lagrange::core?

////////////////////////////////////////////////////////////////////////////////
#include <Eigen/Dense>
////////////////////////////////////////////////////////////////////////////////

namespace lagrange {

template <typename Scalar, size_t N, typename Derived>
void vector_to_eigen(
    const std::vector<std::array<Scalar, N>> &from,
    Eigen::PlainObjectBase<Derived> &to)
{
    if (from.size() == 0) {
        to.resize(0, N);
        return;
    }
    to.resize(from.size(), N);
    for (size_t i = 0; i < from.size(); ++i) {
        for (size_t j = 0; j < N; ++j) {
            to(i, j) = from[i][j];
        }
    }
}

template <typename Scalar, typename Derived>
void vector_to_eigen(
    const std::vector<std::pair<Scalar, Scalar>> &from,
    Eigen::PlainObjectBase<Derived> &to)
{
    if (from.size() == 0) {
        to.resize(0, 2);
        return;
    }
    to.resize(from.size(), 2);
    for (size_t i = 0; i < from.size(); ++i) {
        to.row(i) << from[i].first, from[i].second;
    }
}

template <typename Scalar, int N, typename Derived>
void vector_to_eigen(
    const std::vector<Eigen::Matrix<Scalar, N, 1>> &from,
    Eigen::PlainObjectBase<Derived> &to)
{
    if (from.size() == 0) {
        to.resize(0, N);
        return;
    }
    to.resize(from.size(), N);
    for (size_t i = 0; i < from.size(); ++i) {
        for (size_t j = 0; j < N; ++j) {
            to(i, j) = from[i][j];
        }
    }
}

template <typename Scalar, int N, typename Derived>
void vector_to_eigen(
    const std::vector<Eigen::Matrix<Scalar, 1, N>> &from,
    Eigen::PlainObjectBase<Derived> &to)
{
    if (from.size() == 0) {
        to.resize(0, N);
        return;
    }
    to.resize(from.size(), N);
    for (size_t i = 0; i < from.size(); ++i) {
        for (size_t j = 0; j < N; ++j) {
            to(i, j) = from[i][j];
        }
    }
}

template <typename Scalar>
void vector_to_eigen(const std::vector<Scalar> &from, Eigen::Matrix<Scalar, Eigen::Dynamic, 1> &to)
{
    if (from.size() == 0) {
        to.resize(0);
        return;
    }
    to.resize(from.size(), 1);
    for (size_t i = 0; i < from.size(); ++i) {
        to(i) = from[i];
    }
}

template <typename Scalar, typename Derived>
void flat_vector_to_eigen(
    const std::vector<Scalar> &from,
    Eigen::PlainObjectBase<Derived> &to,
    size_t rows,
    size_t cols,
    int row_major_flag = Eigen::RowMajor)
{
    LA_ASSERT(rows * cols == from.size(), "Invalid vector size");
    to.resize(rows, cols);
    const bool is_row_major = (row_major_flag & Eigen::RowMajor);
    for (size_t i = 0; i < to.size(); ++i) {
        size_t r = (is_row_major ? i / cols : i % rows);
        size_t c = (is_row_major ? i % cols : i / rows);
        to(r, c) = from[i];
    }
}

template <typename Derived, typename Scalar, size_t N>
void eigen_to_vector(const Eigen::MatrixBase<Derived> &from, std::vector<std::array<Scalar, N>> &to)
{
    if (from.cols() != N) {
        throw std::invalid_argument("Wrong number of columns");
    }
    to.resize(from.rows());
    for (size_t i = 0; i < to.size(); ++i) {
        for (size_t j = 0; j < N; ++j) {
            to[i][j] = from(i, j);
        }
    }
}

template <typename Derived, typename Scalar>
void eigen_to_vector(
    const Eigen::MatrixBase<Derived> &from,
    std::vector<std::pair<Scalar, Scalar>> &to)
{
    if (from.cols() != 2) {
        throw std::invalid_argument("Wrong number of columns");
    }
    to.resize(from.rows());
    for (size_t i = 0; i < to.size(); ++i) {
        to[i] = std::make_pair(from[i][0], from[i][1]);
    }
}

template <typename Derived, typename Scalar>
void eigen_to_flat_vector(
    const Eigen::MatrixBase<Derived> &from,
    std::vector<Scalar> &to,
    int row_major_flag = Eigen::RowMajor)
{
    to.resize(from.size());
    const bool is_row_major = (row_major_flag & Eigen::RowMajor);
    for (Eigen::Index i = 0; i < static_cast<Eigen::Index>(to.size()); ++i) {
        Eigen::Index r = (is_row_major ? i / from.cols() : i % from.rows());
        Eigen::Index c = (is_row_major ? i % from.cols() : i / from.rows());
        to[i] = from(r, c);
    }
}

} // namespace lagrange
