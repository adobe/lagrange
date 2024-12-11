/*
 * Copyright 2022 Adobe. All rights reserved.
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

#include <lagrange/api.h>

#include <stdexcept>

namespace lagrange {

/// @addtogroup group-utils-assert
/// @{

///
/// Base exception for errors thrown by Lagrange functions.
///
struct LA_CORE_API Error : public std::runtime_error
{
    using runtime_error::runtime_error;
    ~Error() override;
};

///
/// An exception of this type is thrown when a lagrange::safe_cast<> fails.
///
struct LA_CORE_API BadCastError : public Error
{
    BadCastError() : Error("bad cast") {}
    ~BadCastError() override;
};

///
/// An exception of this type is thrown when a parsing error occurs.
///
struct LA_CORE_API ParsingError : public Error
{
    using Error::Error;
    ~ParsingError() override;
};

/// @}

} // namespace lagrange
