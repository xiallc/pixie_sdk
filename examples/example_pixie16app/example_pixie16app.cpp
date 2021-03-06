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
 * @brief Demonstrates how to use the Pixie16App functions to communicate with Pixie-16 modules.
 * @note The Legacy C API is now deprecated and will no longer receive support outside critical bug
 * fixes. **We will remove the legacy C API on July 31, 2023**.
 *
 * We demonstrate the Legacy C implementation using `Pixie16App.so`. We **really** recommend that
 * you link your code directly with `Pixie16Api.so`.
 */

#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <thread>
#include <vector>

#include <args/args.hxx>
#include <nolhmann/json.hpp>

#include <pixie16app_defs.h>
#include <pixie16app_export.h>
#include <pixie16sys_defs.h>

#if defined(_WIN64) || defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

struct LOG {
    explicit LOG(const std::string& type) {
        type_ = type;

        std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
        std::time_t currentTime = std::chrono::system_clock::to_time_t(now);
        std::chrono::milliseconds now2 =
            std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
        char timeBuffer[80];
        std::strftime(timeBuffer, 80, "%FT%T", gmtime(&currentTime));

        std::stringstream tmp;
        tmp << timeBuffer << "." << std::setfill('0') << std::setw(3) << now2.count() % 1000 << "Z";

        datetime_ = tmp.str();
    }

    friend std::ostream& operator<<(std::ostream& os, const LOG& log) {
        os << log.datetime_ << " - " << log.type_ << " - ";
        return os;
    }

    std::string type_;
    std::string datetime_;
};

struct firmware_spec {
    unsigned int version;
    int revision;
    int adc_msps;
    int adc_bits;

    firmware_spec() : version(0), revision(0), adc_msps(0), adc_bits(0) {}
};

struct module_config {
    std::string com_fpga_config;
    std::string dsp_code;
    std::string dsp_par;
    std::string dsp_var;
    std::string sp_fpga_config;
    unsigned int serial_number;
    unsigned short adc_bit_resolution;
    unsigned short adc_sampling_frequency;
    unsigned short number;
    unsigned short number_of_channels;
    unsigned short revision;
    unsigned short slot;
    firmware_spec fw;
};

typedef std::vector<module_config> module_configs;

struct configuration {
    module_configs modules;
    std::vector<unsigned short> slot_def;
    unsigned short num_modules() const {
        return static_cast<unsigned short>(modules.size());
    }
};

std::string generate_filename(const unsigned int& module_number, const std::string& type,
                              const std::string& ext) {
    return "pixie16app-module" + std::to_string(module_number) + "-" + type + "." + ext;
}

void verify_json_module(const nlohmann::json& mod) {
    if (!mod.contains("slot")) {
        throw std::invalid_argument("Missing slot definition in configuration element.");
    }

    if (!mod.contains("dsp")) {
        throw std::invalid_argument("Missing dsp object in configuration element.");
    }

    if (!mod["dsp"].contains("ldr") || !mod["dsp"].contains("var") || !mod["dsp"].contains("par")) {
        throw std::invalid_argument(
            "Missing dsp object in configuration element: ldr, dsp, or par.");
    }

    if (!mod.contains("fpga")) {
        throw std::invalid_argument("Missing fpga object in configuration element.");
    }

    if (!mod["fpga"].contains("fippi") || !mod["fpga"].contains("sys")) {
        throw std::invalid_argument("Missing fpga firmware definition (fippi or sys).");
    }

    if (mod.contains("fw")) {
        if (!mod["fw"].contains("version") || !mod["fw"].contains("revision") ||
            !mod["fw"].contains("adc_msps") || !mod["fw"].contains("adc_bits")) {
            throw std::invalid_argument(
                "Missing firmware (fw) definition (version, revision, adc_msps or adc_bits).");
        }
    }
}

void read_config(const std::string& config_file_name, configuration& cfg) {
    std::ifstream input(config_file_name, std::ios::in);
    if (input.fail()) {
        throw std::ios_base::failure("open: " + config_file_name + ": " + std::strerror(errno));
    }

    nlohmann::json jf = nlohmann::json::parse(input);
    input.close();

    if (jf.empty() || jf.size() > SYS_MAX_NUM_MODULES) {
        throw std::invalid_argument("invalid number of modules");
    }

    cfg.slot_def.clear();
    for (const auto& module : jf) {
        verify_json_module(module);

        cfg.slot_def.push_back(module["slot"]);

        module_config mod_cfg;
        mod_cfg.slot = module["slot"];
        mod_cfg.number = static_cast<unsigned short>(cfg.slot_def.size() - 1);
        mod_cfg.com_fpga_config = module["fpga"]["sys"];
        mod_cfg.sp_fpga_config = module["fpga"]["fippi"];
        mod_cfg.dsp_code = module["dsp"]["ldr"];
        mod_cfg.dsp_par = module["dsp"]["par"];
        mod_cfg.dsp_var = module["dsp"]["var"];
        if (module.contains("fw")) {
            mod_cfg.fw.version = module["fw"]["version"];
            mod_cfg.fw.revision = module["fw"]["revision"];
            mod_cfg.fw.adc_msps = module["fw"]["adc_msps"];
            mod_cfg.fw.adc_bits = module["fw"]["adc_bits"];
        }
        cfg.modules.push_back(mod_cfg);
    }
}

bool verify_api_return_value(const int& val, const std::string& func_name,
                             const bool& print_success = true) {
    if (val < 0) {
        std::string msg;
        msg = "error message output not supported in legacy API.";
        std::cout << LOG("ERROR") << func_name << " failed with code " << val
                  << " and message: " << msg << std::endl;
        return false;
    }
    if (print_success)
        std::cout << LOG("INFO") << func_name << " finished successfully." << std::endl;
    return true;
}

bool output_statistics_data(const module_config& mod, const std::string& type) {
    std::vector<unsigned int> stats(N_DSP_PAR - DSP_IO_BORDER, 0);
    if (!verify_api_return_value(Pixie16ReadStatisticsFromModule(stats.data(), mod.number),
                                 "Pixie16ReadStatisticsFromModule", false))
        return false;

    std::ofstream csv_output(generate_filename(mod.number, type, "csv"), std::ios::out);
    csv_output << "channel,real_time,live_time,input_count_rate,output_count_rate" << std::endl;
    auto real_time = Pixie16ComputeRealTime(stats.data(), mod.number);

    std::cout << LOG("INFO") << "Begin Statistics for Module " << mod.number << std::endl;
    for (unsigned int chan = 0; chan < mod.number_of_channels; chan++) {
        auto live_time = Pixie16ComputeLiveTime(stats.data(), mod.number, chan);
        auto icr = Pixie16ComputeInputCountRate(stats.data(), mod.number, chan);
        auto ocr = Pixie16ComputeOutputCountRate(stats.data(), mod.number, chan);

        nlohmann::json json_stats = {
            {"module", mod.number},   {"channel", chan}, {"real_time", real_time},
            {"live_time", live_time}, {"icr", icr},      {"ocr", ocr},
        };

        csv_output << chan << "," << real_time << "," << live_time << "," << icr << "," << ocr
                   << std::endl;
        std::cout << LOG("INFO") << json_stats << std::endl;
    }

    std::cout << LOG("INFO") << "End Statistics for Module " << mod.number << std::endl;
    csv_output.close();
    return true;
}


bool save_dsp_pars(const std::string& filename) {
    std::cout << LOG("INFO") << "Saving DSP Parameters to " << filename << "." << std::endl;
    if (!verify_api_return_value(Pixie16SaveDSPParametersToFile(filename.c_str()),
                                 "Pixie16SaveDSPParametersToFile"))
        return false;
    return true;
}

bool execute_adjust_offsets(const module_config& module) {
    std::cout << LOG("INFO") << "Adjusting baseline offset for Module " << module.number << "."
              << std::endl;
    if (!verify_api_return_value(Pixie16AdjustOffsets(module.number),
                                 "Pixie16AdjustOffsets for Module " +
                                     std::to_string(module.number)))
        return false;
    if (!save_dsp_pars(module.dsp_par))
        return false;
    return true;
}

bool execute_baseline_capture(const module_config& mod) {
    std::cout << LOG("INFO") << "Starting baseline capture for Module " << mod.number << std::endl;
    if (!verify_api_return_value(Pixie16AcquireBaselines(mod.number), "Pixie16AcquireBaselines"))
        return false;

    std::vector<std::vector<double>> baselines;
    std::vector<std::vector<double>> timestamps;
    unsigned int max_num_baselines = 0;
    for (unsigned int i = 0; i < mod.number_of_channels; i++) {
        max_num_baselines = MAX_NUM_BASELINES;
        std::vector<double> baseline(max_num_baselines);
        std::vector<double> timestamp(max_num_baselines);

        std::cout << LOG("INFO") << "Acquiring " << max_num_baselines << " baselines for Channel "
                  << i << std::endl;
        if (!verify_api_return_value(Pixie16ReadSglChanBaselines(baseline.data(), timestamp.data(),
                                                                 max_num_baselines, mod.number, i),
                                     "Pixie16ReadSglChanBaselines"))
            return false;
        baselines.push_back(baseline);
        timestamps.push_back(timestamp);
    }

    std::ofstream ofstream1(generate_filename(mod.number, "baselines", "csv"));
    ofstream1 << "bin,timestamp,";
    for (unsigned int i = 0; i < mod.number_of_channels; i++) {
        if (i != static_cast<unsigned int>(mod.number_of_channels - 1))
            ofstream1 << "Chan" << i << ",";
        else
            ofstream1 << "Chan" << i;
    }
    ofstream1 << std::endl;

    for (unsigned int i = 0; i < max_num_baselines; i++) {
        ofstream1 << i << "," << timestamps[0][i] << ",";
        for (unsigned int k = 0; k < mod.number_of_channels; k++) {
            if (k != static_cast<unsigned int>(mod.number_of_channels - 1))
                ofstream1 << baselines[k][i] << ",";
            else
                ofstream1 << baselines[k][i];
        }
        ofstream1 << std::endl;
    }
    return true;
}

bool execute_list_mode_run(unsigned int run_num, const configuration& cfg,
                           const double& runtime_in_seconds, unsigned int synch_wait,
                           unsigned int in_synch) {
    std::cout << LOG("INFO") << "Starting list mode data run for " << runtime_in_seconds << " s."
              << std::endl;

    std::cout << LOG("INFO") << "Calling Pixie16WriteSglModPar to write SYNCH_WAIT = " << synch_wait
              << " in Module 0." << std::endl;
    if (!verify_api_return_value(Pixie16WriteSglModPar("SYNCH_WAIT", synch_wait, 0),
                                 "Pixie16WriteSglModPar - SYNC_WAIT"))
        return false;

    std::cout << LOG("INFO") << "Calling Pixie16WriteSglModPar to write IN_SYNCH  = " << in_synch
              << " in Module 0." << std::endl;
    if (!verify_api_return_value(Pixie16WriteSglModPar("IN_SYNCH", in_synch, 0),
                                 "Pixie16WriteSglModPar - IN_SYNC"))
        return false;

    std::cout << LOG("INFO") << "Starting list-mode run." << std::endl;
    if (!verify_api_return_value(
            Pixie16StartListModeRun(cfg.num_modules(), LIST_MODE_RUN, NEW_RUN),
            "Pixie16StartListModeRun"))
        return false;

    /*
     * Due to communication inefficiencies in the legacy API we need to pause program
     * execution for a second to wait for the module to be able to report that the
     * run has actually started. Without this pause the run_status will return 0
     * despite the fact that we just started the run above.
     *
     * This is **not** necessary in the SDK because we have optimized the module
     * communication.
     */
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    std::vector<std::ofstream*> output_streams(cfg.num_modules());
    for (unsigned short i = 0; i < cfg.num_modules(); i++) {
        output_streams[i] = new std::ofstream(
            generate_filename(i, "list-mode-run" + std::to_string(run_num), "bin"),
            std::ios::out | std::ios::binary);
    }

    unsigned int num_fifo_words = 0;

    std::cout << LOG("INFO") << "Collecting data for " << runtime_in_seconds << " s." << std::endl;
    std::chrono::steady_clock::time_point run_start_time = std::chrono::steady_clock::now();
    double current_run_time = 0;
    double check_time = 0;
    bool run_status = Pixie16CheckRunStatus(cfg.modules[0].number);

    while (run_status != 0) {
        current_run_time = std::chrono::duration_cast<std::chrono::duration<double>>(
                               std::chrono::steady_clock::now() - run_start_time)
                               .count();

        if (current_run_time >= runtime_in_seconds) {
            if (synch_wait == 0) {
                std::cout << LOG("INFO") << "Stopping list-mode run individually." << std::endl;
                if (!verify_api_return_value(Pixie16EndRun(cfg.num_modules()), "Pixie16EndRun"))
                    return false;
            } else {
                /*
                 * Stop run in the Director module (module #0) - a SYNC interrupt should be generated
                 *  to stop run in all modules simultaneously
                 */
                std::cout << LOG("INFO") << "Stopping list-mode run in director module."
                          << std::endl;
                if (!verify_api_return_value(Pixie16EndRun(cfg.modules[0].number), "Pixie16EndRun"))
                    return false;
            }

            break;
        }

        if (current_run_time - check_time > 1) {
            if (current_run_time < runtime_in_seconds)
                std::cout << LOG("INFO") << "Remaining run time: "
                          << std::round(runtime_in_seconds - current_run_time) << " s" << std::endl;
            check_time = current_run_time;
        }

        for (unsigned short mod_num = 0; mod_num < cfg.num_modules(); mod_num++) {
            if (Pixie16CheckRunStatus(mod_num) == 1) {
                if (!verify_api_return_value(
                        Pixie16CheckExternalFIFOStatus(&num_fifo_words, mod_num),
                        "Pixie16CheckExternalFIFOStatus", false))
                    return false;


                std::cout << LOG("INFO") << "FIFO has " << num_fifo_words << " words." << std::endl;
                /*
                     * NOTE: The PixieSDK now uses threaded list-mode FIFO workers that live on the host machine. These
                     * workers perform execute in parallel. They'll read the data from each module as needed to
                     * ensure that the EXTERNAL_FIFO_LENGTH isn't exceeded. When calling
                     * `Pixie16CheckExternalFIFOStatus`, you're actually checking the status of the FIFO workers for
                     * that module.
                     *
                     * We've gated the reads in this example using one-second intervals, but you don't have to.
                     */
                if (num_fifo_words > 0) {
                    std::vector<uint32_t> data(num_fifo_words, 0);
                    if (!verify_api_return_value(
                            Pixie16ReadDataFromExternalFIFO(data.data(), num_fifo_words, mod_num),
                            "Pixie16ReadDataFromExternalFIFO", false))
                        return false;
                    output_streams[mod_num]->write(reinterpret_cast<char*>(data.data()),
                                                   num_fifo_words * sizeof(uint32_t));
                }
            } else {
                std::cout << LOG("INFO") << "Module " << mod_num << " has no active run!"
                          << std::endl;
            }
        }

        /*
         * Check the run status of the Director module (module #0) to see if the run has been stopped.
         * This is possible in a multi-chassis system where modules in one chassis can stop the run
         * in all chassis.
         */
        run_status = Pixie16CheckRunStatus(cfg.modules[0].number);

        //Temper the thread so that we don't slam the module with run status requests.
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << LOG("INFO") << "Checking that the run is finalized in all the modules."
              << std::endl;
    bool all_modules_finished = false;
    const unsigned int max_finalize_attempts = 50;
    for (unsigned int counter = 0; counter < max_finalize_attempts; counter++) {
        for (unsigned short k = 0; k < cfg.num_modules(); k++) {
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
        std::cout << LOG("ERROR") << "All modules did not stop their runs properly!" << std::endl;
        return false;
    }

    std::cout << LOG("INFO") << "List-mode run finished in "
              << std::chrono::duration_cast<std::chrono::duration<double>>(
                     std::chrono::steady_clock::now() - run_start_time)
                     .count()
              << " s" << std::endl;

    std::cout << LOG("INFO")
              << "Reading the final words from the External FIFO and the run statistics."
              << std::endl;
    for (unsigned short mod_num = 0; mod_num < cfg.num_modules(); mod_num++) {
        if (!verify_api_return_value(Pixie16CheckExternalFIFOStatus(&num_fifo_words, mod_num),
                                     "Pixie16CheckExternalFIFOStatus", false))
            return false;

        if (num_fifo_words > 0) {
            std::cout << LOG("INFO") << "External FIFO has " << num_fifo_words << " words."
                      << std::endl;
            std::vector<uint32_t> data(num_fifo_words, 0);
            if (!verify_api_return_value(
                    Pixie16ReadDataFromExternalFIFO(data.data(), num_fifo_words, mod_num),
                    "Pixie16ReadDataFromExternalFIFO", false))
                return false;
            output_streams[mod_num]->write(reinterpret_cast<char*>(data.data()),
                                           num_fifo_words * sizeof(uint32_t));
        }
        if (!output_statistics_data(cfg.modules[mod_num],
                                    "list-mode-stats-run" + std::to_string(run_num))) {
            return false;
        }
    }

    for (auto& stream : output_streams)
        stream->close();

    return true;
}

bool execute_list_mode_runs(const unsigned int num_runs, const configuration& cfg,
                            const double& runtime_in_seconds, unsigned int synch_wait,
                            unsigned int in_synch) {
    for (unsigned int i = 0; i < num_runs; i++) {
        std::cout << LOG("INFO") << "Starting list-mode run number " << i << std::endl;
        if (!execute_list_mode_run(i, cfg, runtime_in_seconds, synch_wait, in_synch)) {
            std::cout << LOG("INFO") << "List-mode data run " << i
                      << " failed! See log for more details." << std::endl;
            return false;
        }
        std::cout << LOG("INFO") << "Finished list-mode run number " << i << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    return true;
}

bool execute_mca_run(const int run_num, const module_config& mod, const double runtime_in_seconds,
                     unsigned int synch_wait, unsigned int in_synch) {
    std::cout << LOG("INFO") << "Calling Pixie16WriteSglModPar to write HOST_RT_PRESET to "
              << runtime_in_seconds << std::endl;
    if (!verify_api_return_value(Pixie16WriteSglModPar("HOST_RT_PRESET",
                                                       Decimal2IEEEFloating(runtime_in_seconds),
                                                       mod.number),
                                 "Pixie16WriteSglModPar - HOST_RT_PRESET"))
        return false;

    std::cout << LOG("INFO") << "Calling Pixie16WriteSglModPar to write SYNCH_WAIT = " << synch_wait
              << " in Module 0." << std::endl;
    if (!verify_api_return_value(Pixie16WriteSglModPar("SYNCH_WAIT", synch_wait, mod.number),
                                 "Pixie16WriteSglModPar - SYNC_WAIT"))
        return false;

    std::cout << LOG("INFO") << "Calling Pixie16WriteSglModPar to write IN_SYNCH  = " << in_synch
              << " in Module 0." << std::endl;
    if (!verify_api_return_value(Pixie16WriteSglModPar("IN_SYNCH", in_synch, mod.number),
                                 "Pixie16WriteSglModPar - IN_SYNC"))
        return false;

    std::cout << LOG("INFO") << "Starting MCA data run for " << runtime_in_seconds << " s."
              << std::endl;
    if (!verify_api_return_value(Pixie16StartHistogramRun(mod.number, NEW_RUN),
                                 "Pixie16StartHistogramRun"))
        return false;

    /*
     * Due to communication inefficiencies in the legacy API we need to pause program
     * execution for a second to wait for the module to be able to report that the
     * run has actually started. Without this pause the run_status will return 0
     * despite the fact that we just started the run above.
     *
     * This is **not** necessary in the SDK because we have optimized the module
     * communication.
     */
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    auto run_start_time = std::chrono::steady_clock::now();
    double current_run_time = 0;
    double check_time = 0;
    bool run_status = Pixie16CheckRunStatus(mod.number);
    while (run_status != 0) {
        current_run_time = std::chrono::duration_cast<std::chrono::duration<double>>(
                               std::chrono::steady_clock::now() - run_start_time)
                               .count();

        if (current_run_time - check_time > 1) {
            run_status = Pixie16CheckRunStatus(mod.number);
            if (current_run_time < runtime_in_seconds)
                std::cout << LOG("INFO")
                          << "Remaining run time: " << runtime_in_seconds - current_run_time << " s"
                          << std::endl;
            check_time = current_run_time;
        }

        if (current_run_time > runtime_in_seconds + 5) {
            std::cout << LOG("ERROR") << "MCA Run failed to stop in the module!" << std::endl;
            std::cout << LOG("WARN") << "Forcing end of MCA run." << std::endl;
            if (!verify_api_return_value(Pixie16EndRun(mod.number), "Pixie16EndRun"))
                return false;
        }
    }

    if (current_run_time < runtime_in_seconds) {
        std::cout << LOG("ERROR") << "MCA Run exited prematurely! Check log for more details."
                  << std::endl;
    } else {
        //@todo We need to temporarily execute a manual end run until P16-440 is complete.
        if (!verify_api_return_value(Pixie16EndRun(mod.number), "Pixie16EndRun"))
            return false;
        std::cout << LOG("INFO") << "MCA Run finished!" << std::endl;
    }

    std::string name = generate_filename(mod.number, "mca-run" + std::to_string(run_num), "csv");
    std::ofstream out(name);
    out << "bin,";

    std::vector<std::vector<uint32_t>> hists;
    unsigned int max_histogram_length = 0;
    for (unsigned int i = 0; i < mod.number_of_channels; i++) {
        std::vector<uint32_t> hist(MAX_HISTOGRAM_LENGTH, 0);
        if (hist.size() > max_histogram_length)
            max_histogram_length = hist.size();

        Pixie16ReadHistogramFromModule(hist.data(), hist.size(), mod.number, i);
        hists.push_back(hist);
        if (i < static_cast<unsigned int>(mod.number_of_channels - 1))
            out << "Chan" << i << ",";
        else
            out << "Chan" << i;
    }
    out << std::endl;

    for (unsigned int bin = 0; bin < max_histogram_length; bin++) {
        out << bin << ",";
        for (auto& hist : hists) {
            std::string val = " ";
            if (bin < hist.size())
                val = std::to_string(hist[bin]);
            if (&hist != &hists.back())
                out << val << ",";
            else
                out << val;
        }
        out << std::endl;
    }

    if (!output_statistics_data(mod, "mca-stats-run" + std::to_string(run_num))) {
        return false;
    }

    return true;
}

bool execute_mca_runs(const unsigned int num_runs, const module_config& mod,
                      const double runtime_in_seconds, unsigned int synch_wait,
                      unsigned int in_synch) {
    for (unsigned int i = 0; i < num_runs; i++) {
        std::cout << LOG("INFO") << "Starting MCA run number " << i << std::endl;
        if (!execute_mca_run(i, mod, runtime_in_seconds, synch_wait, in_synch)) {
            std::cout << LOG("INFO") << "MCA data run " << i
                      << " failed! See log for more details.";
            return false;
        }
        std::cout << LOG("INFO") << "Finished MCA run number " << i << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    return true;
}

bool execute_parameter_read(args::ValueFlag<std::string>& parameter,
                            args::ValueFlag<unsigned int>& crate, const unsigned int module,
                            args::ValueFlag<unsigned int>& channel) {
    if (channel) {
        double result;
        std::cout << LOG("INFO") << "Pixie16ReadSglChanPar"
                  << " reading " << parameter.Get() << " from Crate " << crate.Get() << " Module "
                  << module << " Channel " << channel.Get() << "." << std::endl;
        if (!verify_api_return_value(
                Pixie16ReadSglChanPar(parameter.Get().c_str(), &result, module, channel.Get()),
                "Pixie16ReadSglChanPar", false))
            return false;
        std::cout << LOG("INFO") << parameter.Get() << "=" << result << std::endl;
    } else {
        unsigned int result;
        std::cout << LOG("INFO") << "Pixie16ReadSglModPar reading " << parameter.Get()
                  << " from Crate " << crate.Get() << " Module " << module << "." << std::endl;
        if (!verify_api_return_value(Pixie16ReadSglModPar(parameter.Get().c_str(), &result, module),
                                     "Pixie16ReadSglModPar", false))
            return false;
        std::cout << LOG("INFO") << parameter.Get() << "=" << result << std::endl;
    }
    return true;
}

bool execute_parameter_write(args::ValueFlag<std::string>& parameter,
                             args::ValueFlag<double>& value, args::ValueFlag<unsigned int>& crate,
                             const module_config& module, args::ValueFlag<unsigned int>& channel) {
    std::cout << LOG("INFO") << "Checking current value for " << parameter.Get() << std::endl;
    execute_parameter_read(parameter, crate, module.number, channel);
    if (channel) {
        std::cout << LOG("INFO") << "Pixie16WriteSglChanPar setting " << parameter.Get() << " to "
                  << value.Get() << " for Crate " << crate.Get() << " Module " << module.number
                  << " Channel " << channel.Get() << "." << std::endl;
        if (!verify_api_return_value(Pixie16WriteSglChanPar(parameter.Get().c_str(), value.Get(),
                                                            module.number, channel.Get()),
                                     "Pixie16WriteSglChanPar"))
            return false;
    } else {
        std::cout << LOG("INFO") << "Pixie16WriteSglModPar"
                  << " setting " << parameter.Get() << " to " << value.Get() << " for  Crate "
                  << crate.Get() << " Module " << module.number << "." << std::endl;
        if (!verify_api_return_value(
                Pixie16WriteSglModPar(parameter.Get().c_str(), value, module.number),
                "Pixie16WriteSglModPar"))
            return false;
    }

    std::cout << LOG("INFO") << "Verifying written value for value for " << parameter.Get()
              << std::endl;
    execute_parameter_read(parameter, crate, module.number, channel);

    if (!save_dsp_pars(module.dsp_par))
        return false;
    return true;
}

bool execute_trace_capture(const module_config& mod) {
    std::cout << LOG("INFO") << "Pixie16AcquireADCTrace acquiring traces for Module " << mod.number
              << "." << std::endl;
    if (!verify_api_return_value(Pixie16AcquireADCTrace(mod.number), "Pixie16AcquireADCTrace"))
        return false;

    std::ofstream ofstream1(generate_filename(mod.number, "adc", "csv"));
    ofstream1 << "bin,";

    unsigned int max_trace_length = 0;
    std::vector<std::vector<unsigned short>> traces;
    for (unsigned int i = 0; i < mod.number_of_channels; i++) {
        std::vector<unsigned short> trace(MAX_ADC_TRACE_LEN, 0);
        if (trace.size() > max_trace_length)
            max_trace_length = trace.size();

        if (!verify_api_return_value(
                Pixie16ReadSglChanADCTrace(trace.data(), trace.size(), mod.number, i),
                "Pixie16AcquireADCTrace", false))
            return false;
        traces.push_back(trace);

        if (i != static_cast<unsigned int>(mod.number_of_channels - 1))
            ofstream1 << "Chan" << i << ",";
        else
            ofstream1 << "Chan" << i;
    }
    ofstream1 << std::endl;

    for (unsigned int idx = 0; idx < max_trace_length; idx++) {
        ofstream1 << idx << ",";
        for (auto& trace : traces) {
            std::string val = " ";
            if (idx < trace.size())
                val = std::to_string(trace[idx]);
            if (&trace != &traces.back())
                ofstream1 << val << ",";
            else
                ofstream1 << val;
        }
        ofstream1 << std::endl;
    }
    return true;
}

bool execute_blcut(args::ValueFlag<unsigned int>& module, args::ValueFlag<unsigned int>& channel) {
    if (!module)
        return false;

    std::cout << LOG("INFO") << "Executing Pixie16BLcutFinder for Module" << module.Get() << "."
              << std::endl;
    unsigned int blcut = 0;
    if (!verify_api_return_value(Pixie16BLcutFinder(module.Get(), channel.Get(), &blcut),
                                 "Pixie16BLcutFinder", false))
        return false;
    std::cout << LOG("INFO") << "BLCut for Module " << module.Get() << " Channel " << channel.Get()
              << " is " << blcut << std::endl;
    return true;
}

bool execute_set_dacs(const module_config& module) {
    std::cout << LOG("INFO") << "Executing Pixie16SetDACs for Module" << module.number << "."
              << std::endl;
    if (!verify_api_return_value(Pixie16SetDACs(module.number), "Pixie16SetDACs", false))
        return false;
    return true;
}

bool execute_close_module_connection(const int& numModules) {
    std::cout << LOG("INFO") << "Closing out connection to Modules." << std::endl;
    verify_api_return_value(Pixie16ExitSystem(numModules), "Pixie16ExitSystem");
    return true;
}

double calculate_duration_in_seconds(const std::chrono::system_clock::time_point& start,
                                     const std::chrono::system_clock::time_point& end) {
    return std::chrono::duration<double>(end - start).count();
}

void output_module_info(configuration& cfg) {
    for (auto& mod : cfg.modules) {
        if (!verify_api_return_value(
                Pixie16ReadModuleInfo(mod.number, &mod.revision, &mod.serial_number,
                                      &mod.adc_bit_resolution, &mod.adc_sampling_frequency),
                "Pixie16ReadModuleInfo", ""))
            throw std::runtime_error("Could not get module information for Module " +
                                     std::to_string(mod.number));
        mod.number_of_channels = NUMBER_OF_CHANNELS;
        std::cout << LOG("INFO") << "Begin module information for Module " << mod.number
                  << std::endl;
        std::cout << LOG("INFO") << "Serial Number: " << mod.serial_number << std::endl;
        std::cout << LOG("INFO") << "Revision: " << mod.revision << std::endl;
        std::cout << LOG("INFO") << "ADC Bits: " << mod.adc_bit_resolution << std::endl;
        std::cout << LOG("INFO") << "ADC MSPS: " << mod.adc_sampling_frequency << std::endl;
        std::cout << LOG("INFO") << "Num Channels: " << mod.number_of_channels << std::endl;
        std::cout << LOG("INFO") << "End module information for Module " << mod.number << std::endl;
    }
}


int main(int argc, char** argv) {
    auto start = std::chrono::system_clock::now();
    args::ArgumentParser parser(
        "Sample code that interfaces with a Pixie system through the User API.");
    parser.LongSeparator("=");

    args::Group commands(parser, "commands");
    args::Command boot(commands, "boot", "Boots the crate of modules.");
    args::Command copy(commands, "copy", "Copies DSP parameters from source to destination.");
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
    args::Command tau_finder(commands, "tau_finder",
                             "Executes the Tau Finder control task and returns the values.");

    args::Group arguments(parser, "arguments", args::Group::Validators::AtLeastOne,
                          args::Options::Global);
    args::ValueFlag<std::string> conf_flag(arguments, "cfg", "The configuration file to load.",
                                           {'c', "config"}, args::Options::Required);
    args::ValueFlag<std::string> additional_cfg_flag(
        arguments, "cfg", "The configuration file to load.", {"additional-config"});
    args::HelpFlag help_flag(arguments, "help", "Displays this message", {'h', "help"});
    args::Flag is_offline(arguments, "Offline Mode",
                          "Tells the API to use Offline mode when running.", {'o', "offline"});
    args::ValueFlag<std::string> boot_pattern_flag(arguments, "boot_pattern",
                                                   "The boot pattern used for booting.",
                                                   {'b', "boot_pattern"}, "0x7F");
    args::ValueFlag<double> run_time(
        list_mode, "time", "The amount of time that a data run will take in seconds.",
        {'t', "run-time"}, 10.);
    args::ValueFlag<std::string> parameter(
        arguments, "parameter", "The parameter we want to read from the system.", {'n', "name"});
    args::ValueFlag<unsigned int> channel(
        arguments, "channel",
        "The channel to operate on. If set to the maximum number of channels in the module, then reads/writes execute for all channels.",
        {"chan"});
    args::ValueFlag<unsigned int> crate(arguments, "crate", "The crate", {"crate"}, 0);
    args::ValueFlag<unsigned int> copy_mask(
        copy, "copy_mask", "An integer representing the set of parameters to copy", {"copy-mask"});
    args::ValueFlag<unsigned int> dest_mask(
        copy, "dest_mask", "An integer representing the destination channels", {"dest-mask"});
    args::ValueFlag<unsigned int> dest_channel(copy, "dest_channel",
                                               "The channel that we'll copy to", {"dest-chan"});
    args::ValueFlag<unsigned int> dest_module(copy, "dest_module", "The module that we'll copy to.",
                                              {"dest-mod"});
    args::ValueFlag<unsigned int> module(arguments, "module", "The module to operate on.", {"mod"});
    args::ValueFlag<unsigned int> num_runs(
        arguments, "num_runs", "The number of runs to execute when taking list-mode or MCA data.",
        {"num-runs"}, static_cast<unsigned int>(1));
    args::ValueFlag<double> parameter_value(
        write, "parameter_value", "The value of the parameter we want to write.", {'v', "value"});
    args::ValueFlag<unsigned int> synch_wait(
        list_mode, "synch_wait",
        "SynchWait = 0 to start/stop runs independently. (default)\nSynchWait = 1 to start/stop runs synchronously.",
        {"synch-wait"}, static_cast<unsigned int>(0));
    args::ValueFlag<unsigned int> in_synch(
        list_mode, "in_synch",
        "InSynch = 0 to reset clocks prior to starting a run. (default)\nInSynch = 1 to take no clock action.",
        {"in-synch"}, static_cast<unsigned int>(0));

    adjust_offsets.Add(conf_flag);
    adjust_offsets.Add(boot_pattern_flag);
    baseline.Add(boot_pattern_flag);
    blcut.Add(module);
    blcut.Add(channel);
    boot.Add(conf_flag);
    boot.Add(boot_pattern_flag);
    copy.Add(boot_pattern_flag);
    copy.Add(module);
    copy.Add(channel);
    dacs.Add(module);
    list_mode.Add(num_runs);
    mca.Add(module);
    mca.Add(boot_pattern_flag);
    mca.Add(synch_wait);
    mca.Add(in_synch);
    mca.Add(num_runs);
    mca.Add(run_time);
    read.Add(conf_flag);
    read.Add(crate);
    read.Add(module);
    read.Add(channel);
    read.Add(parameter);
    tau_finder.Add(module);
    trace.Add(conf_flag);
    trace.Add(boot_pattern_flag);
    write.Add(conf_flag);
    write.Add(parameter);
    write.Add(crate);
    write.Add(module);
    write.Add(channel);

    try {
        parser.ParseCLI(argc, argv);
    } catch (args::Help& help) {
        std::cout << LOG("INFO") << help.what() << std::endl;
        std::cout << parser;
        return EXIT_SUCCESS;
    } catch (args::Error& e) {
        std::cout << LOG("ERROR") << e.what() << std::endl;
        std::cout << parser;
        return EXIT_FAILURE;
    }

    configuration cfg;
    try {
        read_config(conf_flag.Get(), cfg);
    } catch (std::exception& e) {
        std::cout << LOG("ERROR") << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << LOG("INFO") << "Finished reading config in "
              << calculate_duration_in_seconds(start, std::chrono::system_clock::now()) << " s."
              << std::endl;

    int offline_mode = 0;
    if (is_offline)
        offline_mode = 1;

    start = std::chrono::system_clock::now();
    std::cout << LOG("INFO") << "Calling Pixie16InitSystem." << std::endl;
    if (!verify_api_return_value(
            Pixie16InitSystem(cfg.num_modules(), cfg.slot_def.data(), offline_mode),
            "Pixie16InitSystem", false))
        return EXIT_FAILURE;

    std::cout << LOG("INFO") << "Finished Pixie16InitSystem in "
              << calculate_duration_in_seconds(start, std::chrono::system_clock::now()) << " s."
              << std::endl;

    try {
        output_module_info(cfg);
    } catch (std::runtime_error& error) {
        std::cout << LOG("ERROR") << error.what() << std::endl;
        return EXIT_FAILURE;
    }

    if (init) {
        execute_close_module_connection(cfg.num_modules());
        return EXIT_SUCCESS;
    }

    unsigned int boot_pattern = stoul(args::get(boot_pattern_flag), nullptr, 0);
    if (additional_cfg_flag)
        boot_pattern = 0x70;

    for (auto& mod : cfg.modules) {
            start = std::chrono::system_clock::now();
            std::cout << LOG("INFO") << "Calling Pixie16BootModule for Module " << mod.number
                      << " with boot pattern: " << std::showbase << std::hex << boot_pattern
                      << std::dec << std::endl;

            if (!verify_api_return_value(
                    Pixie16BootModule(mod.com_fpga_config.c_str(), mod.sp_fpga_config.c_str(),
                                      nullptr, mod.dsp_code.c_str(), mod.dsp_par.c_str(),
                                      mod.dsp_var.c_str(), mod.number, boot_pattern),
                    "Pixie16BootModule", "Finished booting!"))
                return EXIT_FAILURE;
            std::cout << LOG("INFO") << "Finished Pixie16BootModule for Module " << mod.number
                      << " in "
                      << calculate_duration_in_seconds(start, std::chrono::system_clock::now())
                      << " s." << std::endl;
    }

    if (boot) {
        execute_close_module_connection(cfg.num_modules());
        return EXIT_SUCCESS;
    }

    if (additional_cfg_flag) {
        if (!verify_api_return_value(
                Pixie16LoadDSPParametersFromFile(additional_cfg_flag.Get().c_str()),
                "Pixie16LoadDSPParametersFromFile", true))
            return EXIT_FAILURE;
    }

    if (copy) {
        if (!module || !channel || !copy_mask || !dest_channel || !dest_module) {
            std::cout
                << LOG("ERROR")
                << "Pixie16CopyDSPParameters requires the source/destination module and channel "
                   "and the destination mask to execute!"
                << std::endl;
        }
        std::vector<unsigned short> dest_masks;
        for (size_t mod = 0; mod < cfg.num_modules(); mod++) {
            for (size_t chan = 0; chan < cfg.modules[mod].number_of_channels; chan++) {
                if (mod == dest_module.Get() && chan == dest_channel.Get())
                    dest_masks.push_back(1);
                else
                    dest_masks.push_back(0);
            }
        }
        if (!verify_api_return_value(Pixie16CopyDSPParameters(copy_mask.Get(), module.Get(),
                                                              channel.Get(), dest_masks.data()),
                                     "Pixie16CopyDSPParameters", true)) {
            return EXIT_FAILURE;
        }
    }

    if (tau_finder) {
        if (!module) {
            std::cout << LOG("ERROR") << "Pixie16TauFinder requires the module flag to execute!"
                      << std::endl;
        }

        std::vector<double> taus(cfg.modules[module.Get()].number_of_channels);
        if (!verify_api_return_value(Pixie16TauFinder(module.Get(), taus.data()),
                                     "Pixie16TauFinder", true)) {
            return EXIT_FAILURE;
        }
        for (unsigned int i = 0; i < taus.size(); i++) {
            std::cout << "Channel " << i << ": " << taus.at(i) << std::endl;
        }
    }

    if (read) {
        auto mod = cfg.modules[module.Get()];
        if (channel.Get() >= mod.number_of_channels) {
            for (unsigned int ch = 0; ch < mod.number_of_channels; ch++) {
                channel.ParseValue(std::vector<std::string>(1, std::to_string(ch)));
                if (!execute_parameter_read(parameter, crate, module.Get(), channel))
                    return EXIT_FAILURE;
            }
        } else {
            if (!execute_parameter_read(parameter, crate, module.Get(), channel))
                return EXIT_FAILURE;
        }
    }

    if (write) {
        auto mod = cfg.modules[module.Get()];
        if (channel.Get() >= mod.number_of_channels) {
            for (unsigned int ch = 0; ch < mod.number_of_channels; ch++) {
                channel.ParseValue(std::vector<std::string>(1, std::to_string(ch)));
                if (!execute_parameter_write(parameter, parameter_value, crate, mod, channel))
                    return EXIT_FAILURE;
            }
        } else {
            if (!execute_parameter_write(parameter, parameter_value, crate, mod, channel))
                return EXIT_FAILURE;
        }
    }

    if (adjust_offsets) {
        for (auto& mod : cfg.modules)
            if (!execute_adjust_offsets(mod))
                return EXIT_FAILURE;
    }

    if (trace) {
        for (auto& mod : cfg.modules)
            if (!execute_trace_capture(mod))
                return EXIT_FAILURE;
    }

    if (list_mode) {
        if (!execute_list_mode_runs(num_runs.Get(), cfg, run_time.Get(), synch_wait.Get(),
                                    in_synch.Get()))
            return EXIT_FAILURE;
    }

    if (export_settings) {
        if (!save_dsp_pars(cfg.modules.front().dsp_par))
            return EXIT_FAILURE;
    }

    if (baseline) {
        for (auto& mod : cfg.modules)
            if (!execute_baseline_capture(mod))
                return EXIT_FAILURE;
    }

    if (mca) {
        if (module.Get() >= cfg.num_modules()) {
            for (auto& mod : cfg.modules) {
                if (!execute_mca_runs(num_runs.Get(), mod, run_time.Get(), synch_wait.Get(),
                                      in_synch.Get()))
                    return EXIT_FAILURE;
            }
        } else {
            if (!execute_mca_runs(num_runs.Get(), cfg.modules[module.Get()], run_time.Get(),
                                  synch_wait.Get(), in_synch.Get()))
                return EXIT_FAILURE;
        }
    }

    if (blcut) {
        if (!execute_blcut(module, channel))
            return EXIT_FAILURE;
    }

    if (dacs) {
        for (auto& mod : cfg.modules)
            if (!execute_set_dacs(mod))
                return EXIT_FAILURE;
    }

    execute_close_module_connection(cfg.num_modules());
    return EXIT_SUCCESS;
}
