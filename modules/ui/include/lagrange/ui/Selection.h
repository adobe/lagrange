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

#include <lagrange/utils/la_assert.h>
#include <lagrange/ui/Callbacks.h>

#include <unordered_set>

namespace lagrange {
namespace ui {


enum class SelectionElementType { OBJECT, FACE, EDGE, VERTEX };
enum class SelectionBehavior {SET, ADD, ERASE};

template <typename T>
class Selection : public CallbacksBase<Selection<T>> {
public:
    using OnChange = UI_CALLBACK(void(Selection<T>&));

    // ==== Queries ====
    const std::unordered_set<T>& get_selection() const { return m_set; }

    template <typename K>
    const std::vector<K*> get_filtered() const
    {
        std::vector<K*> result;
        for (auto& t : m_set) {
            auto k = dynamic_cast<K*>(t);
            result.push_back(k);
        }
        return result;
    }

    bool has(const T& val) const { return m_set.find(val) != m_set.end(); }

    template <class Container>
    bool has_multiple(const Container& container) const
    {
        for (auto& val : container) {
            if (!has(val)) return false;
        }
        return true;
    }

    size_t size() const { return m_set.size(); }

    // ==== Setters ====
    // They return true if something changed, otherwise false

    bool clear(bool trigger_callbacks = true)
    {
        if (m_set.empty()) return false;

        m_set.clear();
        if (trigger_callbacks) trigger_change();
        return true;
    }

    bool set(const T& value, bool trigger_callbacks = true)
    {
        if (m_set.size() == 1 && (*m_set.begin()) == value) return false;

        m_set.clear();
        m_set.insert(value);
        if (trigger_callbacks) trigger_change();
        return true;
    }

    template <class Container>
    bool set_multiple(const Container& container, bool trigger_callbacks = true)
    {
        if (m_set.size() == container.size() && has_multiple(container)) {
            return false;
        }

        m_set.clear();
        for (auto val : container) m_set.insert(val);
        
        if (trigger_callbacks) trigger_change();
        return true;
    }

    bool add(const T& value, bool trigger_callbacks = true)
    {
        bool changed = (m_set.insert(value)).second;
        if (changed && trigger_callbacks) trigger_change();
        return changed;
    }

    template <class Container>
    bool add_multiple(const Container& container, bool trigger_callbacks = true)
    {
        bool changed = false;
        for (auto& val : container) {
            changed |= add(val, false);
        }
        if (changed && trigger_callbacks) trigger_change();
        return changed;
    }

    bool erase(const T& val, bool trigger_callbacks = true)
    {
        bool changed = m_set.erase(val) > 0;
        if (changed && trigger_callbacks) trigger_change();
        return changed;
    }

    template <class Container>
    bool erase_multiple(const Container& container, bool trigger_callbacks = true)
    {
        bool changed = false;
        for (auto& val : container) {
            changed |= erase(val, false);
        }
        if (changed && trigger_callbacks) trigger_change();
        return changed;
    }

    bool update(const T& val, bool valid, SelectionBehavior behavior, bool trigger_callbacks = true)
    { 
        if (valid) {
            if (behavior == SelectionBehavior::ADD) {
                return add(val, trigger_callbacks);
            } else if (behavior == SelectionBehavior::ERASE) {
                return erase(val, trigger_callbacks);
            } else {
                return set(val, trigger_callbacks);
            }
        } else if (behavior == SelectionBehavior::SET) {
            return clear(trigger_callbacks);
        } else {
            return false;
        }
    }

    template <class Container>
    bool update_multiple(
        const Container& container, SelectionBehavior behavior, bool trigger_callbacks = true)
    {
        if (behavior == SelectionBehavior::ADD) {
            return add_multiple(container, trigger_callbacks);
        } else if (behavior == SelectionBehavior::ERASE) {
            return erase_multiple(container, trigger_callbacks);
        } else {
            return set_multiple(container, trigger_callbacks);
        }
    }

    Callbacks<OnChange>& get_callbacks() { return m_callbacks; }
    void trigger_change() { m_callbacks.template call<OnChange>(*this); }

private:
    std::unordered_set<T> m_set;

protected:
    Callbacks<OnChange> m_callbacks;

    friend CallbacksBase<Selection<T>>;
};

template <typename T>
class TwoStateSelection {
public:
    const Selection<T>& get_persistent() const { return m_persistent; }
    Selection<T>& get_persistent() { return m_persistent; }

    const Selection<T>& get_transient() const { return m_transient; }
    Selection<T>& get_transient() { return m_transient; }

private:
    Selection<T> m_persistent;
    Selection<T> m_transient;
};

class ElementSelection : public TwoStateSelection<unsigned int> {
public:
    ElementSelection(SelectionElementType type = SelectionElementType::OBJECT)
    : m_type(type)
    {
    }

    SelectionElementType get_type() const { return m_type; }

    void set_type(SelectionElementType type) { 
        if (m_type == type) return;
        m_type = type; 

        if (!get_persistent().clear())
            get_persistent().trigger_change();

        if (!get_transient().clear()) 
            get_transient().trigger_change();
    }

private:
    SelectionElementType m_type;
};

} // namespace ui
} // namespace lagrange
