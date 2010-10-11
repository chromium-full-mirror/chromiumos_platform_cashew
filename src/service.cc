// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/service.h"

#include <glog/logging.h>

#include "src/data_plan_provider.h"
#include "src/device.h"
#include "src/policy.h"
#include "src/service_manager.h"

namespace cashew {

// Flimflam Service D-Bus identifiers
static const char *kFlimflamServiceName = "org.chromium.flimflam";

// Flimflam Service property names
static const char *kFlimflamServiceDeviceProperty = "Device";
static const char *kFlimflamServiceStateProperty = "State";
static const char *kFlimflamServiceTypeProperty = "Type";
static const char *kFlimflamServiceUsageUrlProperty = "Cellular.UsageUrl";

// Flimflam Service on-the-wire State values
static const char *kFlimflamServiceStateIdle = "idle";
static const char *kFlimflamServiceStateCarrier = "carrier";
static const char *kFlimflamServiceStateAssociation = "association";
static const char *kFlimflamServiceStateConfiguration = "configuration";
static const char *kFlimflamServiceStateReady = "ready";
static const char *kFlimflamServiceStateDisconnect = "disconnect";
static const char *kFlimflamServiceStateFailure = "failure";
static const char *kFlimflamServiceStateActivationFailure =
    "activation-failure";

// Flimflam Service on-the-wire Type values
static const char *kFlimflamServiceTypeEthernet = "ethernet";
static const char *kFlimflamServiceTypeWifi = "wifi";
static const char *kFlimflamServiceTypeWimax = "wimax";
static const char *kFlimflamServiceTypeBluetooth = "bluetooth";
static const char *kFlimflamServiceTypeCellular = "cellular";

// Chromium OS Usage API property names
static const char *kCrosUsageVersionProperty = "version";
static const char *kCrosUsageStatusProperty = "status";
static const char *kCrosUsageRestrictedProperty = "restricted";
static const char *kCrosUsagePlansProperty = "plans";

// Chromium OS Usage API version values
static const int kCrosUsageVersionMinSupported = 1;
static const int kCrosUsageVersionMaxSupported = 1;

// Chromium OS Usage API status values
static const char *kCrosUsageStatusOk = "OK";
static const char *kCrosUsageStatusError = "ERROR";

Service::Service(ServiceManager * const parent,
                 DBus::Connection& connection,  // NOLINT
                 const DBus::Path& path)
    : DBus::ObjectProxy(connection, path, kFlimflamServiceName),
      parent_(CHECK_NOTNULL(parent)), connection_(connection), path_(path),
      state_(kStateUnknown), type_(kTypeUnknown), device_(NULL),
      provider_(NULL), request_in_progress_(false),
      update_timeout_source_(NULL), policy_(NULL) {
  // init our state with a GetProperties() call to our Flimflam service path
  // we'll update this state by monitoring PropertyChanged signals
  GetServiceProperties();
}

Service::~Service() {
  DeleteCarrierState();
  DeleteDataPlans(&data_plans_);
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
  } else if (property_name.compare(kFlimflamServiceUsageUrlProperty) == 0) {
    OnUsageUrlUpdate(new_value.reader().get_string());
  } else {
    // we don't care about this property
  }
}

// Device methods

void Service::OnCarrierUpdate(const std::string& carrier) {
  DLOG(INFO) << path_ << ": OnCarrierUpdate: carrier = " << carrier;
  // NOTE: device_ can be NULL here because we may still be in Device ctor
  DCHECK(device_ == NULL || carrier.compare(device_->GetCarrier()) == 0);

  // get rid of state associated with old carrier
  if (provider_ != NULL) {
    DCHECK(carrier.compare(provider_->GetCarrier()));
  }
  DeleteCarrierState();

  // if we don't have new carrier info, we can't do anything now
  if (carrier.compare(Device::kCarrierUnknown) == 0) {
    return;
  }

  // create state for new carrier
  DLOG(INFO) << path_ << ": OnCarrierUpdate: creating data plan provider";
  provider_ = new(std::nothrow) DataPlanProvider(carrier);
  if (provider_ == NULL) {
    LOG(ERROR) << path_
        << ": OnCarrierUpdate: could not create data plan provider";
    return;
  }
  provider_->SetDelegate(this);
  DLOG(INFO) << path_ << ": OnCarrierUpdate: getting policy for " << carrier;
  policy_ = Policy::GetPolicy(carrier);
  if (policy_ == NULL) {
    LOG(ERROR) << path_ << ": OnCarrierUpdate: could not get policy for "
        << carrier;
    DeleteCarrierState();
    return;
  }

  // if we already have usage url, set new provider in motion
  if (!usage_url_.empty()) {
    provider_->SetUsageUrl(usage_url_);
    RequestUsageUpdate();
    CreateUpdateTimer();
  } else {
    // we'll do this later in OnUsageUrlUpdate
  }
}

// DataPlanProviderDelegate methods

void Service::OnRequestComplete(const DataPlanProvider *provider,
                                bool successful,
                                const Value *parsed_usage_update) {
  DCHECK(provider != NULL);
  DCHECK(!successful || parsed_usage_update != NULL);
  if (provider != provider_) {
    DLOG(WARNING) <<  path_ << ": OnRequestComplete: wrong provider";
    return;
  }
  DCHECK(request_in_progress_);
  DCHECK(policy_ != NULL);
  DLOG(INFO) << path_ << ": OnRequestComplete: result = " << successful;
  request_in_progress_ = false;
  if (!successful) {
    LOG(WARNING) << path_ << ": OnRequestComplete: request failed";
    return;
  }

  // interpret and validate parsed usage update
  if (!parsed_usage_update->IsType(Value::TYPE_DICTIONARY)) {
    LOG(WARNING) << path_
        << ": OnRequestComplete: root value is not a dictionary";
    return;
  }
  const DictionaryValue *root =
      static_cast<const DictionaryValue*>(parsed_usage_update);

  int version;
  if (!root->GetInteger(kCrosUsageVersionProperty, &version)) {
    LOG(WARNING) << path_ << ": OnRequestComplete: no version property";
    return;
  }
  DLOG(INFO) << path_ << ": OnRequestComplete: version = " << version;
  if (version < kCrosUsageVersionMinSupported ||
      version > kCrosUsageVersionMaxSupported) {
    LOG(WARNING) << path_ << ": OnRequestComplete: version " << version
        << " not in supported range of [" << kCrosUsageVersionMinSupported
        << ", " << kCrosUsageVersionMaxSupported << "]";
    return;
  }

  std::string status;
  if (!root->GetString(kCrosUsageStatusProperty, &status)) {
    LOG(WARNING) << path_ << ": OnRequestComplete: no status property";
    return;
  }
  DLOG(INFO) << path_ << ": OnRequestComplete: status = " << status;
  if (status != kCrosUsageStatusOk && status != kCrosUsageStatusError) {
    LOG(WARNING) << path_ << ": OnRequestComplete: invalid status: " << status;
    return;
  }
  if (status == kCrosUsageStatusError) {
    LOG(WARNING) << path_ << ": OnRequestComplete: status: " << status;
    return;
  }

  bool restricted = false;
  if (root->GetBoolean(kCrosUsageRestrictedProperty, &restricted)) {
    DLOG(INFO) << path_ << ": OnRequestComplete: restricted = " << restricted;
  } else {
    DLOG(INFO) << path_ << ": OnRequestComplete: no restricted property";
    // restricted property is optional
  }

  ListValue *plans = NULL;  // list to which this points is owned by dictionary
  if (!root->GetList(kCrosUsagePlansProperty, &plans)) {
    LOG(WARNING) << path_ << ": OnRequestComplete: no plans property";
    return;
  }
  DCHECK(plans != NULL);
  size_t plans_size = plans->GetSize();
  DLOG(INFO) << path_ << ": OnRequestComplete: plans list has size "
    << plans_size;

  // walk plans list and convert each into a DataPlan object
  DataPlanList new_plans;
  for (size_t i = 0; i < plans_size; ++i) {
    DictionaryValue *plan_dict = NULL;
    if (!plans->GetDictionary(i, &plan_dict)) {
      LOG(WARNING) << path_
          << ": OnRequestComplete: could not get plan[" << i << "]";
      continue;
    }
    DCHECK(plan_dict != NULL);
    DataPlan *plan = DataPlan::FromDictionaryValue(plan_dict);
    if (plan == NULL) {
      LOG(WARNING) << path_
          << ": OnRequestComplete: could not convert plan[" << i << "]";
      continue;
    }
    DLOG(INFO) << path_ << ": OnRequestComplete: converted plan[" << i << "]";
    new_plans.push_back(plan);
  }

  // store new info, causing it to be used for subsequent updates to clients
  DataPlanList old_plans = data_plans_;
  data_plans_ = new_plans;
  LOG(INFO) << path_ << ": OnRequestComplete: updated data plans";

  // consult policy to determine if we should send an unsolicited update
  if (policy_->ShouldEmitDataPlansUpdate(old_plans, new_plans)) {
    DLOG(INFO) << path_ << ": OnRequestComplete: sending update";
    parent_->EmitDataPlansUpdate(*this);
  } else {
    DLOG(INFO) << path_ << ": OnRequestComplete: not sending update";
  }

  DLOG(INFO) << path_ << ": OnRequestComplete: deleting old data plans";
  DeleteDataPlans(&old_plans);
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
  device_ = new(std::nothrow) Device(this, connection_, device_path);
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

void Service::OnUsageUrlUpdate(const std::string& usage_url) {
  DLOG(INFO) << path_ << ": OnUsageUrlUpdate: url = " << usage_url;
  if (usage_url.compare(usage_url_) == 0) {
    return;
  }
  usage_url_ = usage_url;
  if (provider_ != NULL) {
    DCHECK(usage_url.compare(provider_->GetUsageUrl()));
    DestroyUpdateTimer();
    CancelPendingRequests();
    provider_->SetUsageUrl(usage_url);
    RequestUsageUpdate();
    CreateUpdateTimer();
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
  // don't expect to find Cellular.* properties if we're not a cellular service
  if (type_ != kTypeCellular) {
    return;
  }
  it = properties.find(kFlimflamServiceUsageUrlProperty);
  if (it != properties.end()) {
    const DBus::Variant& value = static_cast<DBus::Variant>(it->second);
    OnUsageUrlUpdate(value.reader().get_string());
  } else {
    DLOG(WARNING) << path_
        << ": GetServiceProperties: no Cellular.UsageUrl property";
  }
}

void Service::DeleteDataPlans(DataPlanList *data_plans) {
  DCHECK(data_plans != NULL);
  while (!data_plans->empty()) {
    DataPlan *plan = *data_plans->begin();
    DCHECK(plan != NULL);
    DLOG(INFO) << path_ << ": DeleteDataPlans: deleting plan: "
        << plan->GetName();
    delete plan;
    data_plans->erase(data_plans->begin());
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

void Service::RequestUsageUpdate() {
  DCHECK(provider_ != NULL);
  DCHECK(!usage_url_.empty());
  DCHECK(device_ == NULL ||
         device_->GetCarrier().compare(Device::kCarrierUnknown));

  if (request_in_progress_) {
    return;
  }

  // TODO(vlaviano): apply policy to determine if current connectivity state
  // allows this request (e.g., if we can only make API requests over the
  // cellular service itself, is this service the default route?)

  if (!provider_->RequestUsageUpdate()) {
    DLOG(WARNING) << path_ << ": RequestUsageUpdate: request failed";
  } else {
    DLOG(INFO) << path_ << ": RequestUsageUpdate: request succeeded";
    request_in_progress_ = true;
  }
}

void Service::CancelPendingRequests() {
  if (provider_ != NULL && request_in_progress_) {
    provider_->CancelPendingRequests();
    request_in_progress_ = false;
  }
}

gboolean Service::UpdateTimeoutCallback() {
  DLOG(INFO) << path_ << ": UpdateTimeoutCallback";
  RequestUsageUpdate();
  return TRUE;
}

// static
gboolean Service::StaticUpdateTimeoutCallback(gpointer data) {
  CHECK_NOTNULL(data);
  return reinterpret_cast<Service*>(data)->UpdateTimeoutCallback();
}

bool Service::CreateUpdateTimer() {
  DCHECK(provider_ != NULL);
  DCHECK(policy_ != NULL);
  guint idle_seconds = policy_->GetUpdateTimerIdleSecs(data_plans_);
  DLOG(INFO) << path_ << ": CreateUpdateTimer: idle_seconds = " << idle_seconds;
  if (update_timeout_source_ != NULL) {
    DLOG(WARNING) << path_ << ": CreateUpdateTimer: timer already running";
    return false;
  }
  update_timeout_source_ = g_timeout_source_new_seconds(idle_seconds);
  if (update_timeout_source_ == NULL) {
    DLOG(WARNING) << path_
        << ": CreateUpdateTimer: could not create timeout source";
    return false;
  }
  g_source_set_callback(update_timeout_source_, StaticUpdateTimeoutCallback,
                        this, NULL);
  g_source_attach(update_timeout_source_, NULL);
  return true;
}

void Service::DestroyUpdateTimer() {
  DLOG(INFO) << path_ << ": DestroyUpdateTimer";
  if (update_timeout_source_ != NULL) {
    g_source_destroy(update_timeout_source_);
    update_timeout_source_ = NULL;
  }
}

void Service::DeleteCarrierState() {
  DLOG(INFO) << path_ << ": DeleteCarrierState";
  if (policy_ != NULL) {
    DLOG(INFO) << path_ << ": DeleteCarrierState: deleting policy";
    delete policy_;
    policy_ = NULL;
  }
  DestroyUpdateTimer();
  CancelPendingRequests();
  if (provider_ != NULL) {
    DLOG(INFO) << path_ << ": DeleteCarrierState: deleting data plan provider";
    delete provider_;
    provider_ = NULL;
  }
}

}  // namespace cashew
