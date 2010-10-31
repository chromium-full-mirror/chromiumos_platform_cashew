// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
class Policy;

// byte count
// TODO(vlaviano): this should be a uint64, but libcros/Chrome expects this
typedef int64 Bytes;

// List of DataPlan objects
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

    DataPlan(const std::string& name, DataPlan::Type type,
             base::Time update_time, base::Time start_time,
             base::Time end_time, Bytes data_bytes_max, Bytes data_bytes_used);
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
    virtual Bytes GetDataBytesMax() const;

    // get plan used bytes
    virtual Bytes GetDataBytesUsed() const;

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

  private:
    // human-readable plan name
    const std::string name_;

    // plan type
    const Type type_;

    // last update time (i.e., how fresh is plan data)
    base::Time update_time_;

    // plan start time
    const base::Time start_time_;

    // plan end time
    const base::Time end_time_;

    // max bytes available during plan period
    // irrelevant for plans of type kTypeUnlimited
    const Bytes data_bytes_max_;

    // bytes consumed so far during plan period
    Bytes data_bytes_used_;

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
