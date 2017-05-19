/*
 * BusManager.hpp
 *
 *  Created on: Mar 27, 2016
 *      Author: Philipp Leemann
 */

#pragma once

#include <vector>

#include "tcan/Bus.hpp"
#include "tcan/helper_functions.hpp"

namespace tcan {

//! Container of all CAN buses
template <class Msg>
class BusManager {
 public:
    BusManager():
        buses_(),
        receiveThread_(),
        sanityCheckThread_(),
        running_(false),
        sanityCheckInterval_(100)
    {
    }

    virtual ~BusManager()
    {
        closeBuses();
    }

    bool addBus(Bus<Msg>* bus) {
        if(bus->isSemiSynchronous() && running_) {
            MELO_FATAL("Tried to add a semi-synchronous bus after calling startThreads. This is not allowed due to data concurrency!");
        }

        buses_.push_back( bus );
        return bus->initBus();
    }
    /*! Gets the number of buses
     * @return	number of buses
     */
    unsigned int getSize() const { return buses_.size(); }

    /*! Read and parse messages from all buses. Call this function in the control loop if synchronous mode is used.
     */
    void readMessagesSynchronous() {
        for(auto bus : buses_) {
            if(bus->isSynchronous()) {
                while(bus->readMessage()) {
                }
            }
        }
    }
    /*!
     * Send the messages in the output queue on all buses. Call this function in the control loop if synchronous mode is used.
     * Note that this function may not send all the messages in the output queue if BlockingWrite is disabled (see BusOptions)
     * @return  False if at least one write error occured
     */
    bool writeMessagesSynchronous() {
        bool sendingData = true;
        bool noError = true;
        while(sendingData) {
            sendingData = false;

            for(auto bus : buses_) {
                if(!bus->isAsynchronous() && bus->getNumOutogingMessagesWithoutLock() > 0) {
                    noError &= bus->writeMessagesWithoutLock();
                    sendingData = true;
                }
            }
        }
        return noError;
    }

    /*! Call sanityCheck(..) on all buses. Call this function in the control loop if synchronous mode is used.
     */
    bool sanityCheckSynchronous() {
        bool allFine = true;
        for(auto bus : buses_) {
            if(bus->isSynchronous()) {
                bus->sanityCheck();
                allFine &= bus->allDevicesActive();
            }
        }

        return allFine;
    }

    /*!
     * Check if no device timed out
     * @return  True if at least one device is missing
     */
    bool isMissingDeviceOrHasError() const {
        for(auto bus : buses_) {
            if(bus->isMissingDeviceOrHasError()) {
                return true;
            }
        }
        return false;
    }

    /*!
     * check if we received a message from all devices within timeout
     * @return True if all devices are active
     */
    bool allDevicesActive() const {
        for(auto bus : buses_) {
            if(!(bus->allDevicesActive())) {
                return false;
            }
        }
        return true;
    }

    void closeBuses() {
        // tell all threads to stop
        stopThreads(false);
        for(Bus<Msg>* bus : buses_) {
            bus->stopThreads(false);
        }

        // join all threads and destruct buses
        stopThreads(true);
        for(Bus<Msg>* bus : buses_) {
            delete bus;
        }

        buses_.clear();
    }

    void startThreads() {
        if(running_) {
            return;
        }

        bool hasSemiSyncBus = false;
        int priorityReceiveThread = 0;
        int prioritySanityCheckThread = 0;
        sanityCheckInterval_ = 0;

        std::once_flag flag;

        for(auto bus : buses_) {
            if(bus->isSemiSynchronous()) {
                const BusOptions *options = bus->getOptions();

                priorityReceiveThread = std::max(priorityReceiveThread, options->priorityReceiveThread_);
                prioritySanityCheckThread = std::max(prioritySanityCheckThread, options->prioritySanityCheckThread_);

                std::call_once(flag, [&](){ sanityCheckInterval_ = options->sanityCheckInterval_; });

                if (sanityCheckInterval_ < options->sanityCheckInterval_) {
                    sanityCheckInterval_ = options->sanityCheckInterval_;
                    MELO_WARN("Raising sanity check interval for bus manager to %d", sanityCheckInterval_);
                } else if (sanityCheckInterval_ > options->sanityCheckInterval_) {
                    MELO_WARN("Bus manager sanity check interval (%d) is larger than sanity check interval of added bus %s (%d)",
                              sanityCheckInterval_, options->name_.c_str(), options->sanityCheckInterval_);
                }

                hasSemiSyncBus = true;
            }
        }

        if(hasSemiSyncBus) {
            running_ = true;

            receiveThread_ = std::thread(&BusManager::receiveWorker, this);

            if (!setThreadPriority(receiveThread_, priorityReceiveThread)) {
                MELO_WARN("Failed to set receive thread priority for bus manager\n  %s", strerror(errno));
            }

            if (sanityCheckInterval_ > 0) {
                sanityCheckThread_ = std::thread(&BusManager::sanityCheckWorker, this);

                if (!setThreadPriority(sanityCheckThread_, prioritySanityCheckThread)) {
                    MELO_WARN("Failed to set sanity check thread priority for bus manager\n  %s", strerror(errno));
                }
            }
        }else{
            MELO_INFO("No Bus is configured to be semi synchrounous. Not generating threads.");
        }
    }

    void stopThreads(const bool wait=true) {
        running_ = false;

        if(wait) {
            if(receiveThread_.joinable()) {
                receiveThread_.join();
            }

            if(sanityCheckThread_.joinable()) {
                sanityCheckThread_.join();
            }
        }
    }

 protected:

    // thread loop functions
    void receiveWorker() {
        while(running_) {
            sleep(1);
        }

        MELO_INFO("Receive thread for bus manager terminated");
    }

    void sanityCheckWorker() {
        auto nextLoop = std::chrono::steady_clock::now();

        while(running_) {
            nextLoop += std::chrono::milliseconds(sanityCheckInterval_);
            std::this_thread::sleep_until(nextLoop);

            for(auto bus : buses_) {
                if(bus->isSemiSynchronous()) {
                    bus->sanityCheck();
                }
            }
        }

        MELO_INFO("SanityCheck thread for bus manager terminated");
    }

 protected:
    std::vector<Bus<Msg>*> buses_;

    //! threads for message reception and transmission and device sanity checking
    std::thread receiveThread_;
    std::thread sanityCheckThread_;
    std::atomic<bool> running_;

    unsigned int sanityCheckInterval_;
};

} /* namespace tcan */
