/* SPDX-License-Identifier: Apache-2.0 */

/*
 * Copyright 2021 XIA LLC, All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/** @file log.hpp
 * @brief Defines logging infrastructure components.
 */
 
#ifndef PIXIE_LOG_H
#define PIXIE_LOG_H

#include <atomic>
#include <list>
#include <memory>
#include <sstream>

#include <pixie/os_compat.hpp>

namespace xia
{
namespace logging
{
/*
 * An outputter outputs a log stream. Destruct them last.
 */
struct outputter;
typedef std::list<outputter> outputters;
typedef std::shared_ptr<outputters> outputters_ptr;
PIXIE_EXPORT outputters_ptr PIXIE_API make_outputters();
static outputters_ptr outputs_ptr = make_outputters();
};

/**
 * Log class.
 *
 * This class lives in the pixie namespace to make the references in the code
 * simpler.
 */
class log
{
    friend logging::outputter;
public:
    /*
     * A log level.
     *
     * The levels are ordered from high priority to lower priority with `off`
     * always being first and 0.
     */
    enum level {
        off = 0,
        error,
        warning,
        info,
        debug,
        max_level
    };

    log(level level__)
        : level_(level__) {
    }
    ~log();

    template <typename T>
    std::ostringstream& operator<<(T item) {
        output << item;
        return output;
    }

    log::level get_level() const{
        return level_.load();
    }

private:
    std::atomic<level> level_;
    std::ostringstream output;
};

namespace logging
{
 /*
  * Start and stop a log output stream.
  */
void start(const std::string name,
           const std::string file,
           log::level level = log::warning,
           bool append = true);
void stop(const std::string name);

/*
 * Output control.
 */
void set_level(const std::string name, log::level level);
void set_level_stamp(const std::string name, bool level);
void set_datetime_stamp(const std::string name, bool datetime);
void set_line_numbers(const std::string name, bool line_numbers);

/*
 * Level active
 */
bool level_logging(log::level level);

/**
 * Hex display memory.
 *
 * @param addr The address of the memory to display.
 * @param length The number of elements to display.
 * @param size The size of the data element.
 * @param line_length Number of elements per line.
 * @param offset The printed offset.
 *
 * From https://git.rtems.org/rtems-tools/tree/rtemstoolkit/rtems-utils.cpp#n39
 */
void memdump(log::level level,
             const std::string label,
             const void* addr,
             size_t length,
             size_t size = 1,
             size_t line_length = 16,
             size_t offset = 0);
}
}

#endif  // PIXIE_LOG_H
