// Diagnostic eigen_assert override for Windows ARM64 investigation.
// Prints the address, alignment, and call site, then continues so we can see
// every misaligned construction without aborting the process.
#pragma once

#ifdef LAGRANGE_DIAG_EIGEN_ALIGN

#include <atomic>
#include <cstdio>
#include <cstdint>
#include <cstring>

namespace lagrange_diag {
inline std::atomic<int>& eigen_diag_count()
{
    static std::atomic<int> n{0};
    return n;
}

inline void log_eigen_assert(const char* expr, const char* file, int line)
{
    int n = eigen_diag_count().fetch_add(1);
    if (n < 30) {
        std::fprintf(stderr, "[EIGEN_DIAG #%d] %s:%d  %s\n", n, file, line, expr);
        std::fflush(stderr);
    }
}
} // namespace lagrange_diag

// Replace eigen_assert with a non-fatal diagnostic version. We must define this BEFORE
// any Eigen header is included; this file is force-included via /FI on MSVC and -include on GCC/Clang.
#define eigen_assert(x)                                                             \
    do {                                                                            \
        if (!(x)) {                                                                 \
            ::lagrange_diag::log_eigen_assert(#x, __FILE__, __LINE__);              \
        }                                                                           \
    } while (0)

#endif // LAGRANGE_DIAG_EIGEN_ALIGN
