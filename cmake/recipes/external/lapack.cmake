if(TARGET LAPACK::LAPACK)
    return()
endif()

message(STATUS "Third-party (external): creating target 'LAPACK::LAPACK'")

if("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "arm64" OR "${CMAKE_OSX_ARCHITECTURES}" MATCHES "arm64")
    # Use Accelerate on macOS M1
    set(BLA_VENDOR Apple)
    find_package(LAPACK REQUIRED)
else()
    # Use MKL on other platforms
    include(mkl)
    add_library(LAPACK::LAPACK ALIAS mkl::mkl)
endif()
