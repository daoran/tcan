/*
 * PcanBus.hpp
 *
 *  Created on: Mar 27, 2016
 *      Author: Philipp Leemann
 */

#pragma once

#include "tcan_can/CanBus.hpp"
#include "tcan_pcanfd/PcanfdBusOptions.hpp"

#include <string>
#include <memory>


namespace tcan_pcanfd {

class PcanfdBus : public tcan_can::CanBus {
 public:

    PcanfdBus(const std::string& interface);
    PcanfdBus(std::unique_ptr<PcanfdBusOptions>&& options);

    virtual ~PcanfdBus();

    int getPollableFileDescriptor() {
        // todo: check if this is actually pollable!
        return fd_;
    }

 protected:
    bool initializeInterface();
    bool readData();
    bool writeData(std::unique_lock<std::mutex>* lock);


 protected:
    int fd_;
};

} /* namespace tcan_pcanfd */
