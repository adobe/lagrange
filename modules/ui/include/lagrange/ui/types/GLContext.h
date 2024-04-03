/*
 * Copyright 2019 Adobe. All rights reserved.
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

#include <lagrange/ui/api.h>

// GLEW must come before GLFW
#if defined(__EMSCRIPTEN__)
#include <webgl/webgl2.h>
#else
#include <GL/gl3w.h>
#endif
// Do not change the order
#include <GLFW/glfw3.h>

#include <functional>
#include <memory>
#include <stack>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>

#ifdef _DEBUG
#include <lagrange/utils/assert.h>
#include <cassert>
#endif


namespace lagrange {
namespace ui {

/*
    OpenGL Validation Layer
*/

#ifdef _DEBUG

bool checkGLError(const char* label);
#define LA_GL(x)                          \
    do {                                  \
        x;                                \
        checkGLError(LA_ASSERT_FUNCTION); \
    } while (0)
#else

#define LA_GL(x) \
    do {         \
        x;       \
    } while (0)
#endif

struct LA_UI_API GLState
{
    GLState();

    static void push();
    static void pop();

    template <class F, class... Args>
    void operator()(F func, Args... args)
    {
        void* func_void_ptr = reinterpret_cast<void*>(func);
        void* enable_ptr = reinterpret_cast<void*>(glEnable);
        void* disable_ptr = reinterpret_cast<void*>(glDisable);

        if (func_void_ptr == enable_ptr) {
            set_toggle(true, args...);
        } else if (func_void_ptr == disable_ptr) {
            set_toggle(false, args...);
        } else {
            auto it = values.find(func_void_ptr);
            if (it != values.end()) {
#ifdef _DEBUG
                assert(values[func_void_ptr].size() > 0);
#endif

                values[func_void_ptr].top() = [=]() { LA_GL(func(args...)); };
            }
        }
        // Run
        LA_GL(func(args...));
    }

    static GLState& get();

    static int major_version;
    static int minor_version;
    static std::string get_glsl_version_string();

    static bool on_opengl_thread();

    static std::string_view get_enum_string(GLenum value);

private:
    // Discard
    template <class T, class... Args>
    void set_toggle(bool val, T discard_val, [[maybe_unused]] Args... args)
    {
        (void)(val);
        (void)(discard_val);
    }

    void set_toggle(bool val) { (void)(val); }

    // Discard extra parameters
    template <class... Args>
    void set_toggle(bool val, GLenum name, [[maybe_unused]] Args... args)
    {
        set_toggle(val, name);
    }
    template <class... Args>
    void set_toggle(bool val, int name, [[maybe_unused]] Args... args)
    {
        set_toggle(val, GLenum(name));
    }

    void set_toggle(bool val, GLenum name)
    {
        auto& stack = toggles[name];
        while (stack.size() < m_stack_level) stack.push(glIsEnabled(name));
        stack.top() = val;
    }

    template <class F>
    std::stack<std::function<void(void)>>& get_stack(F func)
    {
        const void* func_void_ptr = reinterpret_cast<const void*>(func);
        return values[func_void_ptr];
    }


    std::unordered_map<const void*, std::stack<std::function<void(void)>>> values;
    std::unordered_map<GLenum, std::stack<bool>> toggles;


    static std::unique_ptr<GLState> m_instance;
    size_t m_stack_level;
    std::thread::id m_gl_thread_id;
};

struct LA_UI_API GLScope
{
    GLScope(bool push = true);
    ~GLScope();


    static GLScope current() { return GLScope(false); }

    template <class F, class... Args>
    void operator()(F func, Args... args)
    {
        GLState::get()(func, args...);
    }

private:
    bool m_push;
};

} // namespace ui
} // namespace lagrange
