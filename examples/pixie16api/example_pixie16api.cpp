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

/** @file example_pixie16api.cpp
 * @brief Demonstrates how to use the Pixie16Api functions to communicate with Pixie-16 modules.
 *
 * We demonstrate both the PixieSDK implementation using `Pixie16Api.so` as well as the Legacy C
 * implementation using `Pixie16App.so`. The only difference between using one library over the
 * other is in the header definitions.
 *
 * The `Pixie16Api.so` includes all of the goodness that comes with using the C++ implementation
 * just with a convenient C wrapper. Users don't have as much control over the nitty gritty details
 * of the PixieSDK. We've intentionally limited this to prevent this backward compatible API from
 * growing out of hand. We **really** recommend that you link your code directly with `PixieSDK.a`.
 */

#include <chrono>
#include <cmath>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>

#include <args/args.hxx>
#include <easylogging/easylogging++.h>

#ifndef LEGACY_EXAMPLE
#include <pixie16/pixie16.h>
#else
#include <pixie16app_defs.h>
#include <pixie16app_export.h>
#include <pixie16sys_defs.h>
#include <pixie16sys_export.h>
#endif

#if defined(_WIN64) || defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

INITIALIZE_EASYLOGGINGPP

struct configuration {
    int num_modules;
    std::vector<unsigned short> slot_map;
    std::string com_fpga_config;
    std::string sp_fpga_config;
    std::string dsp_code;
    std::string dsp_param;
    std::string dsp_var;
};

std::string generate_filename(const unsigned int& module_number, const std::string& type,
                              const std::string& ext) {
#ifndef LEGACY_EXAMPLE
    static const std::string file_prefix = "pixie16api-module";
#else
    static const std::string file_prefix = "pixie16app-module";
#endif
    return file_prefix + std::to_string(module_number) + "-" + type + "." + ext;
}

void read_config(const std::string& config_file_name, configuration& cfg) {
    std::ifstream input(config_file_name, std::ios::in);
    if (input.fail()) {
        throw std::ios_base::failure("open: " + config_file_name + ": " + std::strerror(errno));
    }

    input >> cfg.num_modules;
    if (cfg.num_modules == 0 || cfg.num_modules > SYS_MAX_NUM_MODULES) {
        throw std::invalid_argument("invalid number of modules");
    }

    cfg.slot_map.clear();
    for (int num = 0; num < cfg.num_modules; num++) {
        int slot;
        if (input >> slot) {
            cfg.slot_map.push_back(slot);
        } else {
            throw std::invalid_argument("invalid slot");
        }
    }

    input >> cfg.com_fpga_config;
    if (!input) {
        throw std::invalid_argument("invalid COM FPGA file name");
    }

    input >> cfg.sp_fpga_config;
    if (!input) {
        throw std::invalid_argument("invalid FP FPGA file name");
    }

    std::string trig_holder;
    input >> trig_holder;
    if (!input) {
        throw std::invalid_argument("invalid Trigg file name");
    }

    input >> cfg.dsp_code;
    if (!input) {
        throw std::invalid_argument("invalid DSP code file name");
    }

    input >> cfg.dsp_param;
    if (!input) {
        throw std::invalid_argument("invalid DSP parameters file name");
    }

    input >> cfg.dsp_var;
    if (!input) {
        throw std::invalid_argument("invalid DSP variables file name");
    }

    input.close();
}

bool verify_api_return_value(const int& val, const std::string& func_name,
                             const bool& print_success = true) {
    if (val < 0) {
        LOG(ERROR) << func_name << " failed with Error Code " << val;
        return false;
    }
    if (print_success)
        LOG(INFO) << func_name << " finished successfully.";
    return true;
}

bool save_dsp_pars(const std::string& filename) {
    LOG(INFO) << "Saving DSP Parameters to " << filename << ".";
    if (!verify_api_return_value(Pixie16SaveDSPParametersToFile(filename.c_str()),
                                 "Pixie16SaveDSPParametersToFile"))
        return false;
    return true;
}

bool execute_adjust_offsets(const unsigned int& numModules, const std::string& setfile) {
    for (unsigned int k = 0; k < numModules; k++) {
        LOG(INFO) << "Adjusting baseline offset for Module " << k << ".";
        if (!verify_api_return_value(Pixie16AdjustOffsets(k),
                                     "Pixie16AdjustOffsets for Module" + std::to_string(k)))
            return false;
    }

    if (!save_dsp_pars(setfile))
        return false;

    return true;
}

bool execute_baseline_capture(const unsigned int& module) {
    LOG(INFO) << "Starting baseline capture.";
    if (!verify_api_return_value(Pixie16AcquireBaselines(module), "Pixie16AcquireBaselines"))
        return false;

    double baselines[NUMBER_OF_CHANNELS][MAX_NUM_BASELINES];
    double timestamps[MAX_NUM_BASELINES];
    for (unsigned int i = 0; i < NUMBER_OF_CHANNELS; i++) {
        LOG(INFO) << "Acquiring baselines for Channel " << i;
        if (!verify_api_return_value(
                Pixie16ReadSglChanBaselines(baselines[i], timestamps, MAX_NUM_BASELINES, module, i),
                "Pixie16ReadsglChanBaselines"))
            return false;
    }

    std::ofstream ofstream1(generate_filename(module, "baselines", "csv"));
    ofstream1 << "bin,timestamp,";
    for (unsigned int i = 0; i < NUMBER_OF_CHANNELS; i++)
        ofstream1 << "Chan" << i << ",";
    ofstream1 << std::endl;
    for (unsigned int i = 0; i < MAX_NUM_BASELINES; i++) {
        ofstream1 << i << "," << timestamps[i] << ",";
        for (unsigned int k = 0; k < NUMBER_OF_CHANNELS; k++) {
            if (k != NUMBER_OF_CHANNELS - 1)
                ofstream1 << baselines[k][i] << ",";
            else
                ofstream1 << baselines[k][i];
        }
        ofstream1 << std::endl;
    }

    return true;
}

bool execute_list_mode_run(const configuration& cfg, const double& runtime_in_seconds) {
    LOG(INFO) << "Starting list mode data run for " << runtime_in_seconds << " s.";

    LOG(INFO) << "Calling "
              << "Pixie16WriteSglModPar"
              << " to write SYNCH_WAIT = 1 in Module 0.";
    if (!verify_api_return_value(Pixie16WriteSglModPar("SYNCH_WAIT", 0, 0),
                                 "Pixie16WriteSglModPar - SYNC_WAIT"))
        return false;

    LOG(INFO) << "Calling "
              << "Pixie16WriteSglModPar"
              << " to write IN_SYNCH  = 0 in Module 0.";
    if (!verify_api_return_value(Pixie16WriteSglModPar("IN_SYNCH", 1, 0),
                                 "Pixie16WriteSglModPar - IN_SYNC"))
        return false;

    LOG(INFO) << "Calling "
              << "Pixie16StartListModeRun"
              << ".";
    if (!verify_api_return_value(Pixie16StartListModeRun(cfg.num_modules, LIST_MODE_RUN, NEW_RUN),
                                 "Pixie16StartListModeRun"))
        return false;

    std::vector<std::ofstream> output_streams(cfg.num_modules);
    for (int i = 0; i < cfg.num_modules; i++) {
        output_streams[i] = std::ofstream(generate_filename(i, "list-mode", "bin"),
                                          std::ios::out | std::ios::binary);
    }

    std::vector<uint32_t> data(EXTERNAL_FIFO_LENGTH, 0);
    unsigned int num_fifo_words = 0;

    LOG(INFO) << "Collecting data for " << runtime_in_seconds << " s.";
    std::chrono::steady_clock::time_point run_start_time = std::chrono::steady_clock::now();
    while (std::chrono::duration_cast<std::chrono::duration<double>>(
               std::chrono::steady_clock::now() - run_start_time)
               .count() < runtime_in_seconds) {
        for (int mod_num = 0; mod_num < cfg.num_modules; mod_num++) {
            if (Pixie16CheckRunStatus(mod_num) == 1) {
                if (!verify_api_return_value(
                        Pixie16CheckExternalFIFOStatus(&num_fifo_words, mod_num),
                        "Pixie16CheckExternalFIFOStatus", false))
                    return false;

                if (double(num_fifo_words) / EXTERNAL_FIFO_LENGTH > 0.2) {
                    LOG(INFO) << "External FIFO has " << num_fifo_words << " words.";
                    if (!verify_api_return_value(
                            Pixie16ReadDataFromExternalFIFO(data.data(), num_fifo_words, mod_num),
                            "Pixie16ReadDataFromExternalFIFO", false))
                        return false;
                    output_streams[mod_num].write(reinterpret_cast<char*>(data.data()),
                                                  num_fifo_words * sizeof(uint32_t));
                } else {
                    continue;
                }
            } else {
                LOG(INFO) << "Module " << mod_num << " has no active run!";
            }
        }

        /*
         Check the run status of the Director module (module #0) to see if the run has been stopped.
         This is possible in a multi-chassis system where modules in one chassis can stop the run
         in all chassis.
         */
        if (Pixie16CheckRunStatus(0) == 0) {
            LOG(INFO) << "Run was stopped by the director module. Stopping data collection.";
            break;
        }
    }

    /*
     Stop run in the Director module (module #0) - a SYNC interrupt should be generated
     to stop run in all modules simultaneously
     */
    LOG(INFO) << "Stopping List Mode Run.";
    if (!verify_api_return_value(Pixie16EndRun(0), "Pixie16EndRun"))
        return false;

    LOG(INFO) << "Checking that the run is finalized in all the modules.";
    bool all_modules_finished = false;
    const unsigned int max_finalize_attempts = 50;
    for (unsigned int counter = 0; counter < max_finalize_attempts; counter++) {
        for (int k = 0; k < cfg.num_modules; k++) {
            if (Pixie16CheckRunStatus(k) == 1) {
                all_modules_finished = false;
            } else {
                all_modules_finished = true;
            }
        }
        if (all_modules_finished) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (!all_modules_finished) {
        LOG(ERROR) << "All modules did not stop their runs properly!";
        return false;
    }

    LOG(INFO) << "List-mode run finished in "
              << std::chrono::duration_cast<std::chrono::duration<double>>(
                     std::chrono::steady_clock::now() - run_start_time)
                     .count()
              << " s";

    LOG(INFO) << "Reading the final words from the External FIFO and the run statistics.";
    for (int mod_num = 0; mod_num < cfg.num_modules; mod_num++) {
        if (!verify_api_return_value(Pixie16CheckExternalFIFOStatus(&num_fifo_words, mod_num),
                                     "Pixie16CheckExternalFIFOStatus", false))
            return false;

        if (num_fifo_words > 0) {
            LOG(INFO) << "External FIFO has " << num_fifo_words << " words.";
            if (!verify_api_return_value(
                    Pixie16ReadDataFromExternalFIFO(data.data(), num_fifo_words, mod_num),
                    "Pixie16ReadDataFromExternalFIFO", false))
                return false;
            output_streams[mod_num].write(reinterpret_cast<char*>(data.data()),
                                          num_fifo_words * sizeof(uint32_t));
        }

        LOG(INFO) << "Begin Statistics for Module " << mod_num;
        std::vector<unsigned int> stats(448, 0);
        if (!verify_api_return_value(Pixie16ReadStatisticsFromModule(stats.data(), mod_num),
                                     "Pixie16ReadStatisticsFromModule", false))
            return false;

        LOG(INFO) << "Real Time: " << Pixie16ComputeRealTime(stats.data(), mod_num);
        for (unsigned int chan = 0; chan < NUMBER_OF_CHANNELS; chan++) {
            LOG(INFO) << "Channel " << chan
                      << " LiveTime: " << Pixie16ComputeLiveTime(stats.data(), mod_num, chan);
            LOG(INFO) << "Channel " << chan << " Input Count Rate: "
                      << Pixie16ComputeInputCountRate(stats.data(), mod_num, chan);
            LOG(INFO) << "Channel " << chan << " Output Count Rate: "
                      << Pixie16ComputeOutputCountRate(stats.data(), mod_num, chan);
        }
        LOG(INFO) << "End Statistics for Module " << mod_num;
    }

    return true;
}

bool execute_mca_run(const unsigned int& mod, const double& runtime_in_seconds) {
    LOG(INFO) << "Calling Pixie16WriteSglModPar to write HOST_RT_PRESET to " << runtime_in_seconds;
    if (!verify_api_return_value(
            Pixie16WriteSglModPar("HOST_RT_PRESET", Decimal2IEEEFloating(runtime_in_seconds), mod),
            "Pixie16WriteSglModPar - HOST_RT_PRESET"))
        return false;

    LOG(INFO) << "Calling Pixie16WriteSglModPar to write SYNCH_WAIT = 0 in Module 0.";
    if (!verify_api_return_value(Pixie16WriteSglModPar("SYNCH_WAIT", 0, mod),
                                 "Pixie16WriteSglModPar - SYNC_WAIT"))
        return false;

    LOG(INFO) << "Calling Pixie16WriteSglModPar to write IN_SYNCH  = 1 in Module 0.";
    if (!verify_api_return_value(Pixie16WriteSglModPar("IN_SYNCH", 1, mod),
                                 "Pixie16WriteSglModPar - IN_SYNC"))
        return false;

    LOG(INFO) << "Starting MCA data run for " << runtime_in_seconds << " s.";
    if (!verify_api_return_value(Pixie16StartHistogramRun(mod, NEW_RUN),
                                 "Pixie16StartHistogramRun"))
        return false;

    std::chrono::steady_clock::time_point run_start_time = std::chrono::steady_clock::now();
    while (std::chrono::duration_cast<std::chrono::duration<double>>(
               std::chrono::steady_clock::now() - run_start_time)
               .count() < runtime_in_seconds) {
        //do nothing
    }
    LOG(INFO) << "Stopping MCA data run after "
              << std::chrono::duration_cast<std::chrono::duration<double>>(
                     std::chrono::steady_clock::now() - run_start_time)
                     .count()
              << " s.";
    if (!verify_api_return_value(Pixie16EndRun(mod), "Pixie16EndRun"))
        return false;

    std::string name = generate_filename(mod, "mca", "csv");
    std::ofstream out(name);
    out << "bin,";

    std::vector<std::vector<uint32_t>> hists;
    for (unsigned int i = 0; i < NUMBER_OF_CHANNELS; i++) {
        std::vector<uint32_t> hist(MAX_HISTOGRAM_LENGTH, 0);
        Pixie16ReadHistogramFromModule(hist.data(), MAX_HISTOGRAM_LENGTH, mod, i);
        hists.push_back(hist);
        out << "Chan" << i << ",";
    }
    out << std::endl;

    for (unsigned int bin = 0; bin < MAX_HISTOGRAM_LENGTH; bin++) {
        out << bin << ",";
        for (auto hist : hists) {
            out << hist[bin] << ",";
        }
        out << std::endl;
    }
    return true;
}

bool execute_parameter_read(args::ValueFlag<std::string>& parameter,
                            args::ValueFlag<unsigned int>& crate,
                            args::ValueFlag<unsigned int>& module,
                            args::ValueFlag<unsigned int>& channel) {
    if (channel) {
        double result;
        LOG(INFO) << "Pixie16ReadSglChanPar"
                  << " reading " << parameter.Get() << " from Crate " << crate.Get() << " Module "
                  << module.Get() << " Channel " << channel.Get() << ".";
        if (!verify_api_return_value(Pixie16ReadSglChanPar(parameter.Get().c_str(), &result,
                                                           module.Get(), channel.Get()),
                                     "Pixie16ReadSglChanPar", false))
            return false;
        LOG(INFO) << result;
    } else {
        unsigned int result;
        LOG(INFO) << "Pixie16ReadSglModPar"
                  << " reading " << parameter.Get() << " from Crate " << crate.Get() << " Module "
                  << module.Get() << ".";
        if (!verify_api_return_value(
                Pixie16ReadSglModPar(parameter.Get().c_str(), &result, module.Get()),
                "Pixie16ReadSglModPar", false))
            return false;
        LOG(INFO) << result;
    }
    return true;
}

bool execute_parameter_write(args::ValueFlag<std::string>& parameter,
                             args::ValueFlag<double>& value, args::ValueFlag<unsigned int>& crate,
                             args::ValueFlag<unsigned int>& module,
                             args::ValueFlag<unsigned int>& channel, const std::string& setfile) {
    if (channel) {
        LOG(INFO) << "Pixie16WriteSglChanPar"
                  << " setting " << parameter.Get() << " to " << value.Get() << " for Crate "
                  << crate.Get() << " Module " << module.Get() << " Channel " << channel.Get()
                  << ".";
        if (!verify_api_return_value(Pixie16WriteSglChanPar(parameter.Get().c_str(), value.Get(),
                                                            module.Get(), channel.Get()),
                                     "Pixie16WriteSglChanPar"))
            return false;
    } else {
        LOG(INFO) << "Pixie16WriteSglModPar"
                  << " setting " << parameter.Get() << " to " << value.Get() << " for  Crate "
                  << crate.Get() << " Module " << module.Get() << ".";
        if (!verify_api_return_value(
                Pixie16WriteSglModPar(parameter.Get().c_str(), value, module.Get()),
                "Pixie16WriteSglModPar"))
            return false;
    }

    if (!save_dsp_pars(setfile))
        return false;
    return true;
}

bool execute_trace_capture(args::ValueFlag<unsigned int>& module) {
    if (!module)
        return false;

    LOG(INFO) << "Pixie16AcquireADCTrace"
              << " acquiring traces for Module" << module.Get() << ".";
    if (!verify_api_return_value(Pixie16AcquireADCTrace(module.Get()), "Pixie16AcquireADCTrace"))
        return false;


    unsigned short trace[NUMBER_OF_CHANNELS][MAX_ADC_TRACE_LEN];
    for (unsigned int i = 0; i < NUMBER_OF_CHANNELS; i++) {
        if (!verify_api_return_value(
                Pixie16ReadSglChanADCTrace(trace[i], MAX_ADC_TRACE_LEN, module.Get(), i),
                "Pixie16AcquireADCTrace", false))
            return false;
    }

    std::ofstream ofstream1(generate_filename(module.Get(), "adc", "csv"));
    ofstream1 << "bin,";
    for (unsigned int i = 0; i < NUMBER_OF_CHANNELS; i++)
        ofstream1 << "Chan" << i << ",";
    ofstream1 << std::endl;
    for (unsigned int i = 0; i < MAX_ADC_TRACE_LEN; i++) {
        ofstream1 << i << ",";
        for (unsigned int k = 0; k < NUMBER_OF_CHANNELS; k++) {
            if (k != NUMBER_OF_CHANNELS - 1)
                ofstream1 << trace[k][i] << ",";
            else
                ofstream1 << trace[k][i];
        }
        ofstream1 << std::endl;
    }

    return true;
}

bool execute_blcut(args::ValueFlag<unsigned int>& module, args::ValueFlag<unsigned int>& channel) {
    if (!module)
        return false;

    LOG(INFO) << "Executing Pixie16BLcutFinder for Module" << module.Get() << ".";
    unsigned int blcut = 0;
    if (!verify_api_return_value(Pixie16BLcutFinder(module.Get(), channel.Get(), &blcut),
                                 "Pixie16BLcutFinder", false))
        return false;
    LOG(INFO) << "BLCut for Module " << module.Get() << " "
              << "Channel " << channel.Get() << " is " << blcut;
    return true;
}

bool execute_set_dacs(args::ValueFlag<unsigned int>& module) {
    if (!module)
        return false;

    LOG(INFO) << "Executing Pixie16SetDACs for Module" << module.Get() << ".";
    if (!verify_api_return_value(Pixie16SetDACs(module.Get()), "Pixie16SetDACs", false))
        return false;
    return true;
}

bool execute_close_module_connection(const int& numModules) {
    for (int i = 0; i < numModules; i++) {
        LOG(INFO) << "Closing out connection to Module " << i << ".";
        verify_api_return_value(Pixie16ExitSystem(i),
                                "Pixie16ExitSystem for Module" + std::to_string(i));
    }
    return true;
}

double calculate_duration_in_seconds(const std::chrono::system_clock::time_point& start,
                                     const std::chrono::system_clock::time_point& end) {
    return std::chrono::duration<double>(end - start).count();
}

void configure_logging(int argc, char** argv) {
    START_EASYLOGGINGPP(argc, argv);
    el::Configurations defaultConf;
    defaultConf.setToDefault();
    defaultConf.setGlobally(el::ConfigurationType::Filename, "example_pixie16app.log");
    defaultConf.setGlobally(el::ConfigurationType::Format,
                            "%datetime{%Y-%M-%dT%H:%m:%s.%g} - %level - %msg");
    el::Loggers::reconfigureLogger("default", defaultConf);
}

int main(int argc, char** argv) {
    auto start = std::chrono::system_clock::now();
    configure_logging(argc, argv);
    args::ArgumentParser parser(
        "Sample code that interfaces with a Pixie system through the User API.");
    parser.LongSeparator("=");


    args::Group commands(parser, "commands");
    args::Command boot(commands, "boot", "Boots the crate of modules.");
    args::Command export_settings(
        commands, "export-settings",
        "Boots the system and dumps the settings to the file defined in the config.");
    args::Command histogram(commands, "histogram", "Save histograms from the module.");
    args::Command init(commands, "init", "Initializes the system without going any farther.");
    args::Command list_mode(commands, "list-mode", "Starts a list mode data run");
    args::Command read(commands, "read", "Read a parameter from the module.");
    args::Command write(commands, "write", "Write a parameter to the module.");
    args::Command trace(commands, "trace", "Captures traces from the modules.");
    args::Command adjust_offsets(commands, "adjust_offsets",
                                 "Adjusts the DC offsets for all modules in the config file.");
    args::Command baseline(commands, "baseline", "Acquire and print baselines from the module");
    args::Command mca(commands, "mca", "Starts an MCA data run.");
    args::Command blcut(commands, "blcut",
                        "Starts a control task to find the BLCut for a channel.");
    args::Command dacs(commands, "dacs", "Starts a control task to set the module's DACs");

    args::Group arguments(parser, "arguments", args::Group::Validators::AtLeastOne,
                          args::Options::Global);
    args::Flag api_pixie(arguments, "api", "Select the Pixie API (default: pixie16)", {'P', "api"});

    args::ValueFlag<std::string> conf_flag(arguments, "cfg", "The configuration file to load.",
                                           {'c', "config"}, args::Options::Required);
    args::HelpFlag help_flag(arguments, "help", "Displays this message", {'h', "help"});
    args::Flag is_fast_boot(boot, "fast-boot", "Performs a partial boot of the system.",
                            {'f', "fast-boot"});
    args::Flag is_offline(arguments, "Offline Mode",
                          "Tells the API to use Offline mode when running.", {'o', "offline"});
    args::ValueFlag<std::string> boot_pattern_flag(arguments, "boot_pattern",
                                                   "The boot pattern used for booting.",
                                                   {'b', "boot_pattern"}, "0x7F");
    args::ValueFlag<double> run_time(
        list_mode, "time", "The amount of time that a list mode run will take in seconds.",
        {'t', "run-time"}, 10.);
    args::ValueFlag<std::string> parameter(
        read, "parameter", "The parameter we want to read from the system.", {'n', "name"});
    args::ValueFlag<unsigned int> crate(read, "crate", "The crate", {"crate"}, 0);
    args::ValueFlag<unsigned int> module(read, "module", "The module", {"mod"});
    args::ValueFlag<unsigned int> channel(read, "channel", "The channel", {"chan"});
    args::ValueFlag<double> parameter_value(
        write, "parameter_value", "The value of the parameter we want to write.", {'v', "value"});
    adjust_offsets.Add(conf_flag);
    adjust_offsets.Add(boot_pattern_flag);
    baseline.Add(is_fast_boot);
    baseline.Add(boot_pattern_flag);
    baseline.Add(module);
    blcut.Add(module);
    blcut.Add(channel);
    boot.Add(conf_flag);
    boot.Add(boot_pattern_flag);
    dacs.Add(module);
    mca.Add(module);
    mca.Add(boot_pattern_flag);
    read.Add(conf_flag);
    trace.Add(conf_flag);
    trace.Add(module);
    trace.Add(boot_pattern_flag);
    write.Add(conf_flag);
    write.Add(parameter);
    write.Add(crate);
    write.Add(module);
    write.Add(channel);

    try {
        parser.ParseCLI(argc, argv);
    } catch (args::Help& help) {
        LOG(INFO) << help.what();
        std::cout << parser;
        return EXIT_SUCCESS;
    } catch (args::Error& e) {
        LOG(ERROR) << e.what();
        std::cout << parser;
        return EXIT_FAILURE;
    }

    configuration cfg;
    try {
        read_config(conf_flag.Get(), cfg);
    } catch (std::exception& e) {
        LOG(ERROR) << e.what();
        return EXIT_FAILURE;
    }

    LOG(INFO) << "Finished reading config in "
              << calculate_duration_in_seconds(start, std::chrono::system_clock::now()) << " s.";

    int offline_mode = 0;
    if (is_offline)
        offline_mode = 1;

    start = std::chrono::system_clock::now();
    LOG(INFO) << "Calling "
              << "Pixie16InitSystem.";
    if (!verify_api_return_value(
            Pixie16InitSystem(cfg.num_modules, cfg.slot_map.data(), offline_mode),
            "Pixie16InitSystem", false))
        return EXIT_FAILURE;

    LOG(INFO) << "Finished Pixie16InitSystem in "
              << calculate_duration_in_seconds(start, std::chrono::system_clock::now()) << " s.";

    if (init)
        return EXIT_SUCCESS;

    start = std::chrono::system_clock::now();
    unsigned int boot_pattern = stoul(args::get(boot_pattern_flag), nullptr, 0);
    if (is_fast_boot)
        boot_pattern = 0x70;

    LOG(INFO) << "Calling Pixie16BootModule with boot pattern: " << std::showbase << std::hex
              << boot_pattern << std::dec;

    if (!verify_api_return_value(
            Pixie16BootModule(cfg.com_fpga_config.c_str(), cfg.sp_fpga_config.c_str(), nullptr,
                              cfg.dsp_code.c_str(), cfg.dsp_param.c_str(), cfg.dsp_var.c_str(),
                              cfg.num_modules, boot_pattern),
            "Pixie16BootModule", "Finished booting!"))
        return EXIT_FAILURE;
    LOG(INFO) << "Finished Pixie16BootModule in "
              << calculate_duration_in_seconds(start, std::chrono::system_clock::now()) << " s.";
    if (boot) {
        execute_close_module_connection(cfg.num_modules);
        return EXIT_SUCCESS;
    }

    if (read) {
        if (!execute_parameter_read(parameter, crate, module, channel))
            return EXIT_FAILURE;
        execute_close_module_connection(cfg.num_modules);
        return EXIT_SUCCESS;
    }

    if (write) {
        if (!execute_parameter_write(parameter, parameter_value, crate, module, channel,
                                     cfg.dsp_param))
            return EXIT_FAILURE;
        execute_close_module_connection(cfg.num_modules);
        return EXIT_SUCCESS;
    }

    if (adjust_offsets) {
        if (!execute_adjust_offsets(cfg.num_modules, cfg.dsp_param))
            return EXIT_FAILURE;
        execute_close_module_connection(cfg.num_modules);
        return EXIT_SUCCESS;
    }

    if (trace) {
        if (!execute_trace_capture(module))
            return EXIT_FAILURE;
        execute_close_module_connection(cfg.num_modules);
        return EXIT_SUCCESS;
    }

    if (list_mode) {
        if (!execute_list_mode_run(cfg, run_time.Get()))
            return EXIT_FAILURE;
        execute_close_module_connection(cfg.num_modules);
        return EXIT_SUCCESS;
    }

    if (export_settings) {
        if (!save_dsp_pars(cfg.dsp_param))
            return EXIT_FAILURE;
        return EXIT_SUCCESS;
    }

    if (baseline) {
        if (!execute_baseline_capture(args::get(module)))
            return EXIT_FAILURE;
        execute_close_module_connection(cfg.num_modules);
        return EXIT_SUCCESS;
    }

    if (mca) {
        if (!execute_mca_run(args::get(module), run_time.Get()))
            return EXIT_FAILURE;
        execute_close_module_connection(cfg.num_modules);
        return EXIT_SUCCESS;
    }

    if (blcut) {
        if (!execute_blcut(module, channel))
            return EXIT_FAILURE;
        execute_close_module_connection(cfg.num_modules);
        return EXIT_SUCCESS;
    }

    if (dacs) {
        if (!execute_set_dacs(module))
            return EXIT_FAILURE;
        execute_close_module_connection(cfg.num_modules);
        return EXIT_SUCCESS;
    }

    execute_close_module_connection(cfg.num_modules);
}
