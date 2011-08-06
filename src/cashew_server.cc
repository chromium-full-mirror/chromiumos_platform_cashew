// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/cashew_server.h"

#include <glog/logging.h>

#include "src/aggregator.h"
#include "src/service_manager.h"

namespace cashew {

const char* CashewServer::kServiceName = "org.chromium.Cashew";
const char* CashewServer::kServicePath = "/org/chromium/Cashew";

// Cashew D-Bus errors
const char* kCashewErrorServiceUnknown =
    "org.chromium.Cashew.Error.ServiceUnknown";

CashewServer::CashewServer(DBus::Connection& connection,  // NOLINT
                           ServiceManager * const service_manager,
                           GMainLoop * const main_loop,
                           Aggregator * const aggregator)
    : DBus::ObjectAdaptor(connection, kServicePath),
      service_manager_(CHECK_NOTNULL(service_manager)),
      main_loop_(CHECK_NOTNULL(main_loop)),
      aggregator_(CHECK_NOTNULL(aggregator)) {
  service_manager_->SetCashewServer(this);
}

CashewServer::~CashewServer() {
  DCHECK(!g_main_loop_is_running(main_loop_));
  service_manager_->SetCashewServer(NULL);
}

// Cashew D-Bus API methods

DBusDataPlanList CashewServer::GetDataPlans(const std::string& service_path,
                                            DBus::Error& error) {  // NOLINT
  LOG(INFO) << "GetDataPlans: service = " << service_path;
  DBusDataPlanList dbus_data_plans;
  const Service *service = service_manager_->GetService(service_path);
  if (service == NULL) {
    LOG(WARNING) << "GetDataPlans: unknown service: " << service_path;
    error.set(kCashewErrorServiceUnknown, service_path.c_str());
    DCHECK(dbus_data_plans.empty());
    return dbus_data_plans;
  }
  dbus_data_plans = service->GetDBusDataPlans();
  return dbus_data_plans;
}

void CashewServer::RequestDataPlansUpdate(const std::string& service_path,
                                          DBus::Error& error) {  // NOLINT
  LOG(INFO) <<  "RequestDataPlansUpdate: service = " << service_path;
  const Service *service = service_manager_->GetService(service_path);
  if (service == NULL) {
    LOG(WARNING) << "RequestDataPlansUpdate: unknown service: "
        << service_path;
    error.set(kCashewErrorServiceUnknown, service_path.c_str());
    return;
  }

  // trigger a DataPlansUpdate signal
  // TODO(vlaviano): rate-limiting?
  // TODO(vlaviano): if cached data isn't recent, ask carrier API for fresh
  // data and wait for reply before sending signal.
  EmitDataPlansUpdateSignal(*service);
}

void CashewServer::ReportBytesUsed(const int64_t& bytes,
                                   DBus::Error &error) { // NOLINT
  return;
}

CashewServer::BytesPerItem CashewServer::GetByteUsageData(
    DBus::Error &error) {  // NOLINT
  CashewServer::BytesPerItem to_return;
  return to_return;
}

bool CashewServer::IsAlive(DBus::Error& error) {  // NOLINT
  LOG(INFO) << "IsAlive";
  return true;
}

// Service Manager methods

void CashewServer::EmitDataPlansUpdate(const Service& service) {
  EmitDataPlansUpdateSignal(service);
}

// Private methods

void CashewServer::EmitDataPlansUpdateSignal(const Service& service) {
  LOG(INFO) << "EmitDataPlansUpdateSignal: service = " << service.GetPath();
  DBusDataPlanList dbus_data_plans = service.GetDBusDataPlans();

  // send out the signal
  DataPlansUpdate(service.GetPath(), dbus_data_plans);
}

}  // namespace cashew
