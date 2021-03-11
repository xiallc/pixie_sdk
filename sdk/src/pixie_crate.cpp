/*----------------------------------------------------------------------
* Copyright (c) 2005 - 2020, XIA LLC
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

#include <algorithm>
#include <sstream>

#include <pixie_crate.hpp>

namespace xia
{
namespace pixie
{
namespace crate
{
    error::error(const std::string& what)
        : runtime_error(what) {
    }

    error::error(const char* what)
        : runtime_error(what) {
    }

    crate::crate(size_t num_modules_)
        : num_modules(num_modules_)
    {
        if (num_modules != 0)
            throw error("crate already initialised");
    }

    crate::~crate()
    {
    }

    void
    crate::initialize()
    {
        for (size_t device_number = 0;
             device_number < num_modules;
             ++device_number) {

            modules.push_back(module::module());

            module::module& module = modules.back();

            if (!module::pci_find_module(device_number, module.device))
                break;

            /*
             * I am not sure this extra scan is needed.
             */
            auto mi = std::find_if(modules.begin(),
                                   modules.end(),
                                   [&module](const module::module& m) {
                                       return (module::pci_bus(m.device) ==
                                               module::pci_bus(module.device) &&
                                               module::pci_slot(m.device) ==
                                               module::pci_slot(module.device));
                                   });
            if (mi != modules.end()) {
                std::ostringstream oss;
                oss << "duplicate Pixie16 module found (found " << modules.size()
                    << " of " << num_modules << ')';
                throw error(oss.str());
            }

        }

        if (modules.size() != num_modules) {
            std::ostringstream oss;
            oss << "Pixie16 module(s) not found (found " << modules.size()
                << " of " << num_modules << ')';
            throw error(oss.str());
        }
    }
};
};
};
