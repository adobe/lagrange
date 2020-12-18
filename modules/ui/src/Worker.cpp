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
#include <lagrange/ui/Worker.h>

#include <thread>
#include <queue>

namespace {
    bool should_stop = false;
    std::thread thread;
    std::queue<std::function<void()>> tasks;
}

bool lagrange::ui::Worker::run(std::function<void()> task) {
    if (should_stop) return false;
    if (!thread.joinable()) {
        thread = std::thread([]() {
            while (!should_stop) {
                while (!tasks.empty()) {
                    tasks.front()();
                    tasks.pop();
                }
                
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });
    }
    tasks.push(task);
    return true;
}

void lagrange::ui::Worker::stop() {
    should_stop = true; // do not accept more jobs
    if (thread.joinable()) {
        thread.join();
    }
}
