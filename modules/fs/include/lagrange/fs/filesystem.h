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
#pragma once

#include <lagrange/fs/detail/guess_backend.h>

#if defined(LAGRANGE_USE_STD_FS)

#include <filesystem>
namespace lagrange {
namespace fs {
using namespace std::filesystem;
using ifstream = std::ifstream;
using ofstream = std::ofstream;
using fstream = std::fstream;
} // namespace fs
} // namespace lagrange

#elif defined(LAGRANGE_USE_GHC_FS)

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <ghc/fs_fwd.hpp>
#include <lagrange/utils/warnon.h>
// clang-format on

namespace lagrange {
namespace fs {
using namespace ghc::filesystem;
using ifstream = ghc::filesystem::ifstream;
using ofstream = ghc::filesystem::ofstream;
using fstream = ghc::filesystem::fstream;
} // namespace fs
} // namespace lagrange

#elif defined(LAGRANGE_USE_BOOST_FS)

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

namespace lagrange {
namespace fs {
using namespace boost::filesystem;
using boost::filesystem::fstream;
using boost::filesystem::ifstream;
using boost::filesystem::ofstream;
} // namespace fs
} // namespace lagrange

#else

#error Impossible situation occurred. Either LAGRANGE_USE_STD_FS, LAGRANGE_USE_GHC_FS, or LAGRANGE_USE_BOOST_FS should be defined at this point

#endif
