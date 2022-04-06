#
# Copyright 2020 Adobe. All rights reserved.
# This file is licensed to you under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License. You may obtain a copy
# of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under
# the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
# OF ANY KIND, either express or implied. See the License for the specific language
# governing permissions and limitations under the License.
#
if(TARGET lagrange::warnings)
    return()
endif()

# interface target for our warnings
add_library(lagrange_warnings INTERFACE)
add_library(lagrange::warnings ALIAS lagrange_warnings)

# installation
set_target_properties(lagrange_warnings PROPERTIES EXPORT_NAME warnings)
install(TARGETS lagrange_warnings EXPORT Lagrange_Targets)

include(lagrange_filter_flags)

# options
# More options can be found at:
# https://cmake.org/cmake/help/latest/variable/CMAKE_LANG_COMPILER_ID.html#variable:CMAKE_%3CLANG%3E_COMPILER_ID
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    # using Visual Studio C++ on Windows
    target_compile_definitions(lagrange_warnings INTERFACE _USE_MATH_DEFINES)
    target_compile_definitions(lagrange_warnings INTERFACE _CRT_SECURE_NO_WARNINGS)
    target_compile_definitions(lagrange_warnings INTERFACE _ENABLE_EXTENDED_ALIGNED_STORAGE)
    target_compile_definitions(lagrange_warnings INTERFACE NOMINMAX)

    set(options
        # using /Wall on Visual Studio is a bad idea, since it enables all
        # warnings that are disabled by default. /W4 is for lint-like warnings.
        # https://docs.microsoft.com/en-us/previous-versions/thxezb7y(v=vs.140)
        "/W3"

        "/wd4068" # disable: "Unknown pragma" warnings
        "/wd26812" # disable: Prefer 'enum class' over 'enum'
        "/wd26439" # disable: This kind of function may not throw (issues with Catch2)

        # Adding /bigobj. This is currently required to build Lagrange on
        # windows with debug info.
        # https://docs.microsoft.com/en-us/cpp/build/reference/bigobj-increase-number-of-sections-in-dot-obj-file
        "/bigobj")
    lagrange_filter_flags(options)
    target_compile_options(lagrange_warnings INTERFACE ${options})
else()
    # For non-MSVC compilers, see
    # https://cmake.org/cmake/help/latest/variable/CMAKE_LANG_COMPILER_ID.html
    # for a complete list.
    #
    # The ones we care about are "AppleClang", "Clang", "GNU" and "Intel".
    set(options
        -Wall
        -Werror=c++17-extensions
        -Werror=c++2a-extensions
        -Wextra
        -Wmissing-field-initializers
        -Wno-missing-braces
        -Wno-unknown-pragmas
        -Wshadow
        -msse3
    )

    if(LAGRANGE_MORE_WARNINGS)
    list(APPEND options
        -pedantic

        -Wconversion
        -Wunused

        -Wno-long-long
        -Wpointer-arith
        -Wformat=2
        -Wuninitialized
        -Wcast-qual
        -Wmissing-noreturn
        -Wmissing-format-attribute
        -Wredundant-decls

        -Werror=implicit
        -Werror=nonnull
        -Werror=init-self
        -Werror=main
        -Werror=sequence-point
        -Werror=return-type
        -Werror=trigraphs
        -Werror=array-bounds
        -Werror=write-strings
        -Werror=address
        -Werror=int-to-pointer-cast
        -Werror=pointer-to-int-cast

        -Wunused-variable
        -Wunused-but-set-variable
        -Wunused-parameter

        -Wold-style-cast
        -Wsign-conversion

        -Wstrict-null-sentinel
        -Woverloaded-virtual
        -Wsign-promo
        -Wstack-protector
        -Wstrict-aliasing
        -Wstrict-aliasing=2
        -Wswitch
        -Wswitch-default # incompatible with -Wcovered-switch-default
        -Wswitch-enum
        -Wswitch-unreachable

        -Wcast-align
        -Wdisabled-optimization
        -Winline # produces warning on default implicit destructor
        -Winvalid-pch
        -Wmissing-include-dirs
        -Wpacked
        -Wno-padded
        -Wstrict-overflow
        -Wstrict-overflow=2

        -Wctor-dtor-privacy
        -Wlogical-op
        -Wnoexcept
        -Woverloaded-virtual
        -Wnoexcept-type

        -Wnon-virtual-dtor
        -Wdelete-non-virtual-dtor
        -Werror=non-virtual-dtor
        -Werror=delete-non-virtual-dtor

        -Wno-sign-compare
        -Wdouble-promotion
        -fno-common

        ###########
        # GCC 6.1 #
        ###########

        -Wnull-dereference
        -fdelete-null-pointer-checks
        -Wduplicated-cond
        -Wmisleading-indentation

        ###########################
        # Enabled by -Weverything #
        ###########################

        # Useless until the following false positives are fixed:
        # https://github.com/llvm/llvm-project/issues/34492
        # https://github.com/llvm/llvm-project/issues/52977
        # -Wdocumentation

        -Wdocumentation-pedantic
        -Wdocumentation-html
        -Wdocumentation-deprecated-sync
        -Wdocumentation-unknown-command
        # -Wfloat-equal
        -fcomment-block-commands=cond

        # I would like to use this flag, but it's better for code coverage to always have a default: case
        # -Wcovered-switch-default # incompatible with gcc's -Wswitch-default

        -Wglobal-constructors
        -Wexit-time-destructors
        -Wmissing-variable-declarations
        -Wextra-semi
        -Wweak-vtables
        -Wno-source-uses-openmp
        -Wdeprecated
        -Wnewline-eof
        -Wmissing-prototypes

        -Wno-c++98-compat
        -Wno-c++98-compat-pedantic

        ###########################
        # Need to check if those are still valid today
        ###########################

        -Wimplicit-atomic-properties
        -Wmissing-declarations
        -Wmissing-prototypes
        -Wstrict-selector-match
        -Wundeclared-selector
        -Wunreachable-code
    )

        if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU")
            list(APPEND options
                # GCC pragma currently cannot silence -Wundef via #pragma, so we only enable this warning for clang
                # https://gcc.gnu.org/bugzilla/show_bug.cgi?id=53431
                -Wundef
            )
        endif()
    endif()

    lagrange_filter_flags(options)
    target_compile_options(lagrange_warnings INTERFACE ${options})
endif()

