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

#ifdef LA_SOLVER_ACCELERATE
    #include <lagrange/solver/internal/AccelerateSupport.h>
#else
    #include <lagrange/utils/build.h>
    #if LAGRANGE_TARGET_OS(WASM) || defined(LA_SANITIZE_THREAD) || !defined(LA_SOLVER_MKL)
        #include <Eigen/SparseCholesky>
    #else
        #include <Eigen/PardisoSupport>
    #endif
#endif

namespace lagrange::solver {

#ifdef LA_SOLVER_ACCELERATE
template <typename MatrixType>
using SolverLDLT = lagrange::solver::internal::AccelerateLDLT<MatrixType>;
#else
    #if LAGRANGE_TARGET_OS(WASM) || defined(LA_SANITIZE_THREAD) || !defined(LA_SOLVER_MKL)
template <typename MatrixType>
using SolverLDLT = Eigen::SimplicialLDLT<MatrixType>;
    #else
template <typename MatrixType>
using SolverLDLT = Eigen::PardisoLDLT<MatrixType>;
    #endif
#endif

} // namespace lagrange::solver
