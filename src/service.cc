// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/service.h"

#include <glog/logging.h>

#include "src/device.h"

namespace cashew {

// Flimflam Service D-Bus identifiers
static const char* kFlimflamServiceName = "org.chromium.flimflam";

// Flimflam Service property names
static const char *kFlimflamServiceDeviceProperty = "Device";
static const char *kFlimflamServiceStateProperty = "State";
static const char* kFlimflamServiceTypeProperty = "Type";

// Flimflam Service on-the-wire State values
static const char* kFlimflamServiceStateIdle = "idle";
static const char* kFlimflamServiceStateCarrier = "carrier";
static const char* kFlimflamServiceStateAssociation = "association";
static const char* kFlimflamServiceStateConfiguration = "configuration";
static const char* kFlimflamServiceStateReady = "ready";
static const char* kFlimflamServiceStateDisconnect = "disconnect";
static const char* kFlimflamServiceStateFailure = "failure";
static const char* kFlimflamServiceStateActivationFailure =
    "activation-failure";

// Flimflam Service on-the-wire Type values
static const char* kFlimflamServiceTypeEthernet = "ethernet";
static const char* kFlimflamServiceTypeWifi = "wifi";
static const char* kFlimflamServiceTypeWimax = "wimax";
static const char* kFlimflamServiceTypeBluetooth = "bluetooth";
static const char* kFlimflamServiceTypeCellular = "cellular";

Service::Service(DBus::Connection& connection,  // NOLINT
                 const DBus::Path& path)
    : DBus::ObjectProxy(connection, path, kFlimflamServiceName),
      connection_(connection), path_(path), state_(kStateUnknown),
      type_(kTypeUnknown), device_(NULL) {
  // init our state with a GetProperties() call to our Flimflam service path
  // we'll update this state by monitoring PropertyChanged signals
  GetServiceProperties();

  // init our data plans list with a hardcoded plan
  // TODO(vlaviano): implement backend and return real data
  AddHardcodedDataPlan();
}

Service::~Service() {
  DeleteDataPlans();
  if (device_ != NULL) {
    DLOG(INFO) << path_ << ": deleting device " << device_->GetPath();
    delete device_;
    device_ = NULL;
  }
}

const DBus::Path& Service::GetPath() const {
  return path_;
}

Service::State Service::GetState() const {
  return state_;
}

Service::Type Service::GetType() const {
  return type_;
}

Device* Service::GetDevice() const {
  return device_;
}

DBusDataPlanList Service::GetDBusDataPlans() const {
  DBusDataPlanList dbus_data_plans;
  DataPlanList::const_iterator it;
  for (it = data_plans_.begin(); it != data_plans_.end(); ++it) {
    DataPlan *plan = *it;
    DCHECK(plan != NULL);
    dbus_data_plans.push_back(plan->ToDBusFormat());
  }
  return dbus_data_plans;
}

// Flimflam Service D-Bus Proxy methods

void Service::PropertyChanged(const std::string& property_name,
                              const DBus::Variant& new_value) {
  DLOG(INFO) << path_ << ": PropertyChanged: property_name = " << property_name;
  if (property_name.compare(kFlimflamServiceDeviceProperty) == 0) {
    OnDeviceUpdate(new_value.reader().get_path());
  } else if (property_name.compare(kFlimflamServiceStateProperty) == 0) {
    OnStateUpdate(new_value.reader().get_string());
  } else if (property_name.compare(kFlimflamServiceTypeProperty) == 0) {
    OnTypeUpdate(new_value.reader().get_string());
  } else {
    // we don't care about this property
  }
}

// Private methods

Service::State Service::StateFromString(const std::string& state) const {
  if (state.compare(kFlimflamServiceStateIdle) == 0) {
    return kStateIdle;
  }
  if (state.compare(kFlimflamServiceStateCarrier) == 0) {
    return kStateCarrier;
  }
  if (state.compare(kFlimflamServiceStateAssociation) == 0) {
    return kStateAssociation;
  }
  if (state.compare(kFlimflamServiceStateConfiguration) == 0) {
    return kStateConfiguration;
  }
  if (state.compare(kFlimflamServiceStateReady) == 0) {
    return kStateReady;
  }
  if (state.compare(kFlimflamServiceStateDisconnect) == 0) {
    return kStateDisconnect;
  }
  if (state.compare(kFlimflamServiceStateFailure) == 0) {
    return kStateFailure;
  }
  if (state.compare(kFlimflamServiceStateActivationFailure) == 0) {
    return kStateActivationFailure;
  }
  return kStateUnknown;
}

Service::Type Service::TypeFromString(const std::string& type) const {
  if (type.compare(kFlimflamServiceTypeEthernet) == 0) {
    return kTypeEthernet;
  }
  if (type.compare(kFlimflamServiceTypeWifi) == 0) {
    return kTypeWifi;
  }
  if (type.compare(kFlimflamServiceTypeWimax) == 0) {
    return kTypeWimax;
  }
  if (type.compare(kFlimflamServiceTypeBluetooth) == 0) {
    return kTypeBluetooth;
  }
  if (type.compare(kFlimflamServiceTypeCellular) == 0) {
    return kTypeCellular;
  }
  return kTypeUnknown;
}

void Service::OnDeviceUpdate(const DBus::Path& device_path) {
  DLOG(INFO) << path_ << ": OnDeviceUpdate: device_path = " << device_path;

  // if there's an existing device with a non-matching path: destroy it and
  // fall through to make a new one below.
  if (device_ != NULL && device_->GetPath().compare(device_path)) {
    LOG(WARNING) << path_ << ": OnDeviceUpdate: device path changed from "
        << device_->GetPath() << " to " << device_path;
    delete device_;
    device_ = NULL;
  }

  // if there's an existing device with matching path: all is well
  if (device_ != NULL) {
    return;
  }

  // if there's no existing device: make one
  device_ = new(std::nothrow) Device(connection_, device_path);
  if (device_ == NULL) {
    LOG(ERROR) << path_ << ": OnDeviceUpdate: couldn't create device for "
        << device_path;
    return;
  }
  DLOG(INFO) << path_ << ": OnDeviceUpdate: created device for "
      << device_path;
}

void Service::OnStateUpdate(const std::string& state) {
  DLOG(INFO) << path_ << ": OnStateUpdate: state = " << state;
  state_ = StateFromString(state);
  // TODO(vlaviano): react to state changes
  // note that ready does not necessarily mean that we're the default route
}

void Service::OnTypeUpdate(const std::string& type) {
  DLOG(INFO) << path_ << ": OnTypeUpdate: type = " << type;
  type_ = TypeFromString(type);
  if (type_ != kTypeCellular) {
    LOG(WARNING) << path_ << ": OnTypeUpdate: type is not cellular!";
  }
}

void Service::GetServiceProperties() {
  DLOG(INFO) << path_ << ": GetServiceProperties";
  PropertyMap properties;
  // dbus-c++ throws exceptions
  // invoke the "Existing Non-conformant Code" clause of the style guide and
  // isolate the rest of the system from this
  try {
    // TODO(vlaviano): make this call asynchronous
    properties = GetProperties();
  } catch (DBus::Error& error) {  // NOLINT
    LOG(WARNING) << path_
        << ": GetServiceProperties: GetProperties() -> Exception: "
        << error.name() << ": " << error.message();
    // TODO(vlaviano): schedule another attempt later
    return;
  } catch (...) {  // NOLINT
    LOG(WARNING) << path_
        << ": GetServiceProperties: GetProperties() -> Exception";
    // TODO(vlaviano): schedule another attempt later
    return;
  }
  DLOG(INFO) << "Received " << properties.size() << " properties";

  // grab the properties in which we're interested
  PropertyMap::const_iterator it;
  it = properties.find(kFlimflamServiceDeviceProperty);
  if (it != properties.end()) {
    const DBus::Variant& value = static_cast<DBus::Variant>(it->second);
    OnDeviceUpdate(value.reader().get_path());
  } else {
    DLOG(WARNING) << path_ << ": GetServiceProperties: no Device property";
  }
  it = properties.find(kFlimflamServiceStateProperty);
  if (it != properties.end()) {
    const DBus::Variant& value = static_cast<DBus::Variant>(it->second);
    OnStateUpdate(value.reader().get_string());
  } else {
    DLOG(WARNING) << path_ << ": GetServiceProperties: no State property";
  }
  it = properties.find(kFlimflamServiceTypeProperty);
  if (it != properties.end()) {
    const DBus::Variant& value = static_cast<DBus::Variant>(it->second);
    OnTypeUpdate(value.reader().get_string());
  } else {
    DLOG(WARNING) << path_ << ": GetServiceProperties: no Type property";
  }
}

void Service::DeleteDataPlans() {
  while (!data_plans_.empty()) {
    DataPlan *plan = *data_plans_.begin();
    DCHECK(plan != NULL);
    DLOG(INFO) << path_ << ": DeleteDataPlans: deleting plan: "
        << plan->GetName();
    delete plan;
    data_plans_.erase(data_plans_.begin());
  }
}

void Service::AddHardcodedDataPlan() {
  const std::string name = "Chromium OS Test Plan";
  DataPlan::Type type = DataPlan::kTypeMeteredPaid;
  base::Time update_time = base::Time::Now();
  base::TimeDelta twelve_hours = base::TimeDelta::FromHours(12);
  base::Time start_time = base::Time::Now() - twelve_hours;
  base::Time end_time = base::Time::Now() + twelve_hours;
  Bytes data_bytes_max = 100*1024*1024;  // 100 MB
  Bytes data_bytes_used = 30*1024*1024;  // 30 MB

  DataPlan *plan = new(std::nothrow) DataPlan(name, type, update_time,
                                              start_time, end_time,
                                              data_bytes_max, data_bytes_used);
  if (plan == NULL) {
    LOG(ERROR) << path_ << ": AddHardcodedDataPlan: could not create plan";
    return;
  }

  data_plans_.push_back(plan);
}

}  // namespace cashew
