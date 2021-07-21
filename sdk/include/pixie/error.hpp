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

/** @file error.hpp
 * @brief Defines custom error codes used throughout the SDK.
 */

#ifndef PIXIE_ERROR_H
#define PIXIE_ERROR_H

#include <iostream>
#include <stdexcept>
#include <sstream>

namespace xia
{
namespace pixie
{
namespace error
{
/*
 * Codes
 */
enum struct code {
    /*
     * Success is always 0. Do not number any other enum entries.
     */
    success = 0,
    /*
     * Crate
     */
    crate_already_open,
    crate_not_ready,
    crate_invalid_param,
    /*
     * Module
     */
    module_number_invalid,
    module_total_invalid,
    module_already_open,
    module_close_failure,
    module_offline,
    module_info_failure,
    module_invalid_operation,
    module_invalid_firmware,
    module_initialize_failure,
    module_invalid_param,
    module_invalid_var,
    module_param_disabled,
    module_param_readonly,
    module_param_writeonly,
    module_task_timeout,
    module_invalid_slot,
    module_not_found,
    module_test_invalid,
    /*
     * Channel
     */
    channel_number_invalid,
    channel_invalid_param,
    channel_invalid_var,
    channel_invalid_index,
    channel_param_disabled,
    channel_param_readonly,
    channel_param_writeonly,
    /*
     * Device
     */
    device_load_failure,
    device_boot_failure,
    device_initialize_failure,
    device_copy_failure,
    device_image_failure,
    device_hw_failure,
    device_dma_failure,
    device_dma_busy,
    device_fifo_failure,
    device_eeprom_failure,
    device_eeprom_bad_type,
    device_eeprom_not_found,
    /*
     * Configuration
     */
    config_invalid_param,
    config_param_not_found,
    config_json_error,
    /*
     * File handling
     */
    file_not_found,
    file_open_failure,
    file_read_failure,
    file_size_invalid,
    file_create_failure,
    /*
     * System
     */
    no_memory,
    slot_map_invalid,
    invalid_value,
    not_supported,
    buffer_pool_empty,
    buffer_pool_not_empty,
    buffer_pool_busy,
    buffer_pool_not_enough,
    /*
     * Catch all
     */
    unknown_error,
    internal_failure,
    bad_allocation,
    bad_error_code,
    last
};

/*
 * All exceptions need to be based on this error.
 */
struct error
    : public std::runtime_error {
    typedef pixie::error::code code;
    const code type;

    explicit error(const code type, const std::ostringstream& what);
    explicit error(const code type, const std::string& what);
    explicit error(const code type, const char* what);
    virtual ~error() = default;

    virtual void output(std::ostream& out);

    int result() const;
    std::string result_text() const;

    int return_code() const;
};

/*
 * Internal test: check that error code definition are consistent.
 */
bool check_code_match();

/*
 * Get the API code for an error code.
 */
int api_result(enum code type);
std::string api_result_text(enum code type);

/*
 * Turn the API result into a result code.
 */
int return_code(int result);

/*
 * Bad allocatino error helper.
 */
int api_result_bad_alloc_error();

/*
 * Unknown error helper.
 */
int api_result_unknown_error();

/*
 * Not support helper.
 */
int api_result_not_supported();
}
}
}

std::ostringstream& operator<<(std::ostringstream& out, xia::pixie::error::error& error);
std::ostream& operator<<(std::ostream& out, xia::pixie::error::error& error);

#endif  // PIXIE_ERROR_H
