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
#include <Eigen/Core>
#include <lagrange/testing/common.h>

#include <iostream>
#include <vector>

#include <lagrange/Logger.h>
#include <lagrange/experimental/Array.h>
#include <lagrange/experimental/create_array.h>
#include <lagrange/utils/strings.h>

namespace {

using namespace lagrange::experimental;

template <typename ViewType>
void check_view(std::unique_ptr<ArrayBase>& A_array)
{
    auto A_view = A_array->view<ViewType>();
    REQUIRE(A_array->is_row_major() == (bool)ViewType::IsRowMajor);
    REQUIRE(A_array->data() == A_view.data());
    REQUIRE(A_array->rows() == A_view.rows());
    REQUIRE(A_array->cols() == A_view.cols());

    auto A_view_2 = A_array->view<ViewType>();

    A_view.setZero();
    REQUIRE(A_view_2.maxCoeff() == 0);
    REQUIRE(A_view_2.minCoeff() == 0);

    A_view.setOnes();
    REQUIRE(A_view_2.maxCoeff() == 1);
    REQUIRE(A_view_2.minCoeff() == 1);
}

template <typename RefType>
void check_ref(std::unique_ptr<ArrayBase>& A_array)
{
    auto& A_ref = A_array->get<RefType>();
    REQUIRE(A_array->is_row_major() == (bool)RefType::IsRowMajor);
    REQUIRE(A_array->data() == A_ref.data());
    REQUIRE(A_array->rows() == A_ref.rows());
    REQUIRE(A_array->cols() == A_ref.cols());

    auto A_copy = A_array->get<RefType>();
    REQUIRE(A_array->data() != A_copy.data());

    A_ref.setZero();
    A_copy.setOnes();

    REQUIRE(A_ref.minCoeff() == 0);
    REQUIRE(A_ref.maxCoeff() == 0);
    REQUIRE(A_copy.minCoeff() == 1);
    REQUIRE(A_copy.maxCoeff() == 1);
}

template <typename ViewType, typename Index>
void check_resize(std::unique_ptr<ArrayBase>& A_array, Index rows, Index cols)
{
    // TODO: A drawback of resize is that it can invalidate existing views,
    // which will leads to seg fault.  Not sure resize should be supported.
    REQUIRE_NOTHROW(A_array->resize(rows, cols));

    REQUIRE(A_array->rows() == rows);
    REQUIRE(A_array->cols() == cols);

    check_view<ViewType>(A_array);
}

} // namespace

TEST_CASE("experimental/Array.h", "[array]")
{
    using namespace lagrange;

    SECTION("Create array")
    {
        Eigen::MatrixXd A(3, 3);
        A.setIdentity();

        auto A_array = experimental::create_array(A);
        REQUIRE(A.data() != A_array->data());

        check_view<Eigen::Matrix3d>(A_array);
        check_ref<Eigen::MatrixXd>(A_array);

        using MatrixType =
            Eigen::Matrix<int, Eigen::Dynamic, 3, Eigen::RowMajor | Eigen::AutoAlign>;
        MatrixType B(3, 3);
        B.setIdentity();

        auto B_array = experimental::create_array(B);
        REQUIRE(B_array->is_row_major());
        REQUIRE(B.data() != B_array->data());

        check_view<MatrixType>(B_array);
        check_ref<MatrixType>(B_array);

        REQUIRE_THROWS(B_array->view<Eigen::MatrixXd>());
    }

    SECTION("Create array with const Eigen matrix")
    {
        const Eigen::MatrixXd A = Eigen::MatrixXd::Identity(3, 3);
        auto A_array = experimental::create_array(A);
        REQUIRE(A.data() != A_array->data());
        check_view<Eigen::MatrixXd>(A_array);
        check_ref<Eigen::MatrixXd>(A_array);

        auto A_view = A_array->template view<const Eigen::MatrixXd>();
        REQUIRE(A_array->data() == A_view.data());

        auto& A_ref = A_array->template get<const Eigen::MatrixXd>();
        REQUIRE(A_array->data() == A_ref.data());
    }

    SECTION("Create array with r-values")
    {
        auto A_array = experimental::create_array(Eigen::Matrix3d::Identity().eval());
        check_view<Eigen::Matrix3d>(A_array);
        check_ref<Eigen::Matrix3d>(A_array);
    }

    SECTION("Create with move")
    {
        Eigen::MatrixXd A(3, 3);
        A.setIdentity();

        const void* ptr = A.data();

        auto A_array = experimental::create_array(std::move(A));
        REQUIRE(A_array->data() == ptr);

        check_view<Eigen::Matrix3d>(A_array);
        check_ref<Eigen::MatrixXd>(A_array);
    }

    SECTION("Create from a block")
    {
        Eigen::Matrix3d A(3, 3);
        A.setIdentity();

        auto A_array = experimental::create_array(A.block(0, 0, 2, 2));
        REQUIRE_FALSE(A_array->is_row_major());

        check_view<Eigen::Matrix2d>(A_array);

        // The following line does not work because the data is not stored as
        // exactly as the type Eigen::MatrixXd.
        // check_ref<Eigen::MatrixXd>(A_array);
    }

    SECTION("Create from a const block")
    {
        const Eigen::Matrix3d A = Eigen::Matrix3d::Identity();

        auto A_array = experimental::create_array(A.block(0, 0, 2, 2));
        REQUIRE_FALSE(A_array->is_row_major());

        check_view<Eigen::Matrix2d>(A_array);
    }

    SECTION("Create from a map")
    {
        using Scalar = uint64_t;
        std::vector<Scalar> A(9, 0);
        Eigen::Map<Eigen::Matrix<Scalar, 3, 3, Eigen::RowMajor>> A_map(A.data(), 3, 3);

        auto A_array = experimental::create_array(A_map);
        REQUIRE(A_array->is_row_major());

        check_view<Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>>(A_array);
    }

    SECTION("Create from a const map")
    {
        using Scalar = uint64_t;
        std::vector<Scalar> A(9, 0);
        const Eigen::Map<Eigen::Matrix<Scalar, 3, 3, Eigen::RowMajor>> A_map(A.data(), 3, 3);

        auto A_array = experimental::create_array(A_map);
        REQUIRE(A_array->is_row_major());

        check_view<Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>>(A_array);
    }

    SECTION("Wrap with array")
    {
        Eigen::MatrixXd A(3, 3);
        A.setIdentity();

        auto A_array = experimental::wrap_with_array(A);
        REQUIRE_FALSE(A_array->is_row_major());
        REQUIRE_THROWS(A_array->get<Eigen::Matrix3d>());

        check_view<Eigen::Matrix3d>(A_array);
        check_ref<Eigen::MatrixXd>(A_array);

        SECTION("What happens if I resize?") { check_resize<Eigen::Matrix4d>(A_array, 4, 4); }
    }

    SECTION("Wrap const EigenType")
    {
        const Eigen::MatrixXd A = Eigen::MatrixXd::Zero(3, 3);

        auto A_array = experimental::wrap_with_array(A);
        REQUIRE_FALSE(A_array->is_row_major());
        REQUIRE_THROWS(A_array->get<Eigen::Matrix3d>());

        auto A_view = A_array->template view<const Eigen::MatrixXd>();
        REQUIRE(A_array->data() == A_view.data());

        auto& A_ref = A_array->template get<const Eigen::MatrixXd>();
        REQUIRE(A_array->data() == A_ref.data());
    }

    SECTION("Wrap rvalue")
    {
        // The following block will not compile because Array cannot wrap around
        // r-value reference to data owning objects.
        //
        // Eigen::MatrixXd A(3, 3);
        // A.setIdentity();
        // auto A_array = experimental::wrap_with_array(std::move(A));

        // The following block will not compile for the same reason.
        // auto A_array = experimental::wrap_with_array(Eigen::Matrix3d::Zero().eval());

        // However, it is possible to wrap around r-value reference of
        // Eigen::Map because Eigen::Map does not own the memory.
        std::vector<float> A(9, 0);
        auto A_array = experimental::wrap_with_array(Eigen::Map<Eigen::Matrix3f>(A.data(), 3, 3));
        REQUIRE(A_array->data() == A.data());
    }

    SECTION("Wrap raw array")
    {
        using Scalar = uint64_t;
        std::vector<Scalar> A(9);
        auto A_array = experimental::wrap_with_array(A.data(), 3, 3);

        REQUIRE(A_array->rows() == 3);
        REQUIRE(A_array->cols() == 3);
        REQUIRE(A_array->data() == A.data());

        check_view<Eigen::Matrix<Scalar, 3, 3, Eigen::RowMajor>>(A_array);

        // Resizing RawArray is disabled since it does not own the memory
        // and resizing may invalidate the memory.
        REQUIRE_THROWS(A_array->resize(4, 4));
    }

    SECTION("Wrap const raw array")
    {
        using Scalar = uint64_t;
        const std::vector<Scalar> A(9, 1);
        auto A_array = experimental::wrap_with_array(A.data(), 3, 3);

        REQUIRE(A_array->rows() == 3);
        REQUIRE(A_array->cols() == 3);
        REQUIRE(A_array->data() == A.data());

        // check_view<Eigen::Matrix<Scalar, 3, 3, Eigen::RowMajor>>(A_array);

        //// Resizing RawArray is disabled since it does not own the memory
        //// and resizing may invalidate the memory.
        // REQUIRE_THROWS(A_array->resize(4, 4));
    }

    SECTION("Wrap Eigen map")
    {
        using Scalar = uint64_t;
        std::vector<Scalar> A(9, 0);
        Eigen::Map<Eigen::Matrix<Scalar, 3, 3, Eigen::RowMajor>> A_map(A.data(), 3, 3);

        auto A_array = experimental::wrap_with_array(A_map);
        REQUIRE(A.data() == A_array->data());
        REQUIRE(A_array->is_row_major());

        check_view<Eigen::Matrix<Scalar, Eigen::Dynamic, 3, Eigen::RowMajor>>(A_array);

        // Resizing wrapped map is disabled since it does not own the memory
        // and resizing may invalidate the memory.
        REQUIRE_THROWS(A_array->resize(4, 4));
    }

    SECTION("Wrap const Eigen map")
    {
        using Scalar = uint64_t;
        std::vector<Scalar> A(9, 0);
        using EigenType = Eigen::Matrix<Scalar, 3, 3, Eigen::RowMajor>;
        using MapType = Eigen::Map<const EigenType>;
        MapType A_map(A.data(), 3, 3);

        auto A_array = experimental::wrap_with_array(A_map);
        REQUIRE(A.data() == A_array->data());
        REQUIRE(A_array->is_row_major());

        const auto A_view = A_array->template view<
            const Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>>();
        REQUIRE(A_array->data() == A_view.data());
        REQUIRE(A_view == A_map);
    }


    SECTION("Interop check with std vector of eigen vectors")
    {
        // This checks the assumption that fixed size Eigen objects does not
        // store extra information besides raw data.
        static_assert(
            sizeof(Eigen::Vector3f) == 3 * sizeof(float),
            "Size of Eigen::Vector3f is not 3 * sizeof(float)");

        using element_type = Eigen::Vector3f;
        std::vector<element_type, Eigen::aligned_allocator<element_type>> A(
            3,
            element_type::Ones());
        auto A_array = experimental::wrap_with_array(reinterpret_cast<float*>(A.data()), 3, 3);

        REQUIRE(A_array->rows() == 3);
        REQUIRE(A_array->cols() == 3);
        REQUIRE(A_array->data() == A.data());

        check_view<Eigen::Matrix<float, 3, 3, Eigen::RowMajor>>(A_array);
        REQUIRE_THROWS(A_array->resize(4, 4));
    }

    SECTION("Interop check with std vector of eigen vectors on vectorizable data")
    {
        // This checks the assumption that fixed size Eigen objects does not
        // store extra information besides raw data.
        static_assert(
            sizeof(Eigen::Vector4f) == 4 * sizeof(float),
            "Size of Eigen::Vector4f is not 4 * sizeof(float)");

        using element_type = Eigen::Vector4f;
        std::vector<element_type, Eigen::aligned_allocator<element_type>> A(
            4,
            element_type::Ones());
        auto A_array = experimental::wrap_with_array(reinterpret_cast<float*>(A.data()), 4, 4);

        REQUIRE(A_array->rows() == 4);
        REQUIRE(A_array->cols() == 4);
        REQUIRE(A_array->data() == A.data());

        check_view<Eigen::Matrix<float, 4, 4, Eigen::RowMajor>>(A_array);
        REQUIRE_THROWS(A_array->resize(5, 5));
    }

    SECTION("Row slice")
    {
        Eigen::MatrixXd A(3, 3);
        A.setIdentity();
        std::vector<int> row_indices;
        row_indices.push_back(0);
        row_indices.push_back(2);
        row_indices.push_back(0);
        auto mapping_fn =
            [&row_indices](Eigen::Index i, std::vector<std::pair<Eigen::Index, double>>& weights) {
                weights.clear();
                weights.emplace_back(row_indices[i], 0.2);
                weights.emplace_back(row_indices[i], 0.8);
            };

        auto validate = [&A](auto& B_array) {
            REQUIRE(B_array != nullptr);

            auto B = B_array->template get<Eigen::MatrixXd>();
            REQUIRE(B.row(0) == A.row(0));
            REQUIRE(B.row(1) == A.row(2));
            REQUIRE(B.row(2) == A.row(0));
        };

        auto run = [&](const auto& A_array) {
            auto B_array = A_array->row_slice(row_indices);
            validate(B_array);
            auto C_array = A_array->row_slice(row_indices.size(), mapping_fn);
            validate(C_array);
        };

        SECTION("EigenArray")
        {
            auto A_array = experimental::create_array(A);
            run(A_array);
        }

        SECTION("EigenArrayRef")
        {
            auto A_array = experimental::wrap_with_array(A);
            run(A_array);
        }

        SECTION("const EigenArrayRef")
        {
            auto A_array = experimental::wrap_with_array(const_cast<const Eigen::MatrixXd&>(A));
            run(A_array);
        }

        SECTION("RawArray")
        {
            auto A_array = experimental::
                wrap_with_array<double, int, Eigen::Dynamic, Eigen::Dynamic, Eigen::ColMajor>(
                    A.data(),
                    3,
                    3);
            run(A_array);
        }

        SECTION("const RawArray")
        {
            auto A_array = experimental::
                wrap_with_array<double, int, Eigen::Dynamic, Eigen::Dynamic, Eigen::ColMajor>(
                    const_cast<const double*>(A.data()),
                    3,
                    3);
            run(A_array);
        }
    }

    SECTION("Set with incompatible storage order")
    {
        Eigen::Matrix<int, Eigen::Dynamic, Eigen::Dynamic> M1(2, 2);
        M1 << 1, 2, 3, 4;
        Eigen::Matrix<int, 2, 2, Eigen::RowMajor> M2 = M1.transpose();
        const void* ptr = M1.data();

        auto A = experimental::create_array(std::move(M1));
        REQUIRE(A->data() == ptr);
        REQUIRE(A->data<int>()[1] == 3);
        REQUIRE(A->data<int>()[2] == 2);

        A->set(M2);
        REQUIRE(A->data() == ptr);
        REQUIRE(A->data<int>()[1] == 2);
        REQUIRE(A->data<int>()[2] == 3);
    }
}
