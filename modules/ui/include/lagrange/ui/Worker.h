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

#include <functional>

/*
Extremely simple worker class that allows to execute slow, blocking tasks
independently of the main thread, to allow freezing the view and gui.
 
If you need multi-threading and more control you probably want
to use different solutions. 
 
Usage: call Worker::Do( [&your_data] () { your_code; } );
 
*/
namespace lagrange {
namespace ui {

class Worker {
public:

    // Enqueue the task. Returns false if the task is not accepted (e.g. if called after Stop() ).
    static bool run(std::function<void()> task);

    // Stop the worker once the current tasks are completed. Warning: blocking.
    static void stop();
};
}

}
