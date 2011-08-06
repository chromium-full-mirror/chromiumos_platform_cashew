// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_SERVICE_MOCK_H_
#define SRC_SERVICE_MOCK_H_

#include <gmock/gmock.h>

#include <string>

#include "src/service.h"

namespace cashew {

class ServiceMock : public Service {
  public:
    MOCK_CONST_METHOD0(GetPath, const DBus::Path&());
    MOCK_CONST_METHOD0(GetState, Service::State());
    MOCK_CONST_METHOD0(GetType, Service::Type());
    MOCK_CONST_METHOD0(GetDevice, Device*());
    MOCK_CONST_METHOD0(GetDBusDataPlans, DBusDataPlanList());
    MOCK_CONST_METHOD0(IsDefaultService, bool());
    MOCK_METHOD1(OnCarrierUpdate, void(const std::string& carrier));
    MOCK_METHOD2(OnByteCounterUpdate, void(uint64 rx_bytes, uint64 tx_bytes));
    MOCK_METHOD0(OnByteCounterStopped, void());
    MOCK_METHOD2(OnByteCounterReset, void(uint64 rx_bytes, uint64 tx_bytes));
    MOCK_METHOD1(OnDefaultServiceUpdate, void(bool is_default_service));
};

}  // namespace cashew

#endif  // SRC_SERVICE_MOCK_H_
