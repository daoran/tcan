/*
 * Bus.hpp
 *
 *  Created on: Mar 27, 2016
 *      Author: Philipp Leemann
 */

#pragma once

#include <stdint.h>
#include <unordered_map>
#include <memory>
#include <functional>
#include <vector>
#include <algorithm> // copy(..)


#include "tcan/Bus.hpp"
#include "tcan/EtherCatBusOptions.hpp"
#include "tcan/GenericMsg.hpp"
//#include "tcan/CanDevice.hpp"

namespace tcan {

using EthernetFrame = GenericMsg;

class Datagram {
 public:
    inline void resize(const uint16_t length) {
        uint8_t* oldData = data_;
        data_ = new uint8_t[length];
        std::copy(&oldData[0], &oldData[header_.len_], data_);
        header_.len_ = length;
        delete[] oldData;
    }

    template <typename T>
    inline void write(const uint16_t memoryPosition, const T& data) {
        // check if memoryPosition lies within data_?
        std::copy(&data_[memoryPosition], &data_[memoryPosition + sizeof(T)], &data);
    }

    inline const uint16_t getLength() const { return header_.len_; }
    inline const uint8_t* getData() const { return data_; }
    inline const uint16_t getWorkingCounter() const { return workingCounter_; }

 private:

    struct DatagramHeader {
        enum class Command : uint8_t {
            NOP=0, // No operation
            APRD=1, // Auto Increment Read
            APWR=2, // Auto Increment Write
            APRW=3, // Auto Increment Read Write
            FPRD=4, // Configured Address Read
            FPWR=5, // Configured Address Write
            FPRW=6, // Configured Address Read Write
            BRD=7, // Broadcast Read
            BWR=8, // Broadcast Write
            BRW=9, // Broadcast Read Write
            LRD=10, // Logical Memory Read
            LWR=11, // Logical Memory Write
            LRW=12, // Logical Memory Read Write
            ARMW=13, // Auto Increment Read Multiple Write
            FRMW=14 // Configured Read Multiple Write
        };

        Command cmd_;
        uint8_t idx_;
        uint32_t address_;
        uint16_t len_           :11; // does this sub-byte work?
        uint16_t reserved_      :3;
        uint16_t circulating_   :1;
        uint16_t more_          :1;
        uint16_t irq_;

        DatagramHeader(): cmd_(Command::NOP), idx_(0), address_(0), len_(0), reserved_(0), circulating_(0), more_(0), irq_(0) { }
    } __attribute__((packed)); // prevent structure padding

    DatagramHeader header_;
    uint8_t* data_;
    uint16_t workingCounter_;

    // have a map of (name string => memory address) to have human-readable access to the fields in data_? would need to update this on resize(..) calls..
};

class EtherCatBus : public Bus<EthernetFrame> {
 public:
    typedef std::function<bool(const CanMsg&)> CallbackPtr;
    typedef std::unordered_map<uint32_t, std::pair<CanDevice*, CallbackPtr>> AddressToFunctionMap;

    EtherCatBus() = delete;
    EtherCatBus(EtherCatBusOptions* options);

    virtual ~EtherCatBus();

    /*!
     * in-place construction of a new device
     * @param options   pointer to the option class of the device
     * @return true if successful
     */
    template <class C, typename TOptions>
    inline std::pair<C*, bool> addDevice(TOptions* options) {
        C* dev = new C(options);
        bool success = addDevice(dev);
        return std::make_pair(dev, success);
    }

    /*! Adds a device to the device vector and calls its initDevice function
     * @param device    Pointer to the device
     * @return true if init was successful
     */
    inline bool addDevice(CanDevice* device) {
        // asssign the device some id to calculate the offset in ethernet frame address
        devices_.push_back(device);
        return device->initDeviceInternal(this);
    }

    template <class T>
    inline std::shared_ptr<Datagram> addReadDatagram(Datagram&& datagram, T* device, bool(std::common_type<T>::type::*fp)()) {
        // add a datagram linked with a callback to addressToFunctionMap_, use datagram address as key
        return addDatagram(std::forward<Datagram>(datagram));
    }

    inline std::shared_ptr<Datagram> addWriteDatagram(Datagram&& datagram) {
        return addDatagram(std::forward<Datagram>(datagram));
    }

    /*!
     * Should be called from the same thread as write operations on the datagrams, otherwise it is up to the user to ensure thread safety
     * @return  true on success
     */
    inline void dispatchFrame() {
        unsigned int len = 0;
        for(const auto& datagramPtr : datagrams_) {
            len += datagramPtr->getLength();
        }

        uint8_t* data = new uint8_t[len];
        for(const auto& datagramPtr : datagrams_) {
            // copy members from datagramPtr to data
        }

        // compute expected working counter

        // other option:
        EthernetFrame msg;
        msg.emplaceData(len, data);
        sendMessage(msg, true);
    }

    void discoverDevices() {
        // do not add the devices by hand but get the topology and memory from the connected devices?
    }

 public:/// INTERNAL FUNCTIONS
    /*! Is called after reception of a message. Routes the message to the callback.
     * @param cmsg	reference to the can message
     */
    void handleMessage(const EthernetFrame& msg) {
        // check time the frame took to travel through the physical bus
        // check working counters
        // extract datagrams from ethernet frame and map datagrams to device callback
    }

    /*! Initialized the device driver
     * @return true if successful
     */
    virtual bool initializeInterface() {
        // setup socket (see IpBus)
        return true;
    }

    /*! read CAN message from the device driver
     * @return true if a message was successfully read and parsed
     */
    virtual bool readData() {
        // read from socket (see IpBus)
        return true;
    }

    /*! write CAN message to the device driver
     * @return true if the message was successfully written
     */
    virtual bool writeData(const EthernetFrame& msg) {
        // write to socket (see IpBus)
        return true;
    }

    /*! Do a sanity check of all devices on this bus.
     */
    void sanityCheck() {
        bool isMissing = false;
        bool allActive = true;
        for(auto device : devices_) {
            device->sanityCheck();
            isMissing |= device->isMissing();
            allActive &= device->isActive();
        }

        isMissingDevice_ = isMissing;
        allDevicesActive_ = allActive;
    }
protected:
    inline std::shared_ptr<Datagram> addDatagram(Datagram&& datagram) {
        // put the datagram at apropriate position to replicate the physical structure of the bus in the datagram order?
        auto ptr = std::make_shared<Datagram>(std::forward<Datagram>(datagram));
        datagrams_.push_back(ptr);
        return ptr;
    }

 protected:
    // vector containing all devices
    std::vector<CanDevice*> devices_;

    // map mapping COB id to parse functions
    AddressToFunctionMap addressToFunctionMap_;

    std::vector<std::shared_ptr<Datagram>> datagrams_;
};

} /* namespace tcan */
