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
#include <lagrange/testing/common.h>
#include <catch2/catch_session.hpp>

#ifdef LAGRANGE_UI_OPENGL_TESTS
    #include <lagrange/ui/types/GLContext.h>

struct MiniGLContext
{
    MiniGLContext()
    {
        glfwInit();
        auto window = glfwCreateWindow(1, 1, "Test", NULL, NULL);
        glfwMakeContextCurrent(window);
        gl3wInit();
    }
    ~MiniGLContext() { glfwTerminate(); }
};
#endif

int main(int argc, char* argv[])
{
#ifdef LAGRANGE_UI_OPENGL_TESTS
    MiniGLContext opengl_context;
#endif

    int result = Catch::Session().run(argc, argv);
    return result;
}
