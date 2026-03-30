/*
 * Copyright 2026 Adobe. All rights reserved.
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

#include <cista/containers/string.h>
#include <cista/containers/vector.h>

#include <cstdint>

namespace lagrange::serialization::internal {

namespace data = cista::offset;

/// Type tag matching scene::Value variant indices.
enum class CistaValueType : uint8_t {
    Bool = 0,
    Int = 1,
    Double = 2,
    String = 3,
    Buffer = 4,
    Array = 5,
    Object = 6
};

/// Cista-compatible representation of scene::Value.
/// Only one field is populated based on `type`.
struct CistaValue
{
    CistaValueType type = CistaValueType::Bool;

    bool bool_val = false;
    int32_t int_val = 0;
    double double_val = 0.0;
    data::string string_val;
    data::vector<uint8_t> buffer_val;
    data::vector<CistaValue> array_val;

    // Object stored as parallel key/value vectors (cista has no map).
    data::vector<data::string> object_keys;
    data::vector<CistaValue> object_values;
};

/// Cista-compatible representation of scene::Extensions.
/// Only the `data` map is serialized; `user_data` (std::any) is skipped.
struct CistaExtensions
{
    data::vector<data::string> keys;
    data::vector<CistaValue> values;
};

} // namespace lagrange::serialization::internal
