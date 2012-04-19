// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/data_plan.h"

#include <time.h>

#include <glog/logging.h>

#include "src/device.h"
#include "src/policy.h"
#include "src/service_manager.h"

namespace cashew {

// libcros CellularDataPlan property names
// (what we send to libcros)
static const char *kCellularDataPlanName = "CellularPlanName";
static const char *kCellularDataPlanType = "CellularPlanType";
static const char *kCellularDataPlanUpdateTime = "CellularPlanUpdateTime";
static const char *kCellularDataPlanStartTime = "CellularPlanStart";
static const char *kCellularDataPlanEndTime = "CellularPlanEnd";
static const char *kCellularDataPlanDataBytesMax = "CellularPlanDataBytes";
static const char *kCellularDataPlanDataBytesUsed = "CellularDataBytesUsed";

// libcros CellularDataPlan type values
static const char *kCellularDataPlanTypeUnlimited = "UNLIMITED";
static const char *kCellularDataPlanTypeMeteredFree = "METERED_BASE";
static const char *kCellularDataPlanTypeMeteredPaid = "METERED_PAID";

// Chromium OS Usage API Data Plan property names
// (what we receive from carrier usage API)
static const char *kCrosUsageDataPlanLastUpdateProperty = "lastUpdate";
static const char *kCrosUsageDataPlanNameProperty = "planName";
static const char *kCrosUsageDataPlanTypeProperty = "planType";
static const char *kCrosUsageDataPlanMaxBytesProperty = "maxBytes";
static const char *kCrosUsageDataPlanUsedBytesProperty = "usedBytes";
static const char *kCrosUsageDataPlanEndTimeProperty = "expirationTime";
static const char *kCrosUsageDataPlanStartTimeProperty = "startTime";

// Chromium OS Usage API Data Plan type values
static const char *kCrosUsageDataPlanTypeUnlimited = "UNLIMITED";
static const char *kCrosUsageDataPlanTypeMeteredFree = "METERED_FREE";
static const char *kCrosUsageDataPlanTypeMeteredPaid = "METERED_PAID";

DataPlan::DataPlan(const std::string& name, DataPlan::Type type,
                   base::Time update_time, base::Time start_time,
                   base::Time end_time, ByteCount data_bytes_max,
                   ByteCount data_bytes_used)
    : service_(NULL), name_(name), type_(type), update_time_(update_time),
      start_time_(start_time), end_time_(end_time),
      data_bytes_max_(data_bytes_max), data_bytes_used_(data_bytes_used),
      local_bytes_used_(0), total_bytes_used_(data_bytes_used),
      expiration_timeout_source_(NULL) {
  CHECK_GE(data_bytes_max_, 0);
  CHECK_GE(data_bytes_used_, 0);

  // create and start the expiration timer
  if (!CreateExpirationTimer()) {
    LOG(WARNING) << "ctor: failed to create and start expiration timer";
  }
}

DataPlan::~DataPlan() {
  // destroy the expiration timer
  DestroyExpirationTimer();
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

ByteCount DataPlan::GetDataBytesMax() const {
  return data_bytes_max_;
}

ByteCount DataPlan::GetDataBytesUsed() const {
  return data_bytes_used_;
}

ByteCount DataPlan::GetLocalBytesUsed() const {
  return local_bytes_used_;
}

ByteCount DataPlan::GetTotalBytesUsed() const {
  return total_bytes_used_;
}

void DataPlan::SetService(Service *service) {
  CHECK(service != NULL);
  service_ = service;
}

void DataPlan::SetLocalBytesUsed(ByteCount local_bytes_used) {
  CHECK_GE(local_bytes_used, 0);
  local_bytes_used_ = local_bytes_used;
  total_bytes_used_ = data_bytes_used_ + local_bytes_used_;
  if (total_bytes_used_ < 0) {
    LOG(WARNING) << "SetLocalBytesUsed: overflow detected: |total_bytes_used|";
    total_bytes_used_ = kint64max;
  }

  // if we run out of bytes, stop expiration timer
  if (!HasBytesRemaining()) {
    DestroyExpirationTimer();
  }
}

bool DataPlan::IsActive() const {
  // is the plan current and not exahusted?
  return IsCurrent() && HasBytesRemaining();
}

DBusDataPlan DataPlan::ToDBusFormat() const {
  DBusDataPlan plan;
  // Indexing into map w/ nonexistent key causes empty DBus::Variant to be
  // created and inserted. We then fill in the desired value via its writer
  // DBus::MessageIter.
  plan[kCellularDataPlanName].writer().append_string(name_.c_str());
  plan[kCellularDataPlanType].writer().append_string(
      TypeToLibcrosString(type_));
  plan[kCellularDataPlanUpdateTime].writer().append_int64(
      update_time_.ToInternalValue());
  plan[kCellularDataPlanStartTime].writer().append_int64(
      start_time_.ToInternalValue());
  plan[kCellularDataPlanEndTime].writer().append_int64(
      end_time_.ToInternalValue());
  // omit max bytes field for unlimited plans
  if (type_ != kTypeUnlimited) {
    plan[kCellularDataPlanDataBytesMax].writer().append_int64(data_bytes_max_);
  }
  // send our best estimate of data bytes used
  // this is the baseline, if any, that we received from the carrier usage API
  // plus any local traffic that we've measured since then
  plan[kCellularDataPlanDataBytesUsed].writer().append_int64(total_bytes_used_);
  return plan;
}

// static
DataPlan* DataPlan::FromDictionaryValue(const DictionaryValue *value,
                                        const Policy *policy) {
  CHECK_NOTNULL(value);
  CHECK_NOTNULL(policy);
  std::string last_update_iso8601;
  if (!value->GetString(kCrosUsageDataPlanLastUpdateProperty,
                        &last_update_iso8601)) {
    LOG(WARNING) << "FromDictionaryValue: no last update property";
    return NULL;
  }
  LOG(INFO) << "FromDictionaryValue: last update = " << last_update_iso8601;
  base::Time last_update;
  bool local = policy->ZonelessTimeStringsAreLocal();
  if (!TimeFromIso8601(last_update_iso8601, &last_update, local)) {
    LOG(WARNING) << "FromDictionaryValue: could not convert last update time";
    return NULL;
  }

  std::string plan_name;
  if (!value->GetString(kCrosUsageDataPlanNameProperty, &plan_name)) {
    LOG(WARNING) << "FromDictionaryValue: no plan name property";
    return NULL;
  }
  LOG(INFO) << "FromDictionaryValue: plan name = " << plan_name;

  std::string plan_type_string;
  if (!value->GetString(kCrosUsageDataPlanTypeProperty, &plan_type_string)) {
    LOG(WARNING) << "FromDictionaryValue: no plan type property";
    return NULL;
  }
  LOG(INFO) << "FromDictionaryValue: plan type string = " << plan_type_string;
  Type plan_type;
  if (!CrosUsageStringToType(plan_type_string, &plan_type)) {
    LOG(WARNING) << "FromDictionaryValue: invalid plan type string: "
        << plan_type_string;
    return NULL;
  }
  LOG(INFO) << "FromDictionaryValue: converted plan type string to type "
      << plan_type;

  int64 max_bytes = 0;
  // we only expect max bytes for metered plans
  if (plan_type != kTypeUnlimited) {
    if (!GetInt64FromDictionary(*value, kCrosUsageDataPlanMaxBytesProperty,
                                &max_bytes)) {
      LOG(WARNING) << "FromDictionaryValue: no max bytes property";
      return NULL;
    }
    if (max_bytes < 0) {
      LOG(WARNING) << "FromDictionaryValue: max bytes is negative";
      return NULL;
    }
    LOG(INFO) << "FromDictionaryValue: max bytes = " << max_bytes;
  }

  int64 used_bytes = 0;
  if (!GetInt64FromDictionary(*value, kCrosUsageDataPlanUsedBytesProperty,
                              &used_bytes)) {
    // used bytes is required for metered plans, optional for unlimited plans
    if (plan_type != kTypeUnlimited) {
      LOG(WARNING) << "FromDictionaryValue: no used bytes property";
      return NULL;
    } else {
      LOG(INFO)
          << "FromDictionaryValue: no used bytes property (using default of 0)";
    }
  }
  if (used_bytes < 0) {
    LOG(WARNING) << "FromDictionaryValue: used bytes is negative";
    return NULL;
  }
  LOG(INFO) << "FromDictionaryValue: used bytes = " << used_bytes;

  std::string start_time_iso8601;
  if (!value->GetString(kCrosUsageDataPlanStartTimeProperty,
                        &start_time_iso8601)) {
    LOG(WARNING) << "FromDictionaryValue: no start time property";
    return NULL;
  }
  LOG(INFO) << "FromDictionaryValue: start time = " << start_time_iso8601;
  base::Time start_time;
  if (!TimeFromIso8601(start_time_iso8601, &start_time, local)) {
    LOG(WARNING) << "FromDictionaryValue: could not convert start time";
    return NULL;
  }

  std::string end_time_iso8601;
  if (!value->GetString(kCrosUsageDataPlanEndTimeProperty,
                        &end_time_iso8601)) {
    LOG(WARNING) << "FromDictionaryValue: no end time property";
    return NULL;
  }
  LOG(INFO) << "FromDictionaryValue: end time = " << end_time_iso8601;
  base::Time end_time;
  if (!TimeFromIso8601(end_time_iso8601, &end_time, local)) {
    LOG(WARNING) << "FromDictionaryValue: could not convert end time";
    return NULL;
  }

  if (start_time > end_time) {
    LOG(WARNING) << "FromDictionaryValue: start time > end time";
    return NULL;
  }

  return new(std::nothrow) DataPlan(plan_name, plan_type, last_update,
                                    start_time, end_time, max_bytes,
                                    used_bytes);
}

// static
bool DataPlan::TimeFromIso8601(const std::string& time_8601,
                               base::Time *time_out,
                               bool zoneless_strings_are_local) {
  CHECK_NOTNULL(time_out);
  static const char *kIso8601UtcFormat = "%Y-%m-%dT%H:%M:%SZ";
  static const char *kIso8601ZonelessFormat = "%Y-%m-%dT%H:%M:%S";
  struct tm tm;
  time_t tt;
  bool tm_is_utc;

  // convert from ISO 8601 string to struct tm
  // first try the expected format (ISO 8601 with UTC 'Z' suffix)
  // if that fails, try format with no timezone suffix
  // we'll interpret this as either UTC or local time based on input flag
  // TODO(vlaviano): support +/-nn:nn timezone specifier
  memset(&tm, 0, sizeof(tm));
  if (strptime(time_8601.c_str(), kIso8601UtcFormat, &tm) != NULL) {
    tm_is_utc = true;
  } else if (strptime(time_8601.c_str(), kIso8601ZonelessFormat, &tm) != NULL) {
    if (zoneless_strings_are_local) {
      tm_is_utc = false;
    } else {
      tm_is_utc = true;
    }
  } else {
    LOG(WARNING) << "TimeFromIso8601: strptime failed for time string: "
        << time_8601;
    return false;
  }
  tm.tm_isdst = -1;

  // convert from struct tm to time_t
  if (tm_is_utc) {
    // NOTE: timegm interprets its input as being in UTC.
    if ((tt = timegm(&tm)) == -1) {
      LOG(WARNING) << "TimeFromIso8601: timegm failed";
      return false;
    }
  } else {
    // NOTE: mktime interprets its input as being in local time.
    if ((tt = mktime(&tm)) == -1) {
      LOG(WARNING) << "TimeFromIso8601: mktime failed";
      return false;
    }
  }

  // convert from time_t to base::Time
  *time_out = base::Time::FromTimeT(tt);
  return true;
}

// static
DataPlan* DataPlan::GetActivePlan(const DataPlanList& data_plans) {
  DataPlanList::const_iterator it;
  for (it = data_plans.begin(); it != data_plans.end(); ++it) {
    DataPlan *plan = *it;
    DCHECK(plan != NULL);
    if (plan->IsActive()) {
      return plan;
    }
  }
  return NULL;
}

// static
ByteCount DataPlan::AssignBytesToPlan(DataPlan *data_plan,
                                      ByteCount bytes_to_assign) {
  CHECK(data_plan != NULL);
  CHECK_GE(bytes_to_assign, 0);

  // we cannot assign any bytes to |data_plan| if it is inactive
  if (!data_plan->IsActive()) {
    LOG(INFO) << "AssignBytesToPlan: cannot assign bytes to inactive plan: "
        << data_plan->GetName();
    return 0;
  }

  ByteCount assigned_bytes = 0;
  ByteCount data_remaining = 0;  // relevant for metered plans
  if (data_plan->GetType() != DataPlan::kTypeUnlimited) {
    data_remaining = data_plan->GetDataBytesMax() -
        data_plan->GetTotalBytesUsed();
    CHECK_GE(data_remaining, 0);
  }
  if (data_plan->GetType() == DataPlan::kTypeUnlimited ||
      bytes_to_assign < data_remaining) {
    // can assign all the bytes to |data_plan|
    assigned_bytes = bytes_to_assign;
  } else {
    assigned_bytes = data_remaining;
  }
  CHECK_GE(assigned_bytes, 0);
  data_plan->SetLocalBytesUsed(data_plan->GetLocalBytesUsed() + assigned_bytes);

  LOG(INFO) << "AssignBytesToPlan: " << data_plan->GetName()
      << ": updated plan state: data bytes used = "
      << data_plan->GetDataBytesUsed() << ", local bytes used = "
      << data_plan->GetLocalBytesUsed() << ", total bytes used = "
      << data_plan->GetTotalBytesUsed();

  return assigned_bytes;
}

// static
bool DataPlan::OnByteCounterUpdate(DataPlanList *data_plans, Service *service,
                                   ServiceManager *service_manager,
                                   Device *device, uint64 delta_rx_bytes,
                                   uint64 delta_tx_bytes) {
  DCHECK(data_plans != NULL);
  DCHECK(service != NULL);
  DCHECK(service_manager != NULL);
  DCHECK(device != NULL);

  LOG(INFO) << "OnByteCounterUpdate: delta_rx_bytes = " << delta_rx_bytes
      << ", delta_tx_bytes = " << delta_tx_bytes;

  DataPlan *active_plan = DataPlan::GetActivePlan(*data_plans);
  if (active_plan == NULL) {
    LOG(WARNING) << "OnByteCounterUpdate: no active plan";
    return false;
  }

  ByteCount used_bytes_to_assign = delta_rx_bytes + delta_tx_bytes;
  // try to detect two error conditions:
  // (1) overflow of the unsigned addition above prior to the assignment
  // (2) no overflow, but result has high order bit set and so is interpreted as
  //     a negative number when assigned to |local_bytes_used|
  if (static_cast<uint64>(used_bytes_to_assign) < delta_rx_bytes ||
      static_cast<uint64>(used_bytes_to_assign) < delta_tx_bytes ||
      used_bytes_to_assign < 0) {
    LOG(WARNING) << "OnByteCounterUpdate: overflow detected:"
        " |used_bytes_to_assign|";
    return false;
  }
  LOG(INFO) << "OnByteCounterUpdate: used_bytes_to_assign = "
      << used_bytes_to_assign;

  // iterate through active plans, assigning as much data as possible to each
  // plan and expiring it if its data quota is fully consumed
  bool any_plan_updated = false;
  while (used_bytes_to_assign > 0 && active_plan != NULL) {
    ByteCount bytes_assigned_to_active_plan = AssignBytesToPlan(active_plan,
        used_bytes_to_assign);
    CHECK_LE(bytes_assigned_to_active_plan, used_bytes_to_assign);
    if (bytes_assigned_to_active_plan > 0) {
      used_bytes_to_assign -= bytes_assigned_to_active_plan;
      any_plan_updated = true;
    }

    if (!active_plan->HasBytesRemaining()) {
      // notify service manager that plan has become exhausted
      service_manager->OnDataPlanInactive(*service, *active_plan);
      active_plan = DataPlan::GetActivePlan(*data_plans);
    }
  }

  // if we have no more active plans, stop our local counter
  if (active_plan == NULL) {
    device->StopByteCounter();
    // warn if we've measured usage beyond what should have been available
    if (used_bytes_to_assign > 0) {
      LOG(WARNING) << "OnByteCounterUpdate: used_bytes_to_assign = "
          << used_bytes_to_assign << ", but no active plan";
    }
  }
  return any_plan_updated;
}

// private methods

bool DataPlan::IsCurrent() const {
  base::Time now = base::Time::Now();
  return now >= start_time_ && now < end_time_;
}

bool DataPlan::HasBytesRemaining() const {
  return type_ == kTypeUnlimited || total_bytes_used_ < data_bytes_max_;
}

void DataPlan::OnDataPlanExpired() {
  CHECK(service_ != NULL);

  if (!service_->OnDataPlanExpired(this)) {
    LOG(WARNING) << "OnDataPlanExpired: failed to handle expired data plan";
  }
}

// static
gboolean DataPlan::StaticOnDataPlanExpired(gpointer data) {
  DataPlan *data_plan = reinterpret_cast<DataPlan*>(data);
  CHECK(data_plan != NULL);
  data_plan->OnDataPlanExpired();
  return FALSE;  // don't call back
}

bool DataPlan::CreateExpirationTimer() {
  if (expiration_timeout_source_ != NULL) {
    LOG(WARNING) << "CreateExpirationTimer: timer already running";
    return false;
  }

  base::TimeDelta timeout = end_time_ - base::Time::Now();
  expiration_timeout_source_ = g_timeout_source_new_seconds(
      timeout.InSeconds());
  if (expiration_timeout_source_ == NULL) {
    LOG(WARNING) << "CreateExpirationTimer: failed to create timeout source";
    return false;
  }

  g_source_set_callback(expiration_timeout_source_, StaticOnDataPlanExpired,
                        this, NULL);
  if (g_source_attach(expiration_timeout_source_, NULL) == 0) {
    LOG(WARNING) << "CreateExpirationTimer: failed to attach timeout source";
    return false;
  }

  return true;
}

void DataPlan::DestroyExpirationTimer() {
  if (expiration_timeout_source_ != NULL) {
    g_source_destroy(expiration_timeout_source_);
    expiration_timeout_source_ = NULL;
  }
}

const char* DataPlan::TypeToLibcrosString(DataPlan::Type type) const {
  switch (type) {
    case kTypeUnlimited:
      return kCellularDataPlanTypeUnlimited;
    case kTypeMeteredFree:
      return kCellularDataPlanTypeMeteredFree;
    case kTypeMeteredPaid:
      return kCellularDataPlanTypeMeteredPaid;
    default:
      CHECK(false);
  }
}

// static
bool DataPlan::CrosUsageStringToType(const std::string& type_string,
                                     DataPlan::Type *type_out) {
  CHECK_NOTNULL(type_out);
  if (type_string == kCrosUsageDataPlanTypeUnlimited) {
    *type_out = kTypeUnlimited;
  } else if (type_string == kCrosUsageDataPlanTypeMeteredFree) {
    *type_out = kTypeMeteredFree;
  } else if (type_string == kCrosUsageDataPlanTypeMeteredPaid) {
    *type_out = kTypeMeteredPaid;
  } else {
    return false;
  }
  return true;
}

// static
bool DataPlan::GetInt64FromDictionary(const DictionaryValue& dict,
                                      const std::string& key,
                                      int64* out_value) {
  CHECK_NOTNULL(out_value);
  int generic_int_out = 0;
  if (dict.GetInteger(key, &generic_int_out)) {
    *out_value = generic_int_out;
    return true;
  }
  double generic_double_out = 0;
  if (dict.GetDouble(key, &generic_double_out)) {
    *out_value = generic_double_out;
    return true;
  }
  return false;
}

}  // namespace cashew
