/*
 * Copyright 2021 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */

#include <lagrange/ui/types/Systems.h>


namespace lagrange {
namespace ui {

void Systems::run(Stage stage, Registry& registry)
{
    for (auto& item : m_items) {
        if (item.stage == stage && item.enabled) item.system(registry);
    }
}

StringID Systems::add(Stage stage, const System& system, StringID id /*= 0*/)
{
    if (id != 0) {
        if (std::find_if(m_items.begin(), m_items.end(), [&](const SystemItem& item) {
                return item.id == id;
            }) != m_items.end())
            return 0;
    }

    const auto id_final = (id == 0) ? new_id() : id;

    SystemItem item;
    item.system = system;
    item.stage = stage;
    item.id = id_final;

    m_items.push_back(std::move(item));

    std::stable_sort(m_items.begin(), m_items.end(), [](const SystemItem& a, const SystemItem& b) {
        return a.stage < b.stage;
    });

    return id_final;
}

bool Systems::enable(StringID id, bool value)
{
    auto it = std::find_if(m_items.begin(), m_items.end(), [&](const SystemItem& item) {
        return item.id == id;
    });

    if (it == m_items.end()) {
        return false;
    }

    it->enabled = value;

    return true;
}

bool Systems::succeeds(StringID system, StringID after)
{
    assert(system != after);

    auto it_sys = std::find_if(m_items.begin(), m_items.end(), [&](const SystemItem& item) {
        return item.id == system;
    });
    auto it_after = std::find_if(m_items.begin(), m_items.end(), [&](const SystemItem& item) {
        return item.id == after;
    });

    if (it_sys == m_items.end() || it_after == m_items.end()) return false;

    auto i0 = std::distance(m_items.begin(), it_sys);
    auto i1 = std::distance(m_items.begin(), it_after);

    if (i0 > i1)
        std::rotate(m_items.rend() - i0 - 1, m_items.rend() - i0, m_items.rend() - i1);
    else
        std::rotate(m_items.begin() + i0, m_items.begin() + i0 + 1, m_items.begin() + i1 + 1);

    return true;
}

bool Systems::remove(StringID id)
{
    auto it = std::find_if(m_items.begin(), m_items.end(), [&](const SystemItem& item) {
        return item.id == id;
    });

    if (it == m_items.end()) return false;

    m_items.erase(it);

    return true;
}

StringID Systems::new_id()
{
    return m_id_counter++;
}

} // namespace ui
} // namespace lagrange
