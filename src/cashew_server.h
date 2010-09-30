// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CASHEW_SERVER_H_
#define SRC_CASHEW_SERVER_H_

#include <map>
#include <string>
#include <vector>

#include <base/basictypes.h>  // NOLINT
#include <dbus-c++/dbus.h>  // NOLINT

#include "src/cashew_server_glue.h"

namespace cashew {

typedef std::map<std::string, DBus::Variant> DataPlan;
typedef std::vector<DataPlan> DataPlanList;

// Implements Cashew D-Bus API
class CashewServer : public org::chromium::Cashew_adaptor,
                     public DBus::IntrospectableAdaptor,
                     public DBus::ObjectAdaptor {
  public:
    explicit CashewServer(DBus::Connection& connection);  // NOLINT
    virtual ~CashewServer() {}

    // Cashew D-Bus API methods

    // Get data plan info for a cellular service
    virtual DataPlanList GetDataPlans(const std::string& service,
                                      DBus::Error& error);  // NOLINT

    // Request that a DataPlansUpdate signal be sent for a cellular service
    virtual void RequestDataPlansUpdate(const std::string& service,
                                        DBus::Error& error);  // NOLINT

    // Test method to verify that Cashew service is working
    virtual bool IsAlive(DBus::Error& error);  // NOLINT

    static const char* kServiceName;
    static const char* kServicePath;

  private:
    // Emit a DataPlansUpdate signal for a service
    void EmitDataPlansUpdateSignal(const std::string& service);

    // Fill in data plan with hardcoded data for testing
    void GetHardcodedDataPlan(DataPlan* data_plan);

    DISALLOW_COPY_AND_ASSIGN(CashewServer);
};

}  // namespace cashew

#endif  // SRC_CASHEW_SERVER_H_
