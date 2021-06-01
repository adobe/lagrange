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
#include <lagrange/Logger.h>
#include <lagrange/ui/types/GLContext.h>
#include <cassert>

namespace lagrange {
namespace ui {

std::unique_ptr<GLState> GLState::m_instance = nullptr;
int GLState::major_version = 4;
int GLState::minor_version = 1;

std::string GLState::get_glsl_version_string()
{
    std::string res = "#version ";

    std::string v = "";

    if (major_version == 2) {
        if (minor_version == 0) {
            v = "110";
        } else if (minor_version == 1) {
            v = "120";
        }
    } else if (major_version == 3) {
        if (minor_version == 0) {
            v = "130";
        } else if (minor_version == 1) {
            v = "140";
        } else if (minor_version == 2) {
            v = "150";
        } else if (minor_version == 3) {
            v = "330";
        }
    } else if (major_version == 4) {
        if (minor_version == 0) {
            v = "400";
        } else if (minor_version == 1) {
            v = "410";
        } else if (minor_version == 2) {
            v = "420";
        } else if (minor_version == 3) {
            v = "430";
        } else if (minor_version == 4) {
            v = "440";
        }
    }

    return res + v;
}

bool GLState::on_opengl_thread()
{
    return GLState::get().m_gl_thread_id == std::this_thread::get_id();
}

GLState& GLState::get()
{
    if (m_instance == nullptr) m_instance = std::make_unique<GLState>();
    return *m_instance;
}


GLState::GLState()
    : m_stack_level(1)
    , m_gl_thread_id(std::this_thread::get_id())
{
    get_stack(glDepthMask).push([]() { glDepthMask(GL_TRUE); });
    get_stack(glDepthFunc).push([]() { glDepthFunc(GL_LESS); });

    get_stack(glBlendFunc).push([]() { glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); });
    get_stack(glBlendEquation).push([]() { glBlendEquation(GL_FUNC_ADD); });
    get_stack(glClearColor).push([]() { glClearColor(0, 0, 0, 0); });
    get_stack(glViewport).push([]() { /*No default*/ });
    get_stack(glCullFace).push([]() { glCullFace(GL_BACK); });
    get_stack(glFrontFace).push([]() { glFrontFace(GL_CCW); });
    get_stack(glPolygonMode).push([]() { glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); });
    get_stack(glColorMask).push([]() { glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE); });

    set_toggle(true, GL_DEPTH_TEST);
    set_toggle(true, GL_BLEND);
    set_toggle(true, GL_CULL_FACE);
}

void GLState::pop()
{
    auto& state = get();
    if (state.m_stack_level == 0) {
        assert(false);
    }

    for (auto& it : state.values) {
        auto& stack = it.second;
        // Copy non-empty stack top value
        if (stack.size() > 1) {
            stack.pop();
            auto& fn = stack.top();
            fn();
        } else if (it.second.size() == 1) {
            auto& fn = stack.top();
            fn();
        }
    }

    for (auto& it : state.toggles) {
        auto& stack = it.second;
        if (stack.size() > 1) {
            stack.pop();
            bool value = stack.top();
            if (value)
                glEnable(it.first);
            else
                glDisable(it.first);
        } else if (stack.size() == 1) {
            bool value = stack.top();
            if (value)
                glEnable(it.first);
            else
                glDisable(it.first);
        }
    }

    state.m_stack_level--;
}

void GLState::push()
{
    auto& state = get();
    for (auto& it : state.values) {
        // Copy non-empty stack top value
        if (it.second.size() > 0) it.second.push(it.second.top());
    }

    for (auto& it : state.toggles) {
        // Copy non-empty stack top value
        if (it.second.size() > 0) it.second.push(it.second.top());
    }
    state.m_stack_level++;
}

GLScope::GLScope(bool push)
    : m_push(push)
{
    if (m_push) GLState::push();
}

GLScope::~GLScope()
{
    if (m_push) GLState::pop();
}

bool checkGLError(const char* label)
{
    bool hasErr = false;
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
        switch (err) {
        case GL_INVALID_ENUM: logger().error("GL_INVALID_ENUM: {}", label); break;
        case GL_INVALID_VALUE: logger().error("GL_INVALID_VALUE: {}", label); break;
        case GL_INVALID_OPERATION: logger().error("GL_INVALID_OPERATION: {}", label); break;
        case GL_STACK_OVERFLOW: logger().error("GL_STACK_OVERFLOW: {}", label); break;
        case GL_STACK_UNDERFLOW: logger().error("GL_STACK_UNDERFLOW: {}", label); break;
        case GL_OUT_OF_MEMORY: logger().error("GL_OUT_OF_MEMORY: {}", label); break;
        case GL_INVALID_FRAMEBUFFER_OPERATION:
            logger().error("GL_INVALID_FRAMEBUFFER_OPERATION: {}", label);
            break;
        default: logger().error("Unknown Error: {}", label); break;
        }
        hasErr = true;
        break;
    }

    return hasErr;
}

} // namespace ui
} // namespace lagrange
