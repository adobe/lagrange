if(TARGET BLAS::BLAS)
    return()
endif()

message(STATUS "Third-party (external): creating target 'BLAS::BLAS'")

if("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "arm64" OR "${CMAKE_OSX_ARCHITECTURES}" MATCHES "arm64")
    # Use Accelerate on macOS M1
    set(BLA_VENDOR Apple)
    find_package(BLAS REQUIRED)
else()
    # Use MKL on other platforms
    include(mkl)
    add_library(BLAS::BLAS ALIAS mkl::mkl)
endif()
