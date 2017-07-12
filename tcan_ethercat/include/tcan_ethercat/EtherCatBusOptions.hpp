/*
 * EtherCatBusOptions.hpp
 *
 *  Created on: Mar 24, 2017
 *      Author: Remo Diethelm
 */

#pragma once

#include <stdint.h>

#include "tcan/BusOptions.hpp"

namespace tcan_ethercat {

struct EtherCatBusOptions : public tcan::BusOptions {

    EtherCatBusOptions(const std::string& name = "", unsigned int maxDeviceTimeoutCounter = 20):
        BusOptions(name),
        maxDeviceTimeoutCounter_(maxDeviceTimeoutCounter) {}

    virtual ~EtherCatBusOptions() {}

    unsigned int maxDeviceTimeoutCounter_;
};

} /* namespace tcan_ethercat */