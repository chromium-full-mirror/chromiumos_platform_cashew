// Copyright (c) 2010, 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_DEVICE_MOCK_H_
#define SRC_DEVICE_MOCK_H_

#include <gmock/gmock.h>

#include <string>

#include "src/device.h"

namespace cashew {

class DeviceMock : public Device {
  public:
    MOCK_CONST_METHOD0(GetPath, const DBus::Path&());
    MOCK_CONST_METHOD0(GetType, Device::Type());
    MOCK_CONST_METHOD0(GetCarrier, const std::string&());
    MOCK_CONST_METHOD0(GetInterface, const std::string&());
    MOCK_METHOD0(StartByteCounter, bool());
    MOCK_METHOD0(StopByteCounter, void());
    MOCK_CONST_METHOD0(ByteCounterRunning, bool());
};

}  // namespace cashew

#endif  // SRC_DEVICE_MOCK_H_
