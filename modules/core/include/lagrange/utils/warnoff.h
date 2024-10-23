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
#if defined(__clang__)
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wunknown-warning-option"
    // Not all warnings are supported by older version of clang.
    // So the line above has to come first.
    #pragma clang diagnostic ignored "-Wdeprecated"
    #pragma clang diagnostic ignored "-Wdeprecated-declarations"
    #pragma clang diagnostic ignored "-Wdeprecated-dynamic-exception-spec"
    #pragma clang diagnostic ignored "-Wdeprecated-register"
    #pragma clang diagnostic ignored "-Wdocumentation"
    #pragma clang diagnostic ignored "-Wdocumentation-unknown-command"
    #pragma clang diagnostic ignored "-Wexit-time-destructors"
    #pragma clang diagnostic ignored "-Wextra-semi"
    #pragma clang diagnostic ignored "-Wglobal-constructors"
    #pragma clang diagnostic ignored "-Wignored-qualifiers"
    #pragma clang diagnostic ignored "-Wmissing-noreturn"
    #pragma clang diagnostic ignored "-Wnewline-eof"
    #pragma clang diagnostic ignored "-Wold-style-cast"
    #pragma clang diagnostic ignored "-Wrange-loop-analysis"
    #pragma clang diagnostic ignored "-Wreorder"
    #pragma clang diagnostic ignored "-Wreorder-ctor"
    #pragma clang diagnostic ignored "-Wshadow"
    #pragma clang diagnostic ignored "-Wsign-compare"
    #pragma clang diagnostic ignored "-Wsign-conversion"
    #pragma clang diagnostic ignored "-Wundef"
    #pragma clang diagnostic ignored "-Wunused-but-set-variable"
    #pragma clang diagnostic ignored "-Wunused-lambda-capture"
    #pragma clang diagnostic ignored "-Wunused-local-typedef"
    #pragma clang diagnostic ignored "-Wunused-parameter"
    #pragma clang diagnostic ignored "-Wunused-value"
    #pragma clang diagnostic ignored "-Wunused-variable"
    #pragma clang diagnostic ignored "-Wweak-vtables"
    #pragma clang diagnostic ignored "-Wunused-private-field"
    #pragma clang diagnostic ignored "-Wmissing-field-initializers"
    #pragma clang diagnostic ignored "-Wconversion"
    #pragma clang diagnostic ignored "-Wunused-function"
    #pragma clang diagnostic ignored "-Wbitwise-instead-of-logical"
#elif defined(__GNUC__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wcast-function-type"
    #pragma GCC diagnostic ignored "-Wclass-memaccess"
    #pragma GCC diagnostic ignored "-Wconversion"
    #pragma GCC diagnostic ignored "-Wctor-dtor-privacy"
    #pragma GCC diagnostic ignored "-Wdeprecated"
    #pragma GCC diagnostic ignored "-Wdeprecated-copy"
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    #pragma GCC diagnostic ignored "-Wextra-semi"
    #pragma GCC diagnostic ignored "-Wignored-qualifiers"
    #pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
    #pragma GCC diagnostic ignored "-Wmisleading-indentation"
    #pragma GCC diagnostic ignored "-Wmissing-field-initializers"
    #pragma GCC diagnostic ignored "-Wmissing-noreturn"
    #pragma GCC diagnostic ignored "-Wold-style-cast"
    #pragma GCC diagnostic ignored "-Wredundant-decls"
    #pragma GCC diagnostic ignored "-Wshadow"
    #pragma GCC diagnostic ignored "-Wsign-compare"
    #pragma GCC diagnostic ignored "-Wsign-conversion"
    #pragma GCC diagnostic ignored "-Wstrict-aliasing"
    #pragma GCC diagnostic ignored "-Wstringop-overflow"
    #pragma GCC diagnostic ignored "-Wswitch-default"
    #pragma GCC diagnostic ignored "-Wundef"
    #pragma GCC diagnostic ignored "-Wuninitialized"
    #pragma GCC diagnostic ignored "-Wunused-but-set-variable"
    #pragma GCC diagnostic ignored "-Wunused-function"
    #pragma GCC diagnostic ignored "-Wunused-local-typedefs"
    #pragma GCC diagnostic ignored "-Wunused-parameter"
    #pragma GCC diagnostic ignored "-Wunused-result"
    #pragma GCC diagnostic ignored "-Wunused-variable"
#elif defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable : 26439) // This kind of function may not throw. Declare it 'noexcept'
    #pragma warning(disable : 26451) // Arithmetic overflow.
    #pragma warning(disable : 26495) // Variable '%variable%' is uninitialized.
    #pragma warning(disable : 26812) // Prefer 'enum class' over 'enum'.
    #pragma warning(disable : 4005) // macro redefintion.
    #pragma warning(disable : 4018) // sign/unsign mismatch in comparison.
    #pragma warning(disable : 4101) // unreferenced local variable.
    #pragma warning(disable : 4244) // int conversion, possible loss of data.
    #pragma warning(disable : 4251) // 'type' : class 'type1' needs to have dll-interface to be used
                                    // by clients of class 'type2'
    #pragma warning(disable : 4267) // size_t conversion, possible loss of data.
    #pragma warning(disable : 4305) // conversion to smaller type, possible loss of data.
    #pragma warning(disable : 4275) // non dll-interface class 'std::exception' used as base for dll-interface class 'openvdb::v10_0::Exception' FIXME
    #pragma warning(disable : 4477) // mismatch in printf argument types.
    #pragma warning(disable : 4828) // File contain illegal character.
    #pragma warning(disable : 4996) // Using deprecated methods.
#endif
