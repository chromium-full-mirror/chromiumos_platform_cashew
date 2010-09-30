// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/cashew_server.h"

#include <glog/logging.h>

namespace cashew {

const char* CashewServer::kServiceName = "org.chromium.Cashew";
const char* CashewServer::kServicePath = "/org/chromium/Cashew";

CashewServer::CashewServer(DBus::Connection& connection)  // NOLINT
    : DBus::ObjectAdaptor(connection, kServicePath) {
}

// Cashew D-Bus API methods

DataPlanList CashewServer::GetDataPlans(const std::string& service,
                                        DBus::Error& error) {  // NOLINT
  DLOG(INFO) << "GetDataPlans: service = " << service;
  // TODO(vlaviano): log and return error if we don't like service arg

  // For now, we return hardcoded data.
  // TODO(vlaviano): return real data.
  DataPlan hardcoded_plan;
  GetHardcodedDataPlan(&hardcoded_plan);
  DataPlanList data_plans;
  data_plans.push_back(hardcoded_plan);
  return data_plans;
}

void CashewServer::RequestDataPlansUpdate(const std::string& service,
                                          DBus::Error& error) {  // NOLINT
  DLOG(INFO) <<  "RequestDataPlansUpdate: service = " << service;
  // TODO(vlaviano): log and return error if we don't like service arg

  // trigger a DataPlansUpdate signal
  // TODO(vlaviano): rate-limiting?
  // TODO(vlaviano): if cached data isn't recent, ask carrier API for fresh
  // data and wait for reply before sending signal.
  EmitDataPlansUpdateSignal(service);
}

bool CashewServer::IsAlive(DBus::Error& error) {  // NOLINT
  DLOG(INFO) << "IsAlive";
  return true;
}

// Private methods

void CashewServer::EmitDataPlansUpdateSignal(const std::string& service) {
  DLOG(INFO) << "EmitDataPlansUpdateSignal: service = " << service;
  // TODO(vlaviano): log error and return if we don't like service arg

  // For now, we return hardcoded data.
  // TODO(vlaviano): return real data.
  DataPlan hardcoded_plan;
  GetHardcodedDataPlan(&hardcoded_plan);
  DataPlanList data_plans;
  data_plans.push_back(hardcoded_plan);

  // send out the signal
  this->DataPlansUpdate(service, data_plans);
}

void CashewServer::GetHardcodedDataPlan(DataPlan* plan) {
  DCHECK(plan != NULL);

  const std::string plan_name = "test";
  const std::string plan_type = "UNLIMITED";
  int64 plan_update_time = 0;
  int64 plan_start_time = time(NULL) - 12*3600;  // 12 hours ago
  int64 plan_end_time = time(NULL) + 12*3600;  // 12 hours hence
  int64 plan_data_bytes = 100*1024*1024;  // 100 MB
  int64 plan_data_bytes_used = 30*1024*1024;  // 30 MB

  (*plan)["CellularPlanName"].writer().append_string(plan_name.c_str());
  (*plan)["CellularPlanType"].writer().append_string(plan_type.c_str());
  (*plan)["CellularPlanUpdateTime"].writer().append_int64(plan_update_time);
  (*plan)["CellularPlanStart"].writer().append_int64(plan_start_time);
  (*plan)["CellularPlanEnd"].writer().append_int64(plan_end_time);
  (*plan)["CellularPlanDataBytes"].writer().append_int64(plan_data_bytes);
  (*plan)["CellularDataBytesUsed"].writer().append_int64(plan_data_bytes_used);
}

}  // namespace cashew
