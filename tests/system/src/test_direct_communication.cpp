/*----------------------------------------------------------------------
* Copyright (c) 2005 - 2021, XIA LLC
* All rights reserved.
*
* Redistribution and use in source and binary forms,
* with or without modification, are permitted provided
* that the following conditions are met:
*
*   * Redistributions of source code must retain the above
*     copyright notice, this list of conditions and the
*     following disclaimer.
*   * Redistributions in binary form must reproduce the
*     above copyright notice, this list of conditions and the
*     following disclaimer in the documentation and/or other
*     materials provided with the distribution.
*   * Neither the name of XIA LLC nor the names of its
*     contributors may be used to endorse or promote
*     products derived from this software without
*     specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
* CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
* INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
* MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
* IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
* PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
* ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
* TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
* THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
* SUCH DAMAGE.
*----------------------------------------------------------------------*/
/// @file test_direct_communication.cpp
/// @brief Used to test direct communication with memory registers on the system.
/// @author S. V. Paulauskas
/// @date February 19, 2021
#include "args.hxx"
#include "pixie16app_export.h"
#include "pixie16sys_export.h"
#include "configuration.hpp"

#include <algorithm>
#include <array>
#include <exception>
#include <iostream>
#include <string>
#include <type_traits>

#include <cstdio>

#ifdef _WINDOWS
#include <windows.h>
#else
#include <unistd.h>
#endif

using namespace std;

enum class DATA_IO {
    WRITE = 0,
    READ = 1
};

enum class DATA_PATTERN {
    HI_LO,
    FLIP_FLOP,
    RAMP_UP,
    RAMP_DOWN,
    CONSTANT,
    EVEN_BITS,
    ODD_BITS,
    ZERO
};

DATA_PATTERN convert_string_to_data_pattern(const std::string& input) {
    if (input == "HI_LO")
        return DATA_PATTERN::HI_LO;
    if (input == "FLIP_FLOP")
        return DATA_PATTERN::FLIP_FLOP;
    if (input == "RAMP_UP")
        return DATA_PATTERN::RAMP_UP;
    if (input == "RAMP_DOWN")
        return DATA_PATTERN::RAMP_DOWN;
    if (input == "CONSTANT")
        return DATA_PATTERN::CONSTANT;
    if (input == "EVEN_BITS")
        return DATA_PATTERN::EVEN_BITS;
    if (input == "ODD_BITS")
        return DATA_PATTERN::ODD_BITS;
    return DATA_PATTERN::ZERO;
}

bool verify_power_of_two(const unsigned int& value) {
    if ((value != 0) && ((value & (value - 1)) == 0))
        return true;
    return false;
}

std::vector<unsigned int> prepare_data_to_write(const DATA_PATTERN& data_pattern,
                                                const unsigned int& size) {
    if (!verify_power_of_two(size))
        throw std::invalid_argument("Test data must have a size that's a power of 2!!");

    std::vector<unsigned int> data{};
    for (unsigned int entry = 0; entry < size; entry += 2) {
        switch (data_pattern) {
            case DATA_PATTERN::HI_LO:
                data.emplace_back(0xAAAA5555);
                data.emplace_back(0x5555AAAA);
                break;
            case DATA_PATTERN::FLIP_FLOP:
                data.emplace_back(0xA0500A05);
                data.emplace_back(0x50A0050A);
                break;
            case DATA_PATTERN::RAMP_UP:
                data.emplace_back(entry);
                data.emplace_back(entry + 1);
                break;
            case DATA_PATTERN::RAMP_DOWN:
                data.emplace_back(size - entry);
                data.emplace_back(size - entry - 1);
                break;
            case DATA_PATTERN::CONSTANT:
                data.emplace_back(0x50f750fa);
                data.emplace_back(0x50f750fa);
                break;
            case DATA_PATTERN::EVEN_BITS:
                data.emplace_back(0xA5A5A5A5);
                data.emplace_back(0xA5A5A5A5);
                break;
            case DATA_PATTERN::ODD_BITS:
                data.emplace_back(0x5A5A5A5A);
                data.emplace_back(0x5A5A5A5A);
                break;
            case DATA_PATTERN::ZERO:
                data.emplace_back(0);
                data.emplace_back(0);
                break;
            default:
                break;
        }
    }
    return data;
}

bool verify_api_return_value(const int& val, const std::string& func_name,
                             const std::string& okmsg = "OK") {
    if (val < 0) {
        cerr << endl << "ERROR - " << func_name << " with Error Code " << val << endl;
        return false;
    }
    if (!okmsg.empty())
        cout << okmsg << endl;
    return true;
}

int verify_data_read(const unsigned int* expected, const unsigned int* returned,
                     const int& module_number, const unsigned int& size) {
    unsigned int error_count = 0;
    for (unsigned int idx = 0; idx < size; idx++) {
        if (expected[idx] != returned[idx]) {
            cout << "ERROR - Data mismatch in module " << module_number << ", rd_data="
                 << hex << returned[idx] << ", wr_data=" << expected[idx] << ", position="
                 << dec << idx << endl;
            error_count++;
        }
    }
    return error_count;
}

int main(int argc, char* argv[]) {
    args::ArgumentParser parser("Tests direct communication with the system.");
    parser.helpParams.addDefault = true;
    parser.helpParams.addChoices = true;

    args::Group commands(parser, "commands");
    args::Command dsp(commands, "dsp", "Tests related to the DSP.");
    args::Command external_memory(commands, "external_memory",
                                  "Tests related to the external memory.");

    args::Group arguments(parser, "arguments", args::Group::Validators::AtLeastOne,
                          args::Options::Global);
    args::Positional<std::string> configuration(arguments, "cfg", "The configuration file to load.",
                                                args::Options::Required);
    args::HelpFlag help_flag(arguments, "help", "Displays this message", {'h', "help"});

    args::ValueFlag<std::string> boot_pattern_flag(arguments, "boot_pattern",
                                                   "The boot pattern used for booting.",
                                                   {'b', "boot_pattern"}, "0x7F");
    args::ValueFlag<std::string> address_flag(arguments, "address",
                                              "The memory address in hex.",
                                              {'a', "address"}, "0x0");
    args::ValueFlag<unsigned int> module_number_flag(arguments, "module_number",
                                                     "The module number to act against.",
                                                     {'m', "module"}, 0);
    args::Flag write(arguments, "write", "Flag to perform a write procedure", {'w', "write"});
    args::Flag read(arguments, "read", "Flag to perform a read procedure", {'r', "read"});
    args::Flag verbose(arguments, "verbose", "Flag to control verbosity", {'v', "verbose"});
    args::Flag is_dry_run(arguments, "dry_run", "Flag to control actual execution of procedures.",
                          {"dry_run"});

    args::Group data_arguments(parser, "data_arguments", args::Group::Validators::AtLeastOne,
                               args::Options::Global);
    args::ValueFlag<std::string> data_flag(data_arguments, "data",
                                           "The data that we want to write to the register.",
                                           {'d', "data"}, "0x0");
    args::MapFlag<std::string, DATA_PATTERN> data_pattern_flag(data_arguments, "test_data_pattern",
                                                               "The type of test data to generate as 32-bit words."
                                                               "\nDefault: CONSTANT",
                                                               {'p', "pattern"},
                                                               {{"HI_LO",     DATA_PATTERN::HI_LO},
                                                                {"FLIP_FLOP", DATA_PATTERN::FLIP_FLOP},
                                                                {"RAMP_UP",   DATA_PATTERN::RAMP_UP},
                                                                {"RAMP_DOWN", DATA_PATTERN::RAMP_DOWN},
                                                                {"CONSTANT",  DATA_PATTERN::CONSTANT},
                                                                {"EVEN_BITS", DATA_PATTERN::EVEN_BITS},
                                                                {"ODD_BITS",  DATA_PATTERN::ODD_BITS},
                                                                {"ZERO",      DATA_PATTERN::ZERO}},
                                                               DATA_PATTERN::CONSTANT);
    args::ValueFlag<unsigned int> data_size_flag(data_arguments, "data_size",
                                                 "The number of 32-bit words to put into the buffer.",
                                                 {'s', "data_size"}, 65536);


    try {
        parser.ParseCLI(argc, argv);
    } catch (args::Help& help) {
        cout << parser;
        return EXIT_SUCCESS;
    } catch (args::ValidationError& e) {
        cerr << e.what() << endl;
        cout << parser;
        return EXIT_FAILURE;
    }

    xia::configuration::Configuration cfg;
    try {
        cfg = xia::configuration::read_configuration_file(configuration.Get());
    } catch (invalid_argument& invalidArgument) {
        cerr << invalidArgument.what() << endl;
        return EXIT_FAILURE;
    }

    static const int offline_mode = 0;
    cout << "INFO - Calling Pixie16InitSystem.......";
    if (!verify_api_return_value(Pixie16InitSystem(cfg.numModules, cfg.slot_map, offline_mode),
                                 "Pixie16InitSystem"))
        return EXIT_FAILURE;

    unsigned int boot_pattern = stoul(args::get(boot_pattern_flag), nullptr, 0);
    cout << "INFO - Calling Pixie16BootModule with boot pattern: " << showbase << hex
         << boot_pattern << dec
         << "............" << endl;

    if (!is_dry_run) {
        if (!verify_api_return_value(
                Pixie16BootModule(cfg.ComFPGAConfigFile.c_str(), cfg.SPFPGAConfigFile.c_str(),
                                  cfg.TrigFPGAConfigFile.c_str(), cfg.DSPCodeFile.c_str(),
                                  cfg.DSPParFile.c_str(), cfg.DSPVarFile.c_str(), cfg.numModules,
                                  boot_pattern),
                "Pixie16BootModule", "INFO - Finished booting!"))
            return EXIT_FAILURE;
    }

    unsigned int address = stoul(args::get(address_flag), nullptr, 0);

    DATA_IO data_io;

    cout << "INFO - Performing a test with the ";
    if (dsp) {
        cout << "DSP" << endl;
        auto data = prepare_data_to_write(args::get(data_pattern_flag), args::get(data_size_flag));

        if (write) {
            data_io = DATA_IO::WRITE;
            cout << "INFO - Performing a write to memory address " << args::get(address_flag)
                 << " with a size of " << args::get(data_size_flag) << " on Module "
                 << args::get(module_number_flag) << endl;
            if (!is_dry_run)
                Pixie_DSP_Memory_IO(data.data(), address, args::get(data_size_flag) - 1,
                                    static_cast<std::underlying_type<DATA_IO>::type>(data_io),
                                    args::get(module_number_flag));
        }

        if (read) {
            data_io = DATA_IO::READ;
            vector<unsigned int> read_data(args::get(data_size_flag), 0);
            cout << "INFO - Performing a read from memory address " << args::get(address_flag)
                 << " with a size of " << args::get(data_size_flag) << " on Module "
                 << args::get(module_number_flag) << endl;
            if (!is_dry_run) {
                Pixie_DSP_Memory_IO(data.data(), address, args::get(data_size_flag) - 1,
                                    static_cast<std::underlying_type<DATA_IO>::type>(data_io),
                                    args::get(module_number_flag));

                auto error_count = verify_data_read(data.data(), read_data.data(),
                                                    args::get(module_number_flag),
                                                    args::get(data_size_flag));
                if (error_count == 0)
                    cout << "INFO - Data read was the same as data written!" << endl;
                if (verbose) {
                    cout << "INFO - Outputting read data to terminal:";
                    cout << hex;
                    for (const auto& it: read_data) {
                        cout << it;
                        cout << dec;
                    }
                }
            }
        }
    }
    if (external_memory) {
        cout << "External Memory" << endl;
    }
}
