/*
 * EtherCatBus.hpp
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
#include <inttypes.h>

#include <ethercat.h>


#include "tcan/Bus.hpp"
#include "tcan/CanMsg.hpp"
#include "tcan/EtherCatBusOptions.hpp"
#include "tcan/EtherCatDevice.hpp"
#include "tcan/EtherCatDatagramData.hpp"
#include "tcan/SdoMsg.hpp"

namespace tcan {



//struct EthernetHeader {
//    uint8_t destination_[6];
//    uint8_t source_[6];
//    uint8_t etherType_[2];
//
//    EthernetHeader()
//    :   destination_{0,0,0,0,0,0},
//        source_{0,0,0,0,0,0},
//        etherType_{0,0} {}
//};

struct EtherCatHeader {
    uint16_t data_ = 0;

    EtherCatHeader() {}

    uint16_t getLength() const {
        return (data_ & 0xFFE0) >> 5;
    }

    void setLength(uint16_t length) {
        data_ &= ~0xFFE0; // clear bits first.
        data_ = (length << 5) & 0xFFE0;
    }

    uint16_t getReserved() const {
        return (data_ & 0x0001) >> 4;
    }

    void setReserved(uint16_t reserved) {
        data_ &= ~0x0001; // clear bits first.
        data_ = (reserved << 4) & 0x0001;
    }

    uint16_t getType() const {
        return (data_ & 0x000F) >> 0;
    }

    void setType(uint16_t type) {
        data_ &= ~0x000F; // clear bits first.
        data_ = (type << 0) & 0x000F;
    }
};

class EtherCatDatagram {
 public:
    inline void resize(const uint16_t length) {
        uint8_t* oldData = data_;
        data_ = new uint8_t[length];
        std::copy(&oldData[0], &oldData[header_.lenRCM_.elements_.len_], data_);
        header_.lenRCM_.elements_.len_ = length;
        delete[] oldData;
    }

    template <typename T>
    inline void write(const uint16_t memoryPosition, const T& data) {
        // check if memoryPosition lies within data_?
        std::copy(&data_[memoryPosition], &data_[memoryPosition + sizeof(T)], &data);
    }

    inline const uint16_t getTotalLength() const { return getDataLength() + 12; }
    inline const uint16_t getDataLength() const { return header_.lenRCM_.elements_.len_; }
    inline const uint8_t* getData() const { return data_; }
    inline const uint16_t getWorkingCounter() const { return workingCounter_; }

    inline void setZero() {
        for (uint16_t i = 0; i < getDataLength(); i++) {
            data_[i] = 0;
        }
    }

 private:

    struct EtherCatDatagramHeader {
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
        // Note: Bit field ordering depends on the Endianness which is implementation dependent.
        struct LenRCMBits {
          uint16_t len_           :11; // does this sub-byte work?
          uint16_t reserved_      :3;
          uint16_t circulating_   :1;
          uint16_t more_          :1;
        };
        // Note: The constructor per default calls the initializer of the first element of a union.
        union LenRCM {
          uint16_t all_;
          LenRCMBits elements_;
        };

        Command cmd_;
        uint8_t idx_;
        uint32_t address_;
        LenRCM lenRCM_;
        uint16_t irq_;

//        DatagramHeader(): cmd_(Command::NOP), idx_(0), address_(0), len_(0), reserved_(0), circulating_(0), more_(0), irq_(0) { }
        EtherCatDatagramHeader(): cmd_(Command::NOP), idx_(0), address_(0), lenRCM_{0}, irq_(0) { }

//        uint16_t getLength() const {
//            return (lenRCM_ & 0xFFE0) >> 5;
//        }
//
//        void setLength(uint16_t length) {
//            lenRCM_ &= ~0xFFE0; // clear bits first.
//            lenRCM_ = (length << 5) & 0xFFE0;
//        }
//
//        uint16_t getReserved() const {
//            return (lenRCM_ & 0x001C) >> 2;
//        }
//
//        void setReserved(uint16_t reserved) {
//            lenRCM_ &= ~0x001C; // clear bits first.
//            lenRCM_ = (reserved << 2) & 0x001C;
//        }
//
//        uint16_t getCirculating() const {
//            return (lenRCM_ & 0x0002) >> 1;
//        }
//
//        void setCirculating(uint16_t circulating) {
//            lenRCM_ &= ~0x0002; // clear bits first.
//            lenRCM_ = (circulating << 1) & 0x0002;
//        }
//
//        uint16_t getMore() const {
//            return (lenRCM_ & 0x0001) >> 0;
//        }
//
//        void setMore(uint16_t more) {
//            lenRCM_ &= ~0x0001; // clear bits first.
//            lenRCM_ = (more << 1) & 0x0001;
//        }
    } __attribute__((packed)); // prevent structure padding


 public:
    EtherCatDatagramHeader header_;
    uint8_t* data_ = nullptr;
    uint16_t workingCounter_ = 0;

    // have a map of (name string => memory address) to have human-readable access to the fields in data_? would need to update this on resize(..) calls..
};


class EtherCatDatagrams {
 public:
    // TODO: function callbacks could be added here.
    // TODO: avoid multiple pdos per address?
    std::unordered_map<uint32_t, std::pair<EtherCatDatagram, EtherCatDatagram>> rxAndTxDatagrams_;
};


class EtherCatBus : public Bus<EtherCatDatagrams> {
 public:
    typedef std::function<bool(const EtherCatDatagram&)> CallbackPtr;
    typedef std::unordered_map<uint32_t, std::pair<EtherCatDevice*, CallbackPtr>> AddressToFunctionMap;

    EtherCatBus() = delete;
    EtherCatBus(EtherCatBusOptions* options)
    : Bus<EtherCatDatagrams>(options),
      wkcExpected_(0),
      wkc_(0),
      inOP_(false),
      ecatContext_({
          &ecat_port,          // .port          =
          &ecat_slave[0],       // .slavelist     =
          &ecat_slavecount,     // .slavecount    =
          EC_MAXSLAVE,        // .maxslave      =
          &ecat_group[0],       // .grouplist     =
          EC_MAXGROUP,        // .maxgroup      =
          &ecat_esibuf[0],      // .esibuf        =
          &ecat_esimap[0],      // .esimap        =
          0,                  // .esislave      =
          &ecat_elist,          // .elist         =
          &ecat_idxstack,       // .idxstack      =
          &ecat_error,         // .ecaterror     =
          0,                  // .DCtO          =
          0,                  // .DCl           =
          &ecat_DCtime,         // .DCtime        =
          &ecat_SMcommtype[0],  // .SMcommtype    =
          &ecat_PDOassign[0],   // .PDOassign     =
          &ecat_PDOdesc[0],     // .PDOdesc       =
          &ecat_SM,             // .eepSM         =
          &ecat_FMMU,           // .eepFMMU       =
          NULL                // .FOEhook()
      }) {}

    virtual ~EtherCatBus() {
        cleanupInterface();
    }

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
    inline bool addDevice(EtherCatDevice* device) {
        // asssign the device some id to calculate the offset in ethernet frame address
        devices_.push_back(device);
        return device->initDeviceInternal(this);
    }

//    template <class T>
//    inline std::shared_ptr<EtherCatDatagram> addReadDatagram(EtherCatDatagram&& datagram, T* device, bool(std::common_type<T>::type::*fp)()) {
//        // add a datagram linked with a callback to addressToFunctionMap_, use datagram address as key
//        return addDatagram(std::forward<EtherCatDatagram>(datagram));
//    }
//
//    inline std::shared_ptr<EtherCatDatagram> addWriteDatagram(EtherCatDatagram&& datagram) {
//        return addDatagram(std::forward<EtherCatDatagram>(datagram));
//    }

    /*!
     * Should be called from the same thread as write operations on the datagrams, otherwise it is up to the user to ensure thread safety
     * @return  true on success
     */
//    inline void dispatchFrame() {
//        // Compute length of the ethernet frame.
//        uint16_t len = 20; // Ethernet + EtherCat overhead. TODO: derive EthernetFrame from GenericMsg and add length getter
//        for (const auto& datagramPtr : datagrams_) {
//            len += datagramPtr->getTotalLength();
//        }
//
//        // Instantiate data.
//        uint8_t* data = new uint8_t[len];
//        uint16_t pos = 0;
//
//        // Copy ethercat header.
//        data[pos++] = static_cast<uint8_t>((ethercatHeader.data_ >> 0) & 0xFF);
//        data[pos++] = static_cast<uint8_t>((ethercatHeader.data_ >> 8) & 0xFF);
//        // Copy ethercat datagrams.
//        for (const auto& datagramPtr : datagrams_) {
//            // Copy ethercat datagram header.
//            data[pos++] = static_cast<uint8_t>(datagramPtr->header_.cmd_);
//            data[pos++] = static_cast<uint8_t>(datagramPtr->header_.idx_);
//            data[pos++] = static_cast<uint8_t>((datagramPtr->header_.address_ >> 0) & 0xFF);
//            data[pos++] = static_cast<uint8_t>((datagramPtr->header_.address_ >> 8) & 0xFF);
//            data[pos++] = static_cast<uint8_t>((datagramPtr->header_.address_ >> 16) & 0xFF);
//            data[pos++] = static_cast<uint8_t>((datagramPtr->header_.address_ >> 24) & 0xFF);
//            data[pos++] = static_cast<uint8_t>((datagramPtr->header_.lenRCM_.lenRCMAll_ >> 0) & 0xFF);
//            data[pos++] = static_cast<uint8_t>((datagramPtr->header_.lenRCM_.lenRCMAll_ >> 8) & 0xFF);
//            data[pos++] = static_cast<uint8_t>((datagramPtr->header_.irq_ >> 0) & 0xFF);
//            data[pos++] = static_cast<uint8_t>((datagramPtr->header_.irq_ >> 8) & 0xFF);
//            // Copy ethercat datagram data.
//            for (uint16_t i = 0; i < datagramPtr->getDataLength(); i++) {
//                data[pos++] = static_cast<uint8_t>(datagramPtr->data_[i]);
//            }
//            // Copy ethercat datagram working counter.
//            data[pos++] = static_cast<uint8_t>((datagramPtr->workingCounter_ >> 0) & 0xFF);
//            data[pos++] = static_cast<uint8_t>((datagramPtr->workingCounter_ >> 8) & 0xFF);
//        }
//
//        // compute expected working counter
//
//        // other option:
//        EthernetFrame msg;
//        msg.emplaceData(len, data);
//        sendMessage(msg);
//    }

    void discoverDevices() {
        // do not add the devices by hand but get the topology and memory from the connected devices?
    }

 public:/// INTERNAL FUNCTIONS
    /*! Is called after reception of a message. Routes the message to the callback.
     * @param cmsg    reference to the can message
     */
    void handleMessage(const EtherCatDatagrams& msg) {

        // TODO:
        // check time the frame took to travel through the physical bus
        // check working counters
        // extract datagrams from ethernet frame and map datagrams to device callback
    }

    /*! Initialize the device driver
     * @return true if successful
     */
    virtual bool initializeInterface() {
        printf("Starting simple test\n");
        inOP_ = false;

        /*
         * Followed by start of the application we need to set up the NIC to be used as
         * EtherCAT Ethernet interface. In a simple setup we call ec_init(ifname) and if
         * SOEM comes with support for cable redundancy we call ec_init_redundant that
         * will open a second port as backup. You can send NULL as ifname if you have a
         * dedicated NIC selected in the nicdrv.c. It returns >0 if succeeded.
         */
        const char* ifname = options_->name_.c_str();
        if (ecx_init(&ecatContext_, ifname) <= 0) {
            MELO_ERROR_STREAM("No socket connection on '" << ifname << "'.");
            MELO_ERROR_STREAM("Excecute as root.");
            return false;
        }

        MELO_INFO_STREAM("EtherCAT initialization on '" << ifname << "' succeeded.");

        /*
         * SOEM is a light weight ethercat master library used in embedded systems,
         * It supports only runtime configuration. It requests a BRD (Broad Cast Read)
         * of address 0, all fully functional slaves in the network will respond to this
         * request, and therefore we will get a working counter equal to the number of
         * slaves in the network. ec_config_init also sets up the mailboxes for slaves
         * that support it. When ec_config_init finishes it will have requested all
         * slaves to state PRE_OP. All data read and configured are stored in a global
         * array which acts as a placeholder for key values, consult ec_slave for
         * detailed information.
         */
        if (ecx_config_init(&ecatContext_, FALSE) == 0) {
            MELO_ERROR_STREAM("No slaves have been found.");
            return false;
        }

        MELO_INFO_STREAM("The following " << *ecatContext_.slavecount << " slaves have been found and configured:");
        for (int i = 1; i <= *ecatContext_.slavecount; i++) {
            MELO_INFO_STREAM(i << ": " << std::string(ecatContext_.slavelist[i].name));
        }

        if (!allDeviceMatch()) {
            MELO_ERROR_STREAM("Expected and discovered devices mismatch.");
            return false;
        }
        MELO_INFO_STREAM("Expected and discovered devices match.");

        /*!
         * We now have the network up and configured. Mailboxes are up for slaves that
         * support it. Next we will create an IOmap and configure the SyncManager's and
         * FMMU's to link the EtherCAT master and the slaves. The IO mapping is done
         * automatically, SOEM strives to keep the logical process image as compact as
         * possible. It is done by trying to fit Bit oriented slaves together in single
         * bytes. Below is an example of 8 slaves and how they are ordered. During
         * mapping SOEM also calculates an expected wkc_ for the IO mapped together.
         * That is the primary key to detect errors.
         *
         *  * Outputs are placed together in the beginning of the IOmap
         *  * Inputs follow
         *
         * When the mapping is done SOEM requests slaves to enter SAFE_OP.
         */
        ecx_config_map_group(&ecatContext_, &IOmap_, 0);
        ecx_configdc(&ecatContext_);

        printf("Slaves mapped, state to SAFE_OP.\n");
        /* wait for all slaves to reach SAFE_OP state */
        if (ecx_statecheck(&ecatContext_, 0, EC_STATE_SAFE_OP,  EC_TIMEOUTSTATE * 4) != EC_STATE_SAFE_OP) {
            printf("State EC_STATE_SAFE_OP has not been reached.\n");
        }

//        int oloop_ = ecatContext_.slavelist[0].Obytes;
//        if ((oloop_ == 0) && (ecatContext_.slavelist[0].Obits > 0)) oloop_ = 1;
////        if (oloop_ > 8) oloop_ = 8;
//        int iloop_ = ecatContext_.slavelist[0].Ibytes;
//        if ((iloop_ == 0) && (ecatContext_.slavelist[0].Ibits > 0)) iloop_ = 1;
////        if (iloop_ > 8) iloop_ = 8;

        printf("segments : %d : %d %d %d %d\n",
            ecatContext_.grouplist[0].nsegments,
            ecatContext_.grouplist[0].IOsegment[0],
            ecatContext_.grouplist[0].IOsegment[1],
            ecatContext_.grouplist[0].IOsegment[2],
            ecatContext_.grouplist[0].IOsegment[3]);

        // Disable symmetrical transfers
        ecatContext_.grouplist[0].blockLRW = 1;

        wkcExpected_ = (ecatContext_.grouplist[0].outputsWKC * 2) + ecatContext_.grouplist[0].inputsWKC;
        printf("Calculated workcounter %d\n", wkcExpected_.load());




        checkSlaveStates();
        for (EtherCatDevice* device : devices_) {
            if (!device->initializeInterface()) {
                MELO_ERROR_STREAM("Device '" << device->getName() << "' was not initialized successfully.");
                return false;
            }
        }
        checkSlaveStates();
        checkSlaveErrors();


        strangeFunction();

        return true;
    }

    void cleanupInterface() {
        /* request INIT state for all slaves */
        printf("\nRequest init state for all slaves\n");
        ecatContext_.slavelist[0].state = EC_STATE_INIT;
        ecx_writestate(&ecatContext_, 0);

        /* stop SOEM, close socket */
        printf("End simple test, close socket\n");
        ecx_close(&ecatContext_);
        printf("Closed socket\n");
    }

    /*! read CAN message from the device driver
     * @return true if a message was successfully read and parsed
     */
    virtual bool readData() {
        if (!datagramsSent_) {
            MELO_WARN_STREAM("No data has been sent yet.");
            return false;
        }

        receiveProcessData();

        if (wkc_ < wkcExpected_) {
            MELO_WARN_STREAM("Working counter is too low (" << wkc_ << " < " << wkcExpected_ << ").");
            return false;
        }

        for (int i = 1; i <= *ecatContext_.slavecount; i++) {
            memcpy(
                datagramsSent_->rxAndTxDatagrams_[i].second.data_,
                ecatContext_.slavelist[i].inputs,
                datagramsSent_->rxAndTxDatagrams_[i].second.getDataLength());
        }

        // TODO fill this.
//        EtherCatDatagrams etherCatDatagrams;
//        handleMessage(etherCatDatagrams);

        return false; // TODO true?
    }

    /*! write CAN message to the device driver
     * @return true if the message was successfully written
     */
    virtual bool writeData(const EtherCatDatagrams& msg) {

        for (const auto& rxAndTxDatagram : msg.rxAndTxDatagrams_) {
            memcpy(
                ecatContext_.slavelist[rxAndTxDatagram.second.first.header_.address_].outputs,
                rxAndTxDatagram.second.first.getData(),
                rxAndTxDatagram.second.first.getDataLength());
        }

        datagramsSent_.reset(new EtherCatDatagrams(msg));

        sendProcessData();

        return true;
    }

    /*! Do a sanity check of all devices on this bus.
     */
    void sanityCheck() {
//        uint8 currentgroup = 0;
//        if(inOP_ && ((wkc_ < wkcExpected_) || ecatContext_.grouplist[currentgroup].docheckstate)) {
//            /* one ore more slaves are not responding */
//            ecatContext_.grouplist[currentgroup].docheckstate = FALSE;
//            ecx_readstate(&ecatContext_);
//            for (int slave = 1; slave <= *ecatContext_.slavecount; slave++) {
//                if ((ecatContext_.slavelist[slave].group == currentgroup) && (ecatContext_.slavelist[slave].state != EC_STATE_OPERATIONAL))
//                {
//                    ecatContext_.grouplist[currentgroup].docheckstate = TRUE;
//                    if (ecatContext_.slavelist[slave].state == (EC_STATE_SAFE_OP + EC_STATE_ERROR)) {
//                        printf("ERROR : slave %d is in SAFE_OP + ERROR, attempting ack.\n", slave);
//                        ecatContext_.slavelist[slave].state = (EC_STATE_SAFE_OP + EC_STATE_ACK);
//                        ecx_writestate(&ecatContext_, slave);
//                    }
//                    else if(ecatContext_.slavelist[slave].state == EC_STATE_SAFE_OP) {
//                        printf("WARNING : slave %d is in SAFE_OP, change to OPERATIONAL.\n", slave);
//                        ecatContext_.slavelist[slave].state = EC_STATE_OPERATIONAL;
//                        ecx_writestate(&ecatContext_, slave);
//                    }
//                    else if(ecatContext_.slavelist[slave].state > 0) {
//                        if (ecx_reconfig_slave(&ecatContext_, slave, timeoutmon_)) {
//                            ecatContext_.slavelist[slave].islost = FALSE;
//                            printf("MESSAGE : slave %d reconfigured\n",slave);
//                        }
//                    }
//                    else if(!ecatContext_.slavelist[slave].islost) {
//                        /* re-check state */
//                        ecx_statecheck(&ecatContext_, slave, EC_STATE_OPERATIONAL, EC_TIMEOUTRET);
//                        if (!ecatContext_.slavelist[slave].state) {
//                            ecatContext_.slavelist[slave].islost = TRUE;
//                            printf("ERROR : slave %d lost\n",slave);
//                        }
//                    }
//                }
//                if (ecatContext_.slavelist[slave].islost) {
//                    if(!ecatContext_.slavelist[slave].state) {
//                        if (ecx_recover_slave(&ecatContext_, slave, timeoutmon_)) {
//                          ecatContext_.slavelist[slave].islost = FALSE;
//                            printf("MESSAGE : slave %d recovered\n",slave);
//                        }
//                    }
//                    else {
//                        ecatContext_.slavelist[slave].islost = FALSE;
//                        printf("MESSAGE : slave %d found\n",slave);
//                    }
//                }
//            }
//            if(!ecatContext_.grouplist[currentgroup].docheckstate) {
//                printf("OK : all slaves resumed OPERATIONAL.\n");
//            }
//        }






        bool isMissingOrError = false;
        bool allActive = true;
        for(auto device : devices_) {
            device->sanityCheck();
            isMissingOrError |= device->isMissing() | device->hasError();
            allActive &= device->isActive();
        }

        isMissingDeviceOrHasError_ = isMissingOrError;
        allDevicesActive_ = allActive;
    }

    std::shared_ptr<EtherCatDatagrams> getData() {
        return datagramsSent_;
    }

//    inline std::shared_ptr<EtherCatDatagram> addDatagram(EtherCatDatagram&& datagram) {
//        // put the datagram at apropriate position to replicate the physical structure of the bus in the datagram order?
//        auto ptr = std::make_shared<EtherCatDatagram>(std::forward<EtherCatDatagram>(datagram));
//        datagrams_.push_back(ptr);
//        return ptr;
//    }

    bool allDeviceMatch() {
        // Verify the expected number of slaves
        if (*ecatContext_.slavecount < static_cast<int>(devices_.size())) {
            MELO_ERROR_STREAM("Found too few slaves (" << *ecatContext_.slavecount << " < " << static_cast<int>(devices_.size()) << ").")
            return false;
        }
        else if (*ecatContext_.slavecount > static_cast<int>(devices_.size())) {
          MELO_ERROR_STREAM("Found too many slaves (" << *ecatContext_.slavecount << " < " << static_cast<int>(devices_.size()) << ").")
            return false;
        }

        // Verify slave by slave that the name is correct
        for (const EtherCatDevice* device : devices_) {
            if (std::string(ecatContext_.slavelist[device->getAddress()].name) != device->getName()) {
                MELO_ERROR_STREAM("Device with address '" << device->getAddress() << "' expected name '" << device->getName() << "' but found '" << std::string(ecatContext_.slavelist[device->getAddress()].name) << "'.");
                return false;
            }
        }

        return true;
    }

    void getSlaveInfo() {
        int ret = 0;
        int wkc = 0;
        int Osize = 0, Isize = 0;

        #define Nsdo  16
        #define Ndata 64
        int sdodata[Nsdo], sdodatasize;
        char *databuf;
        databuf = (char*)&sdodata[0];

        // Check PDO mapping size
        ret = ecx_readPDOmap(&ecatContext_, 1, &Osize, &Isize);
        printf("\n\nec_readPDOmap returned %d\n", ret);
        printf("Osize in bits = %d\n", Osize);
        printf("Isize in bits = %d\n", Isize);

        // Get complete OD dump
        ret = ecx_readODlist(&ecatContext_, 1, &odinfo_);
        printf("\nc_readODlist returned %d\n", ret);
        printf("Slave = %d, Entries = %d\n", odinfo_.Slave, odinfo_.Entries);
        for (int k = 0; k < odinfo_.Entries; k++) {
            wkc = ecx_readODdescription(&ecatContext_, k, &odinfo_);
            wkc = ecx_readOE(&ecatContext_, k, &odinfo_, &odentryinfo_);
            printf("\nIndex = 0x%x\n", odinfo_.Index[k]);
            printf("    MaxSub     = %d\n", odinfo_.MaxSub[k]+1);
            printf("    ObjectCode = %d\n", odinfo_.ObjectCode[k]);
            printf("    DataType   = %d\n", odinfo_.DataType[k]);
            printf("    Description: %s\n", &odinfo_.Name[k][0]);
            printf("    OE Entries = %d\n", odentryinfo_.Entries);
            for (int j = 0; j < odentryinfo_.Entries; j++) {
                for (int n=0; n<Nsdo; n++) sdodata[n] = 0;
                sdodatasize = Nsdo*sizeof(int);
                wkc = ecx_SDOread(&ecatContext_, odinfo_.Slave, odinfo_.Index[k], j, 0, &sdodatasize, &sdodata, EC_TIMEOUTRXM);
                printf("    OE = %d\n", j);
                printf("        ValueInfo  = %d\n", odentryinfo_.ValueInfo[j]);
                printf("        DataType   = %d\n", odentryinfo_.DataType[j]);
                printf("        BitLength  = %d\n", odentryinfo_.BitLength[j]);
                printf("        ObjAccess  = %d\n", odentryinfo_.ObjAccess[j]);
                printf("        Name       = %s\n", &odentryinfo_.Name[j][0]);
                printf("        Value      =");
                for (int n=0; n<sdodatasize; n++) printf(" 0x%x", (0xFF & databuf[n]));
                printf("\n");
            }
        }
    }

    void checkSlaveStates() {
        int slavestate = ecx_readstate(&ecatContext_);
        printf("\nSlave EtherCAT StateMachine state is 0x%x\n", slavestate);

        int sdodata = 0;
        int sdodatasize = sizeof(int);
        wkc_ = ecx_SDOread(&ecatContext_, 1, 0x6041, 0, 0, &sdodatasize, &sdodata, EC_TIMEOUTRXM);
        printf("Slave DSP402 StateMachine state is 0x%x\n", (uint16)sdodata);
    }

    void checkSlaveErrors() {
        printf("\nERROR REPORT:\n");
        if (ecx_iserror(&ecatContext_)) {
            int i = 0;
            do {
                char* errstr = ecx_elist2string(&ecatContext_);
                printf("    Error %d: %s", ++i, errstr);
            } while(ecx_iserror(&ecatContext_));
        }
    }

    template <typename Value>
    void sendSdoWrite(uint16_t slave, uint16_t index, uint8_t subindex, bool completeAccess, Value value) {
        const int size = sizeof(Value);
        wkc_ = ecx_SDOwrite(&ecatContext_, slave, index, subindex, static_cast<boolean>(completeAccess), size, &value, EC_TIMEOUTRXM);
    }

    template <typename Value>
    void sendSdoRead(uint16_t slave, uint16_t index, uint8_t subindex, bool completeAccess, Value& value) {
        int size = sizeof(Value);
        wkc_ = ecx_SDOread(&ecatContext_, slave, index, subindex, static_cast<boolean>(completeAccess), &size, &value, EC_TIMEOUTRXM);
        if (size != sizeof(Value)) {
            MELO_WARN_STREAM("Expected (" << sizeof(Value) << ") and read (" << size << ") SDO value size mismatch.");
        }
    }

    void sendSdoReadAndPrint(uint16_t slave, uint16_t index, uint8_t subindex, bool completeAccess) {
        int sdodata[4]={0,0,0,0};
        sendSdoRead(slave, index, subindex, completeAccess, sdodata);
        printf(" OD entry {Index 0x%x, Subindex 0x%x} is  0x%x 0x%x 0x%x 0x%x\n", index, subindex, sdodata[3], sdodata[2], sdodata[1], sdodata[0]);
    }

    void syncDc(uint16_t slave, bool activate) {
        MELO_INFO_STREAM("Starting synchronizing clock.")
        const double timeStep = 1e-3;
        ecx_dcsync0(&ecatContext_, slave, static_cast<boolean>(activate), (int64)(timeStep*1e9), (int64)(timeStep*0.5*1e9));
        MELO_INFO_STREAM("Finished synchronizing clock.")
    }

    void sendProcessData() {
        ecx_send_processdata(&ecatContext_);
    }

    void receiveProcessData() {
        wkc_ = ecx_receive_processdata(&ecatContext_, EC_TIMEOUTRET);
    }

    void setStatePreOp() { setState(EC_STATE_PRE_OP); }
    void setStateSafeOp() { setState(EC_STATE_SAFE_OP); }
    void setStateOperational() { setState(EC_STATE_OPERATIONAL); }

    void waitForStateOperational() { waitForState(EC_STATE_OPERATIONAL); }

    void strangeFunction() {
        if (ecatContext_.slavelist[0].state == EC_STATE_OPERATIONAL ) {
            printf("Operational state reached for all slaves.\n");
            inOP_ = true;

      //            // Handle PDO streams
      //            outdata_.controlword.all = 0;
      //            ecatcomm_slave_set_rxpdo(&outdata_, CLEAR_CONTROLWORD, 0.0);
      //            ecatcomm_slave_set_rxpdo(&outdata_, SHUTDOWN, 0.0);
      //            ecx_send_processdata(&ecatContext_);
      //            wkc_ = ecx_receive_processdata(&ecatContext_, EC_TIMEOUTRET);
      //            ecatcomm_slave_get_txpdo(&indata_);
      //            osal_usleep(1000);
        }
        else {
            printf("Not all slaves reached operational state.\n");
            ecx_readstate(&ecatContext_);
            for(int i = 1; i <= *ecatContext_.slavecount; i++) {
                if(ecatContext_.slavelist[i].state != EC_STATE_OPERATIONAL) {
                    printf("Slave %d State=0x%2.2x StatusCode=0x%4.4x : %s\n",
                    i, ecatContext_.slavelist[i].state, ecatContext_.slavelist[i].ALstatuscode, ec_ALstatuscode2string(ecatContext_.slavelist[i].ALstatuscode));
                }
            }
        }
    }

 protected:
    void setState(uint16_t state) {
        ecatContext_.slavelist[0].state = state;
        ecx_writestate(&ecatContext_, 0);
        ecx_statecheck(&ecatContext_, 0, state,  EC_TIMEOUTSTATE * 4);
    }

    void waitForState(uint16_t state) {
        int chk = 40;
        do {
            sendProcessData();
            receiveProcessData();
            ecx_statecheck(&ecatContext_, 0, state,  50000);
        } while (chk-- && (ecatContext_.slavelist[0].state != state));
    }

//    void sendSdo(uint32_t deviceAddress, tcan::SdoMsg& sdoMsg) {
//        // TODO: tcan::CanMsg can only hold 4 bytes. In case of CoE this should not be limited.
//
//        switch (static_cast<tcan::SdoMsg::Command>(sdoMsg.getCommandByte())) {
//            case tcan::SdoMsg::Command::READ:
//                int sdodata = 0;
//                int sdodatasize = static_cast<int>(sizeof(int));
//                wkc_ = ecx_SDOread(&ecatContext_, deviceAddress, sdoMsg.getIndex(), sdoMsg.getSubIndex(), FALSE, &sdodatasize, &sdodata, EC_TIMEOUTRXM);
//                sdoMsg.setData(static_cast<uint8_t>(sdodatasize), static_cast<uint8_t*>(&sdodata));
//                break;
//            case tcan::SdoMsg::Command::WRITE_1_BYTE:
//            case tcan::SdoMsg::Command::WRITE_2_BYTE:
//            case tcan::SdoMsg::Command::WRITE_4_BYTE:
//                wkc_ = ecx_SDOwrite(&ecatContext_, deviceAddress, sdoMsg.getIndex(), sdoMsg.getSubIndex(), FALSE, sdodatasize, sdodata, EC_TIMEOUTRXM);
//                break;
//            default:
//                break;
//        }
//    }

protected:
    // vector containing all devices
    std::vector<EtherCatDevice*> devices_;

    // map mapping COB id to parse functions
    AddressToFunctionMap addressToFunctionMap_;

    std::shared_ptr<EtherCatDatagrams> datagramsSent_;



    // EtherCAT Master Data
    /** Main slave data array.
     *  Each slave found on the network gets its own record.
     *  ec_slave[0] is reserved for the master. Structure gets filled
     *  in by the configuration function ec_config().
     */
    ec_slavet        ecat_slave[EC_MAXSLAVE];
    /** number of slaves found on the network */
    int              ecat_slavecount;
    /** slave group structure */
    ec_groupt        ecat_group[EC_MAXGROUP];

    /** cache for EEPROM read functions */
    uint8            ecat_esibuf[EC_MAXEEPBUF];
    /** bitmap for filled cache buffer bytes */
    uint32           ecat_esimap[EC_MAXEEPBITMAP];
    /** current slave for EEPROM cache buffer */
    ec_eringt        ecat_elist;
    ec_idxstackT     ecat_idxstack;

    /** SyncManager Communication Type struct to store data of one slave */
    ec_SMcommtypet   ecat_SMcommtype[EC_MAX_MAPT];
    /** PDO assign struct to store data of one slave */
    ec_PDOassignt    ecat_PDOassign[EC_MAX_MAPT];
    /** PDO description struct to store data of one slave */
    ec_PDOdesct      ecat_PDOdesc[EC_MAX_MAPT];

    /** buffer for EEPROM SM data */
    ec_eepromSMt     ecat_SM;
    /** buffer for EEPROM FMMU data */
    ec_eepromFMMUt   ecat_FMMU;
    /** Global variable TRUE if error available in error stack */
    boolean          ecat_error = FALSE;

    int64            ecat_DCtime;

    ecx_portt        ecat_port;
    ecx_redportt     ecat_redport;


    ecx_contextt ecatContext_;





    ec_ODlistt odinfo_;
    ec_OElistt odentryinfo_;

    const int timeoutmon_ = 500;
    char IOmap_[4096];
    std::atomic<int> wkcExpected_;
    std::atomic<int> wkc_;
    std::atomic<bool> inOP_;
};

} /* namespace tcan */

