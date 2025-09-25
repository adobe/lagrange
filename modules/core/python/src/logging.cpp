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

#include "logging.h"

#include <lagrange/Logger.h>
#include <lagrange/python/binding.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <spdlog/sinks/base_sink.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <mutex>

namespace lagrange::python {

class PythonLoggingSink : public spdlog::sinks::base_sink<std::mutex>
{
protected:
    void sink_it_(const spdlog::details::log_msg& msg) override
    {
        // Logging in python requires the current thread to hold the GIL.
        if (!PyGILState_Check()) return;

        namespace nb = nanobind;
        auto payload = msg.payload;
        auto res = nb::str(payload.data(), payload.size());
        nb::module_ logging = nb::module_::import_("logging");
        auto py_logger = logging.attr("getLogger")("lagrange");

        switch (msg.level) {
        case spdlog::level::trace: py_logger.attr("debug")(res); break;
        case spdlog::level::debug: py_logger.attr("debug")(res); break;
        case spdlog::level::info: py_logger.attr("info")(res); break;
        case spdlog::level::warn: py_logger.attr("warning")(res); break;
        case spdlog::level::err: py_logger.attr("error")(res); break;
        case spdlog::level::critical: py_logger.attr("critical")(res); break;
        case spdlog::level::off: break;
        default: break;
        }
    }

    void flush_() override
    {
        // Logging in python requires the current thread to hold the GIL.
        if (!PyGILState_Check()) return;

        namespace nb = nanobind;
        nb::module_ logging = nb::module_::import_("logging");
        auto py_logger = logging.attr("getLogger")("lagrange");
        auto handlers = py_logger.attr("handlers");
        for (auto handler : handlers) {
            handler.attr("flush")();
        }
    }
};

void register_python_logger()
{
    auto& logger = lagrange::logger();
    logger.sinks().clear();
    logger.sinks().emplace_back(std::make_shared<PythonLoggingSink>());
    logger.set_level(spdlog::level::trace); // Log level will be controlled by Python.
}

} // namespace lagrange::python
