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

namespace nb = nanobind;

namespace lagrange::python {

namespace {

class PythonLoggingSink : public spdlog::sinks::base_sink<std::mutex>
{
public:
    PythonLoggingSink(std::string logger_name)
    {
        nb::module_ logging = nb::module_::import_("logging");
        m_py_logger = logging.attr("getLogger")(logger_name);
    }

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override
    {
        // Logging in python requires the current thread to hold the GIL.
        if (!PyGILState_Check()) return;

        auto payload = msg.payload;
        auto res = nb::str(payload.data(), payload.size());

        switch (msg.level) {
        case spdlog::level::trace: m_py_logger.attr("debug")(res); break;
        case spdlog::level::debug: m_py_logger.attr("debug")(res); break;
        case spdlog::level::info: m_py_logger.attr("info")(res); break;
        case spdlog::level::warn: m_py_logger.attr("warning")(res); break;
        case spdlog::level::err: m_py_logger.attr("error")(res); break;
        case spdlog::level::critical: m_py_logger.attr("critical")(res); break;
        case spdlog::level::off: break;
        default: break;
        }
    }

    void flush_() override
    {
        // Logging in python requires the current thread to hold the GIL.
        if (!PyGILState_Check()) return;

        auto handlers = m_py_logger.attr("handlers");
        for (auto handler : handlers) {
            handler.attr("flush")();
        }
    }

private:
    nanobind::object m_py_logger;
};

} // namespace

void register_python_logger(nanobind::module_& m)
{
    auto& logger = lagrange::logger();
    std::string logger_name = nb::cast<std::string>(m.attr("__name__"));
    logger.sinks().clear();
    logger.sinks().emplace_back(std::make_shared<PythonLoggingSink>(std::move(logger_name)));
    logger.set_level(spdlog::level::trace); // Log level will be controlled by Python.
}

} // namespace lagrange::python
