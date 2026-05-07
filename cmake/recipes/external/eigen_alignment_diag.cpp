// See eigen_alignment_diag.h. Defined in a separate translation unit so the
// cpptrace dependency stays private to this static library and does not leak
// into every consumer of Eigen3::Eigen.
#include "eigen_alignment_diag.h"

#ifdef LAGRANGE_DIAG_EIGEN_ALIGN

#include <cpptrace/cpptrace.hpp>

#include <atomic>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <mutex>

namespace lagrange_diag {

namespace {
std::atomic<int>& trace_count()
{
    static std::atomic<int> n{0};
    return n;
}

std::mutex& trace_mutex()
{
    static std::mutex m;
    return m;
}

bool is_alignment_assert(const char* file)
{
    if (!file) return false;
    return std::strstr(file, "DenseStorage") != nullptr;
}
} // namespace

void eigen_assert_handler(const char* expr, const char* file, int line)
{
    if (is_alignment_assert(file)) {
        // Alignment assertion: log + capture trace, but DO NOT abort. ARM64 NEON
        // tolerates unaligned access; we only need to find where the misaligned
        // plain_array<> is being constructed.
        int n = trace_count().fetch_add(1);
        if (n < 30) {
            std::lock_guard<std::mutex> lock(trace_mutex());
            std::fprintf(
                stderr,
                "\n[EIGEN_DIAG #%d] %s:%d  %s\n",
                n,
                file,
                line,
                expr ? expr : "(null)");
            cpptrace::generate_trace(/*skip*/ 1, /*max*/ 64).print(std::cerr);
            std::fflush(stderr);
        }
        return;
    }
    // Any other Eigen invariant violation: preserve original assert() behavior
    // so unrelated bugs still surface (and tests fail) rather than being masked.
    std::fprintf(stderr, "Eigen assertion failed: %s at %s:%d\n", expr, file, line);
    std::fflush(stderr);
    std::abort();
}

} // namespace lagrange_diag

#endif // LAGRANGE_DIAG_EIGEN_ALIGN
