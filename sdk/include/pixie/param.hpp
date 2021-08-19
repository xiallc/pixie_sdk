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

/** @file param.hpp
 * @brief Defines functions and data structures related to handling parameter sets
 */

#ifndef PIXIE_PARAM_H
#define PIXIE_PARAM_H

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <pixie/os_compat.hpp>

#include <pixie/pixie16/hw.hpp>

namespace xia {
namespace pixie {
namespace firmware {
struct firmware;
typedef std::shared_ptr<firmware> firmware_ref;
}  // namespace firmware
/**
 * @brief Tools for working with parameters and variables in the SDK.
 */
namespace param {
/**
 * @brief Defines System parameters that we use in various locations.
 *
 * Do not force any values onto the enum.
 */
enum struct system_param {
    number_modules,
    offline_analysis,
    pxi_slot_map,
    /*
     * Size marker
     */
    END
};

/**
 * @brief Defines user facing module parameters.
 *
 * Do not force any values onto the enum.
 */
enum struct module_param {
    module_number,
    module_csra,
    module_csrb,
    module_format,
    max_events,
    synch_wait,
    in_synch,
    slow_filter_range,
    fast_filter_range,
    fasttrigbackplaneena,
    crateid,
    slotid,
    modid,
    trigconfig0,
    trigconfig1,
    trigconfig2,
    trigconfig3,
    host_rt_preset,
    /*
     * Size marker
     */
    END
};

/**
 * @brief Define user-facing Channel parameters.
 *
 * Do not force any values onto the enum.
 */
enum struct channel_param {
    trigger_risetime,
    trigger_flattop,
    trigger_threshold,
    energy_risetime,
    energy_flattop,
    tau,
    trace_length,
    trace_delay,
    voffset,
    xdt,
    baseline_percent,
    emin,
    binfactor,
    baseline_average,
    channel_csra,
    channel_csrb,
    blcut,
    integrator,
    fasttrigbacklen,
    cfddelay,
    cfdscale,
    cfdthresh,
    qdclen0,
    qdclen1,
    qdclen2,
    qdclen3,
    qdclen4,
    qdclen5,
    qdclen6,
    qdclen7,
    exttrigstretch,
    vetostretch,
    multiplicitymaskl,
    multiplicitymaskh,
    externdelaylen,
    ftrigoutdelay,
    chantrigstretch,
    /*
     * Size marker
     */
    END
};

/**
 * @brief Module variables that are defined within the DSP VAR file
 *
 * Do not force any values onto the enum.
 */
enum struct module_var {
    /*
     * In
     */
    ModNum,
    ModCSRA,
    ModCSRB,
    ModFormat,
    RunTask,
    ControlTask,
    MaxEvents,
    CoincPattern,
    CoincWait,
    SynchWait,
    InSynch,
    Resume,
    SlowFilterRange,
    FastFilterRange,
    ChanNum,
    HostIO,
    UserIn,
    FastTrigBackplaneEna,
    CrateID,
    SlotID,
    ModID,
    TrigConfig,
    HostRunTimePreset,
    PowerUpInitDone,
    U00,
    /*
     * Out
     */
    RealTimeA,
    RealTimeB,
    RunTimeA,
    RunTimeB,
    GSLTtime,
    DSPerror,
    SynchDone,
    UserOut,
    AOutBuffer,
    AECorr,
    LECorr,
    HardwareID,
    HardVariant,
    FIFOLength,
    DSPrelease,
    DSPbuild,
    NumEventsA,
    NumEventsB,
    BufHeadLen,
    EventHeadLen,
    ChanHeadLen,
    LOutBuffer,
    FippiID,
    FippiVariant,
    DSPVariant,
    U20,
    /*
     * Size marker
     */
    END
};

/**
 * @brief Defines Channel variables that are defined within the DSP VAR file.
 *
 * Do not force any values onto the enum.
 */
enum struct channel_var {
    /*
     * In
     */
    ChanCSRa,
    ChanCSRb,
    GainDAC,
    OffsetDAC,
    DigGain,
    SlowLength,
    SlowGap,
    FastLength,
    FastGap,
    PeakSample,
    PeakSep,
    CFDThresh,
    FastThresh,
    ThreshWidth,
    PAFlength,
    TriggerDelay,
    ResetDelay,
    ChanTrigStretch,
    TraceLength,
    Xwait,
    TrigOutLen,
    EnergyLow,
    Log2Ebin,
    MultiplicityMaskL,
    MultiplicityMaskH,
    PSAoffset,
    PSAlength,
    Integrator,
    BLcut,
    BaselinePercent,
    FtrigoutDelay,
    Log2Bweight,
    PreampTau,
    Xavg,
    FastTrigBackLen,
    CFDDelay,
    CFDScale,
    ExternDelayLen,
    ExtTrigStretch,
    VetoStretch,
    QDCLen0,
    QDCLen1,
    QDCLen2,
    QDCLen3,
    QDCLen4,
    QDCLen5,
    QDCLen6,
    QDCLen7,
    /*
     * Out
     */
    LiveTimeA,
    LiveTimeB,
    FastPeaksA,
    FastPeaksB,
    OverflowA,
    OverflowB,
    InSpecA,
    InSpecB,
    UnderflowA,
    UnderflowB,
    ChanEventsA,
    ChanEventsB,
    AutoTau,
    U30,
    /*
     * Size marker
     */
    END
};

/**
 * @brief defines the Variable's input/output mode.
 */
enum rwrowr { rw, ro, wr };

/**
 * @brief Variable enabled or disabled
 */
enum enabledisable { enable, disable };

/**
 * @brief Variable addressing defines what chip this variable is associated with.
 */
enum addressing { dsp_reg, fpga_reg, composite };

/*
 * Value type.
 */
typedef uint32_t value_type;

/*
 * Values.
 */
typedef std::vector<value_type> values;

/**
 * @brief A data structure describing information about Parameters.
 */
template<typename P>
struct parameter_desc {
    const P par; /*!< Parameter (index) */
    const rwrowr mode; /*!< In/out of the variable */
    const size_t size; /*!< Number of DSP words it covers */
    enabledisable state; /*!< Variable's state */
    const std::string name; /*!< Name of the variable */
    parameter_desc(const P par_, enabledisable state_, const rwrowr mode_, const size_t size_,
                   const std::string name_)
        : par(par_), mode(mode_), size(size_), state(state_), name(name_) {}
    bool writeable() const {
        return state == enable && mode != ro;
    }
};

/**
 * @brief A data structure describing information about a Variable.
 */
template<typename V>
struct variable_desc : public parameter_desc<V> {
    hw::address address; /*!< DSP memory address */
    variable_desc(const V var_, enabledisable state_, const rwrowr mode_, const size_t size_,
                  const std::string name_)
        : parameter_desc<V>(var_, state_, mode_, size_, name_), address(0) {}
};

/*
 * Parameter descriptor sets for the system, modules and channels
 */
typedef parameter_desc<system_param> system_param_desc;
typedef std::vector<system_param_desc> system_param_descs;
typedef parameter_desc<module_param> module_param_desc;
typedef std::vector<module_param_desc> module_param_descs;
typedef parameter_desc<channel_param> channel_param_desc;
typedef std::vector<channel_param_desc> channel_param_descs;

/*
 * Variable descriptor sets for modules and channels
 */
typedef variable_desc<module_var> module_var_desc;
typedef std::vector<module_var_desc> module_var_descs;
typedef variable_desc<channel_var> channel_var_desc;
typedef std::vector<channel_var_desc> channel_var_descs;

/**
 * @brief A variable is an object that combines descriptors with values.
 */
template<typename Vdesc>
struct variable {
    /**
     * @brief Structure to describe the data associated with a descriptor.
     */
    struct data {
        bool dirty; /*!< Written to hardware? */
        value_type value;
        data() : dirty(false), value(0) {}
    };
    const Vdesc& var; /*!< The variable descriptor */
    std::vector<data> value; /*!< The value(s) */

    variable(const Vdesc& var_) : var(var_) {
        value.resize(var.size);
    }
};

/*
 * Module variables
 */
typedef variable<module_var_desc> module_variable;
typedef std::vector<module_variable> module_variables;

/*
 * Channel variables
 */
typedef variable<channel_var_desc> channel_variable;
typedef std::vector<channel_variable> channel_variables;
typedef std::vector<channel_variables> channels_variables;

/**
 * @brief Copies filter variables from one channel to another. Only used with channel objects.
 */
template<typename V>
struct copy_filter_var {
    V var;
    uint32_t mask;

    copy_filter_var(V var_, uint32_t mask_ = UINT32_MAX) : var(var_), mask(mask_) {}
};

typedef std::vector<copy_filter_var<channel_var>> copy_filter;

/*
 * Copy filter masks. The masks select the filters used in a copy.
 */
const unsigned int energy_mask = 1 << 0;
const unsigned int trigger_mask = 1 << 1;
const unsigned int analog_signal_cond_mask = 1 << 2;
const unsigned int histogram_control_mask = 1 << 3;
const unsigned int decay_time_mask = 1 << 4;
const unsigned int pulse_shape_analysis_mask = 1 << 5;
const unsigned int baseline_control_mask = 1 << 6;
const unsigned int channel_csra_mask = 1 << 7;
const unsigned int cfd_trigger_mask = 1 << 8;
const unsigned int trigger_stretch_len_mask = 1 << 9;
const unsigned int fifo_delays_mask = 1 << 10;
const unsigned int multiplicity_mask = 1 << 11;
const unsigned int qdc_mask = 1 << 12;
const unsigned int all_mask = (1 << 12) - 1;

/*
 * Look up maps. A fast way to map a name to a parameter or variable.
 */
typedef std::map<std::string, system_param> system_param_map;
typedef std::map<std::string, module_param> module_param_map;
typedef std::map<std::string, channel_param> channel_param_map;
typedef std::map<std::string, module_var> module_var_map;
typedef std::map<std::string, channel_var> channel_var_map;

/**
 * @brief Defines an Address map that can be used to parse binary data blobs.
 *
 * This structure is typically used to parse Statistics or configuration data from the DSP.
 */
struct address_map {
    typedef std::pair<size_t, hw::address> desc_address;
    typedef std::vector<desc_address> desc_addresses;
    /**
     * @brief Data structure for working with address ranges
     */
    struct range {
        hw::address start;
        hw::address end;
        size_t size;
        range();
        void set_size();
        void output(std::ostream& out) const;
    };

    range full;

    range module;
    range module_in;
    range module_out;

    range channels;
    range channels_in;
    range channels_out;

    size_t vars;
    size_t module_vars;
    size_t channel_vars;
    size_t vars_per_channel;

    address_map();

    void set(const size_t num_channels, const module_var_descs& module_descs,
             const channel_var_descs& channel_descs);
    hw::address channel_base(const size_t channel);

    void output(std::ostream& out) const;

private:
    void check_channel_gap(const size_t max_channels, const channel_var_descs& channel_descs,
                           const desc_addresses& addresses);

    template<typename V>
    void get(const V& vars, desc_addresses& addresses, const rwrowr mode);
    hw::address min(const desc_addresses& addresses);
    hw::address max(const desc_addresses& addresses);
};

/**
 * @brief Get a descriptor from the descriptors by its variable name.
 */
template<typename D, typename V>
const typename D::value_type& get_descriptor(const D& descs, const V var) {
    return descs[size_t(var)];
}

/*
 * Get a copy of the defaults.
 */
const module_var_descs& get_module_var_descriptors();
const channel_var_descs& get_channel_var_descriptors();

/*
 * Get the maps
 */
const system_param_map get_system_param_map();
const module_param_map get_module_param_map();
const channel_param_map get_channel_param_map();

/*
 * Check is the parameter or variable is valid?
 */
bool is_system_param(const std::string& label);
bool is_module_param(const std::string& label);
bool is_channel_param(const std::string& label);
bool is_module_var(const std::string& label);
bool is_channel_var(const std::string& label);

/*
 * Look up parameters and variables.
 */
system_param lookup_system_param(const std::string& label);
module_param lookup_module_param(const std::string& label);
channel_param lookup_channel_param(const std::string& label);
module_var lookup_module_var(const std::string& label);
channel_var lookup_channel_var(const std::string& label);

/**
 * @brief Maps a Module parameter to module variable.
 */
module_var map_module_param(module_param par);

/*
 * Load the variables from the DSP variable file into the
 * descriptors.
 */
void load(const std::string& dspvarfile, module_var_descs& module_var_descriptors,
          channel_var_descs& channel_var_descriptors);
void load(firmware::firmware_ref& dspvarfw, module_var_descs& module_var_descriptors,
          channel_var_descs& channel_var_descriptors);
void load(std::istream& input, module_var_descs& module_var_descriptors,
          channel_var_descs& channel_var_descriptors);

/**
 * @brief Copy the variables based on the filter.
 */
void copy_parameters(const copy_filter& filter, const channel_variables& source,
                     channel_variables& dest);

/**
 * @brief Copy the variables based on the filter mask.
 */
void copy_parameters(const unsigned int filter_mask, const channel_variables& source,
                     channel_variables& dest);

};  // namespace param
};  // namespace pixie
};  // namespace xia

std::ostream& operator<<(std::ostream& out, const xia::pixie::param::address_map& config);

#endif  // PIXIE_PARAM_H
