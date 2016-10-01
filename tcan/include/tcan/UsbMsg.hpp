/*
 * CanMsg.hpp
 *
 *  Created on: Mar 27, 2016
 *      Author: Philipp Leemann
 */

#pragma once

#include <algorithm> // copy(..)

namespace tcan {

//! General USB message container

class UsbMsg {
 public:
    UsbMsg():
        length_(0),
        data_(nullptr)
    {
    }

    UsbMsg(const unsigned int length, const char* data):
        length_(length),
        data_(new char[length_])
    {
        std::copy(&data[0], &data[length_], data_);
    }

    UsbMsg(const std::string msg):
        length_(msg.length()),
        data_(new char[length_])
    {
        std::copy(&msg.c_str()[0], &msg.c_str()[length_], data_);
    }

    UsbMsg(const UsbMsg& other):
        length_(other.length_),
        data_(new char[length_])
    {
        std::copy(&other.data_[0], &other.data_[length_], data_);
    }

    virtual ~UsbMsg() {
        if(data_) {
            delete[] data_;
        }
    }

    unsigned int getLength() const { return length_; }
    const char* getData() const { return data_; }

 private:
    unsigned int length_;
    char* data_;

};

} /* namespace tcan */
