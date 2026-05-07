// Diagnostic Eigen assertion override for Windows ARM64 Debug investigation.
// Replaces eigen_assert with a non-fatal handler that captures a stack trace
// via cpptrace whenever the failing call originates from DenseStorage.h (i.e.
// the alignment check on plain_array<>). The actual cpptrace call lives in
// eigen_alignment_diag.cpp so this header has no dependency on cpptrace.
//
// Force-included via /FI on MSVC (see cmake/recipes/external/Eigen3.cmake).
// Pre-defining eigen_assert here works because Eigen/src/Core/util/Macros.h
// guards its own definition with #ifndef eigen_assert.
#pragma once

#ifdef LAGRANGE_DIAG_EIGEN_ALIGN

namespace lagrange_diag {
void eigen_assert_handler(const char* expr, const char* file, int line);
} // namespace lagrange_diag

#define eigen_assert(x)                                                              \
    do {                                                                             \
        if (!(x)) {                                                                  \
            ::lagrange_diag::eigen_assert_handler(#x, __FILE__, __LINE__);           \
        }                                                                            \
    } while (0)

#endif // LAGRANGE_DIAG_EIGEN_ALIGN
