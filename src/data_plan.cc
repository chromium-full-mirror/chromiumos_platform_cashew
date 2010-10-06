// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/data_plan.h"

#include <glog/logging.h>

namespace cashew {

// libcros CellularDataPlan property names
static const char* kCellularDataPlanName = "CellularPlanName";
static const char* kCellularDataPlanType = "CellularPlanType";
static const char* kCellularDataPlanUpdateTime = "CellularPlanUpdateTime";
static const char* kCellularDataPlanStartTime = "CellularPlanStart";
static const char* kCellularDataPlanEndTime = "CellularPlanEnd";
static const char* kCellularDataPlanDataBytesMax = "CellularPlanDataBytes";
static const char* kCellularDataPlanDataBytesUsed = "CellularDataBytesUsed";

// libcros CellularDataPlan type values
static const char* kCellularDataPlanTypeUnlimited = "UNLIMITED";
static const char* kCellularDataPlanTypeMeteredFree = "METERED_BASE";
static const char* kCellularDataPlanTypeMeteredPaid = "METERED_PAID";

DataPlan::DataPlan(const std::string& name, DataPlan::Type type,
                   base::Time update_time, base::Time start_time,
                   base::Time end_time, Bytes data_bytes_max,
                   Bytes data_bytes_used)
    : name_(name), type_(type), update_time_(update_time),
      start_time_(start_time), end_time_(end_time),
      data_bytes_max_(data_bytes_max), data_bytes_used_(data_bytes_used) {
}

DataPlan::~DataPlan() {
}

const std::string& DataPlan::GetName() const {
  return name_;
}

DataPlan::Type DataPlan::GetType() const {
  return type_;
}

base::Time DataPlan::GetUpdateTime() const {
  return update_time_;
}

base::Time DataPlan::GetStartTime() const {
  return start_time_;
}

base::Time DataPlan::GetEndTime() const {
  return end_time_;
}

Bytes DataPlan::GetDataBytesMax() const {
  return data_bytes_max_;
}

Bytes DataPlan::GetDataBytesUsed() const {
  return data_bytes_used_;
}

DBusDataPlan DataPlan::ToDBusFormat() const {
  DBusDataPlan plan;
  // Indexing into map w/ nonexistent key causes empty DBus::Variant to be
  // created and inserted. We then fill in the desired value via its writer
  // DBus::MessageIterator.
  plan[kCellularDataPlanName].writer().append_string(name_.c_str());
  plan[kCellularDataPlanType].writer().append_string(TypeToString(type_));
  plan[kCellularDataPlanUpdateTime].writer().append_int64(
      update_time_.ToInternalValue());
  plan[kCellularDataPlanStartTime].writer().append_int64(
      start_time_.ToInternalValue());
  plan[kCellularDataPlanEndTime].writer().append_int64(
      end_time_.ToInternalValue());
  // omit max bytes field for unlimited plans
  // can libcros/Chrome deal? if not, can set this to int64 max.
  if (type_ != kTypeUnlimited) {
    plan[kCellularDataPlanDataBytesMax].writer().append_int64(data_bytes_max_);
  }
  plan[kCellularDataPlanDataBytesUsed].writer().append_int64(data_bytes_used_);
  return plan;
}

// private methods

const char* DataPlan::TypeToString(DataPlan::Type type) const {
  switch (type) {
    case kTypeUnlimited:
      return kCellularDataPlanTypeUnlimited;
    case kTypeMeteredFree:
      return kCellularDataPlanTypeMeteredFree;
    case kTypeMeteredPaid:
      return kCellularDataPlanTypeMeteredPaid;
    default:
      DCHECK(false);
  }
}

}  // namespace cashew
