// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(vlaviano): update accessor style (GetFoo/SetFoo -> foo)

#ifndef SRC_DATA_PLAN_H_
#define SRC_DATA_PLAN_H_

#include <map>
#include <string>
#include <vector>

#include <base/basictypes.h>  // NOLINT
#include <base/time.h>  // NOLINT
#include <dbus-c++/dbus.h>  // NOLINT

#include "src/values.h"

namespace cashew {

class DataPlan;
class Device;
class Policy;
class Service;
class ServiceManager;

// byte count
// TODO(vlaviano): this should be a uint64, but libcros/Chrome expects this
typedef int64 ByteCount;

// List of DataPlan objects
// TODO(vlaviano): consider promoting this from a typedef to a class
typedef std::vector<DataPlan*> DataPlanList;

// DataPlan object formatted for D-Bus (using DBus::Variants to wrap fields)
typedef std::map<std::string, DBus::Variant> DBusDataPlan;

// List of DataPlan objects formatted for D-Bus
typedef std::vector<DBusDataPlan> DBusDataPlanList;

// Represents a data plan associated with a cellular service
class DataPlan {
  public:
    // valid type values for this plan
    typedef enum {
      kTypeUnlimited = 0,
      kTypeMeteredFree,
      kTypeMeteredPaid,
    } Type;

    // |data_bytes_max| must be set to 0 for unlimited plans
    DataPlan(const std::string& name, DataPlan::Type type,
             base::Time update_time, base::Time start_time,
             base::Time end_time, ByteCount data_bytes_max,
             ByteCount data_bytes_used);
    virtual ~DataPlan();

    // get plan name
    virtual const std::string& GetName() const;

    // get plan type
    virtual Type GetType() const;

    // get last update time
    virtual base::Time GetUpdateTime() const;

    // get plan start time
    virtual base::Time GetStartTime() const;

    // get plan end time
    virtual base::Time GetEndTime() const;

    // get plan max bytes
    virtual ByteCount GetDataBytesMax() const;

    // get plan used bytes
    virtual ByteCount GetDataBytesUsed() const;

    // get local used bytes
    virtual ByteCount GetLocalBytesUsed() const;

    // set local used bytes
    // must be >= 0
    virtual void SetLocalBytesUsed(ByteCount local_bytes_used);

    // get total used bytes
    virtual ByteCount GetTotalBytesUsed() const;

    // is this an active plan?
    // we define active to mean that the plan is current and not exhausted
    virtual bool IsActive() const;

    // return a{sv} representation for D-Bus
    virtual DBusDataPlan ToDBusFormat() const;

    // construct DataPlan from DictionaryValue
    static DataPlan* FromDictionaryValue(const DictionaryValue *value,
                                         const Policy *policy);

    // convert ISO 8601 time value to base::Time
    // returns true on success and false on failure
    // |time_out| must not be NULL and is set on a successful return
    static bool TimeFromIso8601(const std::string& time_8601,
                                base::Time *time_out,
                                bool zoneless_strings_are_local);

    // returns first active plan in |data_plans|
    // returns NULL if list is empty or contains no active plans
    static DataPlan* GetActivePlan(const DataPlanList& data_plans);

    // event handler for |service's| local byte counter on |device|
    // updates active plans in |data_plans| based on the |rx_bytes| and
    //   |tx_bytes| values gathered during the most recent sample interval
    // notifies |service_manager| if an active plan is completely consumed
    // returns true if any plans were updated and false otherwise
    // |data_plans|, |service|, |service_manager| and |device| must not be NULL
    static bool OnByteCounterUpdate(DataPlanList *data_plans, Service *service,
                                    ServiceManager *service_manager,
                                    Device *device, uint64 rx_bytes,
                                    uint64 tx_bytes);

  private:
    // human-readable plan name
    const std::string name_;

    // plan type
    const Type type_;

    // last update time (i.e., how fresh is plan data)
    // this refers to carrier API data, not local measurements
    base::Time update_time_;

    // plan start time
    const base::Time start_time_;

    // plan end time
    const base::Time end_time_;

    // max bytes available during plan period
    // irrelevant for plans of type kTypeUnlimited
    const ByteCount data_bytes_max_;

    // bytes consumed so far during plan period
    // this measurement comes from the carrier API
    ByteCount data_bytes_used_;

    // local measurement of bytes used since we received |data_bytes_used_|
    // baseline from the carrier API
    ByteCount local_bytes_used_;

    // our best estimate of total bytes used for this plan
    // carrier baseline (|data_bytes_used_|) +
    // locally measured traffic (|local_bytes_used_|)
    ByteCount total_bytes_used_;

    // convert plan type enum value to string formatted for libcros
    const char* TypeToLibcrosString(Type type) const;

    // convert string formatted for Cros Usage API to plan type enum value
    // returns true on success and false on failure
    // |type_out| must not be NULL and is set on a successful return
    static bool CrosUsageStringToType(const std::string& type_string,
                                      Type *type_out);

    // converts the value associated with |key| in |dict| to an int64
    // returns true on success and false on failure
    // |out_value| must not be NULL and is set on a successful return
    static bool GetInt64FromDictionary(const DictionaryValue& dict,
                                       const std::string& key,
                                       int64* out_value);

    DISALLOW_COPY_AND_ASSIGN(DataPlan);
};

}  // namespace cashew

#endif  // SRC_DATA_PLAN_H_
