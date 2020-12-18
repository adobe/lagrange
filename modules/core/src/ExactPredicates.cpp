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
#include <lagrange/ExactPredicates.h>

#include <exception>
#include <sstream>

#include <lagrange/ExactPredicatesShewchuk.h>

namespace lagrange {

std::unique_ptr<ExactPredicates> ExactPredicates::create(const std::string& engine)
{
    if (engine == "shewchuk") {
        return std::make_unique<ExactPredicatesShewchuk>();
    } else {
        std::stringstream err_msg;
        err_msg << "Unsupported exact predicate engine: " << engine;
        throw std::runtime_error(err_msg.str());
    }
}

} // namespace lagrange
