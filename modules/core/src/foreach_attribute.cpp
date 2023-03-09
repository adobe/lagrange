/*
 * Copyright 2023 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */

#include <lagrange/foreach_attribute.h>

#include <tbb/parallel_for.h>

namespace lagrange::details {

void par_foreach_attribute_id(span<const AttributeId> ids, function_ref<void(AttributeId)> cb)
{
    tbb::parallel_for(size_t(0), ids.size(), [&](size_t i) { cb(ids[i]); });
}

} // namespace lagrange::details
