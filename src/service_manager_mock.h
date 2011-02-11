// Copyright (c) 2010, 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_SERVICE_MANAGER_MOCK_H_
#define SRC_SERVICE_MANAGER_MOCK_H_

#include <gmock/gmock.h>

#include <string>

#include "src/service_manager.h"

namespace cashew {

class ServiceManagerMock : public ServiceManager {
  public:
    MOCK_CONST_METHOD1(GetService,
                       const Service*(const std::string& service_path));
    MOCK_METHOD1(SetCashewServer, void(CashewServer *cashew_server));
    MOCK_CONST_METHOD0(GetDefaultTechnology, Service::Type());
    MOCK_CONST_METHOD0(GetConnectivityState,
                       ServiceManager::ConnectivityState());
    MOCK_METHOD1(EmitDataPlansUpdate, void(const Service& service));
    MOCK_METHOD2(OnDataPlanInactive,
                 void(const Service& service, const DataPlan& plan));
};

}  // namespace cashew

#endif  // SRC_SERVICE_MANAGER_MOCK_H_
