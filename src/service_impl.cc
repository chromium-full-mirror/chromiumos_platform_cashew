// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/service_impl.h"

#include <glog/logging.h>

#include <iostream>
#include <sstream>
#include <vector>

#include "src/aggregator.h"
#include "src/data_plan.h"
#include "src/data_plan_provider.h"
#include "src/device.h"
#include "src/policy.h"
#include "src/service_manager.h"

using std::vector;

namespace cashew {

// Flimflam Service D-Bus identifiers
static const char *kFlimflamServiceName = "org.chromium.flimflam";

// Flimflam Service property names
static const char *kFlimflamServiceDeviceProperty = "Device";
static const char *kFlimflamServiceStateProperty = "State";
static const char *kFlimflamServiceTypeProperty = "Type";
static const char *kFlimflamServiceUsageUrlProperty = "Cellular.UsageUrl";
static const char *kFlimflamServiceStickyHostRouteProperty = "StickyHostRoute";

// Shill Service property names
static const char kShillServiceHTTPProxyPortProperty[] = "HTTPProxyPort";

// Flimflam Service on-the-wire State values
static const char *kFlimflamServiceStateIdle = "idle";
static const char *kFlimflamServiceStateCarrier = "carrier";
static const char *kFlimflamServiceStateAssociation = "association";
static const char *kFlimflamServiceStateConfiguration = "configuration";
static const char *kFlimflamServiceStateReady = "ready";
static const char *kFlimflamServiceStatePortal = "portal";
static const char *kFlimflamServiceStateOnline = "online";
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
// NOTE: add new values to IsValidCrosUsageStatus below
// NOTE: add new values to Metrics Manager's UsageRequestStatus enum
// NOTE: add new values to MetricsEnumFromStatusString below
// TODO(vlaviano): improve this
static const char *kCrosUsageStatusOk = "OK";
static const char *kCrosUsageStatusError = "ERROR";
static const char *kCrosUsageStatusMalformedRequest = "MALFORMED REQUEST";
static const char *kCrosUsageStatusInternalError = "INTERNAL ERROR";
static const char *kCrosUsageStatusServiceUnavailable = "SERVICE UNAVAILABLE";
static const char *kCrosUsageStatusRequestRefused = "REQUEST REFUSED";
static const char *kCrosUsageStatusUnknownDevice = "UNKNOWN DEVICE";

// GetServiceProperties retry interval
static const guint kSecondsPerMinute = 60;
static const guint kGetServicePropertiesIntervalSeconds = 1 * kSecondsPerMinute;

ServiceImpl::ServiceImpl(ServiceManager * const parent,
                         DBus::Connection& connection,  // NOLINT
                         MetricsManager * const metrics_manager,
                         Aggregator * const aggregator,
                         const DBus::Path& path)
    : DBus::ObjectProxy(connection, path, kFlimflamServiceName),
      parent_(CHECK_NOTNULL(parent)), connection_(connection),
      metrics_manager_(CHECK_NOTNULL(metrics_manager)),
      aggregator_(CHECK_NOTNULL(aggregator)), prev_rx_bytes_(0),
      prev_tx_bytes_(0), path_(path), state_(kStateUnknown),
      type_(kTypeUnknown), device_(NULL), provider_(NULL),
      request_in_progress_(false), usage_request_complete_(false),
      update_timeout_source_(NULL), policy_(NULL),
      is_default_service_(false), get_properties_source_id_(0),
      retrying_get_properties_(false) {
  // schedule a GetProperties() call to our Flimflam service path to init state
  // we'll keep trying periodically until we succeed
  // we'll subsequently update this state by monitoring PropertyChanged signals
  get_properties_source_id_ =
      g_idle_add(StaticGetServicePropertiesCallback, this);
  if (get_properties_source_id_ == 0) {
    LOG(ERROR) << path_ << ": ctor: g_idle_add failed";
  }
  property_changed_handler_.delegate(this);
}

ServiceImpl::~ServiceImpl() {
  DeleteCarrierState();
  DeleteDataPlans(&data_plans_);
  if (device_ != NULL) {
    LOG(INFO) << path_ << ": deleting device " << device_->GetPath();
    delete device_;
    device_ = NULL;
  }
  if (get_properties_source_id_ != 0 &&
      !g_source_remove(get_properties_source_id_)) {
    LOG(WARNING) << path_ << ": dtor: g_source_remove failed";
  }
}

const DBus::Path& ServiceImpl::GetPath() const {
  return path_;
}

Service::State ServiceImpl::GetState() const {
  return state_;
}

Service::Type ServiceImpl::GetType() const {
  return type_;
}

// static
Service::Type ServiceImpl::TypeFromString(const std::string& type) {
  if (type == kFlimflamServiceTypeEthernet) {
    return kTypeEthernet;
  }
  if (type == kFlimflamServiceTypeWifi) {
    return kTypeWifi;
  }
  if (type == kFlimflamServiceTypeWimax) {
    return kTypeWimax;
  }
  if (type == kFlimflamServiceTypeBluetooth) {
    return kTypeBluetooth;
  }
  if (type == kFlimflamServiceTypeCellular) {
    return kTypeCellular;
  }
  return kTypeUnknown;
}

Device* ServiceImpl::GetDevice() const {
  return device_;
}

DBusDataPlanList ServiceImpl::GetDBusDataPlans() const {
  DBusDataPlanList dbus_data_plans;
  DataPlanList::const_iterator it;
  for (it = data_plans_.begin(); it != data_plans_.end(); ++it) {
    DataPlan *plan = *it;
    DCHECK(plan != NULL);
    // filter out inactive plans
    if (plan->IsActive()) {
      dbus_data_plans.push_back(plan->ToDBusFormat());
    } else {
      LOG(INFO) << path_ << ": GetBusDataPlans: skipping inactive plan: \""
          << plan->GetName() << "\"";
    }
  }
  return dbus_data_plans;
}

bool ServiceImpl::IsDefaultService() const {
  if (is_default_service_) {
    // cross-check with parent's idea of default technology
    if (parent_->GetDefaultTechnology() != kTypeCellular) {
      LOG(WARNING) << path_ << ": IsDefaultService: "
          << "service manager doesn't think default technology is cellular";
    }
    return true;
  }
  return false;
}

// Flimflam Service D-Bus Proxy methods

void ServiceImpl::PropertyChanged(const std::string& property_name,
                                  const DBus::Variant& new_value) {
  LOG(INFO) << path_ << ": PropertyChanged: property_name = " << property_name;
  // Queue a tuple representing this signal for later processing from the glib
  // main loop. We do this to avoid libdbus-c++ deadlocks that can occur when
  // sending a dbus message from within a dbus callback like this one.
  PropertyChangedSignal signal(property_name, new_value);
  property_changed_handler_.EnqueueSignal(signal);
}

// PropertyChangedDelegate methods

void ServiceImpl::OnPropertyChanged(const PropertyChangedHandler *handler,
                                    const std::string& property_name,
                                    const DBus::Variant& new_value) {
  DCHECK(handler == &property_changed_handler_);
  LOG(INFO) << path_ << ": OnPropertyChanged: property_name = "
      << property_name;
  if (property_name == kFlimflamServiceDeviceProperty) {
    OnDeviceUpdate(new_value.reader().get_path());
  } else if (property_name == kFlimflamServiceStateProperty) {
    OnStateUpdate(new_value.reader().get_string());
  } else if (property_name == kFlimflamServiceTypeProperty) {
    OnTypeUpdate(new_value.reader().get_string());
  } else if (property_name == kFlimflamServiceUsageUrlProperty) {
    OnUsageUrlUpdate(new_value.reader().get_string());
  } else if (property_name == kFlimflamServiceStickyHostRouteProperty) {
    OnStickyHostRouteUpdate(new_value.reader().get_string());
  } else if (property_name == kShillServiceHTTPProxyPortProperty) {
    OnHTTPProxyPortUpdate(new_value.reader().get_uint16());
  } else {
    // we don't care about this property
  }
}

// Device methods

void ServiceImpl::OnCarrierUpdate(const std::string& carrier) {
  LOG(INFO) << path_ << ": OnCarrierUpdate: carrier = " << carrier;
  DCHECK(device_ != NULL);
  DCHECK(carrier == device_->GetCarrier());

  // get rid of state associated with old carrier
  if (provider_ != NULL) {
    DCHECK(carrier != provider_->GetCarrier());
  }
  DeleteCarrierState();

  // we need to make a new request
  usage_request_complete_ = false;

  // if we don't have new carrier info, we can't do anything now
  if (carrier.empty()) {
    return;
  }

  // create state for new carrier
  LOG(INFO) << path_ << ": OnCarrierUpdate: creating data plan provider";
  provider_ = new(std::nothrow) DataPlanProvider(carrier);
  if (provider_ == NULL) {
    LOG(ERROR) << path_
        << ": OnCarrierUpdate: could not create data plan provider";
    return;
  }
  provider_->SetDelegate(this);
  LOG(INFO) << path_ << ": OnCarrierUpdate: getting policy for " << carrier;
  policy_ = Policy::GetPolicy(carrier);
  if (policy_ == NULL) {
    LOG(ERROR) << path_ << ": OnCarrierUpdate: could not get policy for "
        << carrier;
    DeleteCarrierState();
    return;
  }

  // Make sure the proxy is set correctly
  provider_->SetHttpProxy(http_proxy_);

  // if we already have usage url, set new provider in motion
  if (!usage_url_.empty()) {
    provider_->SetUsageUrl(usage_url_);

    vector<std::string> nameservers;
    device_->GetNameServers(&nameservers);
    provider_->SetNameServers(nameservers);

    ReconsiderSendingUsageRequests();
  } else {
    // we'll do this later in OnUsageUrlUpdate
  }
}

void ServiceImpl::OnByteCounterUpdate(uint64 rx_bytes, uint64 tx_bytes) {
  LOG(INFO) << path_ << ": OnByteCounterUpdate: rx_bytes = " << rx_bytes
      << ", tx_bytes = " << tx_bytes;

  // try to detect overflow
  if (rx_bytes < prev_rx_bytes_) {
    LOG(WARNING) << path_ << ": OnByteCounterUpdate: overflow detected:"
        " |rx_bytes|";
    rx_bytes = kuint64max;
  }
  if (tx_bytes < prev_tx_bytes_) {
    LOG(WARNING) << path_ << ": OnByteCounterUpdate: overflow detected:"
        " |tx_bytes|";
    tx_bytes = kuint64max;
  }

  // compute deltas
  uint64 delta_rx_bytes = rx_bytes - prev_rx_bytes_;
  uint64 delta_tx_bytes = tx_bytes - prev_tx_bytes_;

  // pass along byte counter info to |aggregator_|
  aggregator_->OnByteCounterUpdate(this, delta_rx_bytes, delta_tx_bytes);

  // update prev counts
  prev_rx_bytes_ = rx_bytes;
  prev_tx_bytes_ = tx_bytes;

  // dispatch this notification to our data plans list and consider sending
  // out a signal if any plans were updated
  if (usage_request_complete_ &&
      DataPlan::OnByteCounterUpdate(&data_plans_, this, parent_, device_,
                                    delta_rx_bytes, delta_tx_bytes)) {
    MaybeEmitDataPlansUpdate();
  }
}

void ServiceImpl::OnByteCounterStopped() {
  // reset previous byte counter info trackers
  prev_rx_bytes_ = 0;
  prev_tx_bytes_ = 0;
}

// DataPlan methods

bool ServiceImpl::OnDataPlanExpired(DataPlan *data_plan) {
  CHECK(data_plan != NULL);
  CHECK(device_ != NULL);

  // update byte counter stats
  if (!device_->ReadStats()) {
    LOG(WARNING) << "OnDataPlanExpired: failed to read byte counter stats";
    return false;
  }

  // request byte counter stats
  uint64 rx_bytes;
  uint64 tx_bytes;
  if (!device_->GetByteCounterStats(&rx_bytes, &tx_bytes)) {
    LOG(WARNING) << "OnDataPlanExpired: failed to retrieve byte counter stats";
    return false;
  }

  // compute deltas
  uint64 delta_rx_bytes = rx_bytes - prev_rx_bytes_;
  uint64 delta_tx_bytes = tx_bytes - prev_tx_bytes_;

  ByteCount used_bytes_to_assign = delta_rx_bytes + delta_tx_bytes;
  // try to detect overflow
  if (static_cast<uint64>(used_bytes_to_assign) < delta_rx_bytes ||
      static_cast<uint64>(used_bytes_to_assign) < delta_tx_bytes ||
      used_bytes_to_assign < 0) {
    LOG(WARNING) << "OnDataPlanExpired: overflow detected:"
        " |used_bytes_to_assign|";
    return false;
  }

  // pass along usage info to |aggregator_|
  aggregator_->OnByteCounterUpdate(this, delta_rx_bytes, delta_tx_bytes);

  // update prev counts
  prev_rx_bytes_ = rx_bytes;
  prev_tx_bytes_ = tx_bytes;

  // assign as much as we can to the expiring |data_plan|
  ByteCount assigned_bytes = DataPlan::AssignBytesToPlan(data_plan,
                                                         used_bytes_to_assign);
  CHECK_LE(assigned_bytes, used_bytes_to_assign);

  // if we still have bytes left, assign them to other active data plans
  used_bytes_to_assign -= assigned_bytes;
  if (used_bytes_to_assign > 0) {
    DataPlan::OnByteCounterUpdate(&data_plans_, this, parent_, device_,
                                  used_bytes_to_assign, 0);
  }

  return true;
}

// DataPlanProviderDelegate methods

void ServiceImpl::OnRequestComplete(const DataPlanProvider *provider,
                                    bool successful,
                                    const Value *parsed_usage_update) {
  DCHECK(provider != NULL);
  DCHECK(!successful || parsed_usage_update != NULL);
  if (provider != provider_) {
    LOG(WARNING) <<  path_ << ": OnRequestComplete: wrong provider";
    return;
  }
  DCHECK(request_in_progress_);
  DCHECK(policy_ != NULL);
  LOG(INFO) << path_ << ": OnRequestComplete: result = " << successful;
  request_in_progress_ = false;
  if (!successful) {
    LOG(WARNING) << path_ << ": OnRequestComplete: request failed";
    metrics_manager_->OnUsageRequestStatusSample(
        MetricsManager::kUsageRequestStatusFailed);
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
  LOG(INFO) << path_ << ": OnRequestComplete: version = " << version;
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
  LOG(INFO) << path_ << ": OnRequestComplete: status = " << status;
  metrics_manager_->OnUsageRequestStatusSample(
      MetricsEnumFromStatusString(status));
  if (!IsValidCrosUsageStatus(status)) {
    LOG(WARNING) << path_ << ": OnRequestComplete: invalid status: " << status;
    return;
  }
  if (status != kCrosUsageStatusOk) {
    LOG(WARNING) << path_ << ": OnRequestComplete: status: " << status;
    OnCrosUsageErrorResult(status);
    return;
  }

  bool restricted = false;
  if (root->GetBoolean(kCrosUsageRestrictedProperty, &restricted)) {
    LOG(INFO) << path_ << ": OnRequestComplete: restricted = " << restricted;
  } else {
    LOG(INFO) << path_ << ": OnRequestComplete: no restricted property";
    // restricted property is optional
  }

  ListValue *plans = NULL;  // list to which this points is owned by dictionary
  if (!root->GetList(kCrosUsagePlansProperty, &plans)) {
    LOG(WARNING) << path_ << ": OnRequestComplete: no plans property";
    return;
  }
  DCHECK(plans != NULL);
  size_t plans_size = plans->GetSize();
  LOG(INFO) << path_ << ": OnRequestComplete: plans list has size "
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
    DataPlan *plan = DataPlan::FromDictionaryValue(plan_dict, policy_);
    if (plan == NULL) {
      LOG(WARNING) << path_
          << ": OnRequestComplete: could not convert plan[" << i << "]";
      continue;
    }
    // set the service for |plan|
    plan->SetService(this);
    LOG(INFO) << path_ << ": OnRequestComplete: converted plan[" << i << "]";
    new_plans.push_back(plan);
  }

  // store new info, causing it to be used for subsequent updates to clients
  DataPlanList old_plans = data_plans_;
  data_plans_ = new_plans;
  LOG(INFO) << path_ << ": OnRequestComplete: updated data plans";

  MaybeEmitDataPlansUpdate();

  LOG(INFO) << path_ << ": OnRequestComplete: deleting old data plans";
  DeleteDataPlans(&old_plans);

  // check if we have any active data plans associated with this service
  DataPlan *active_plan = DataPlan::GetActivePlan(data_plans_);
  if (active_plan == NULL) {
    LOG(INFO) << path_ << ": OnRequestComplete: no active plans";
    return;
  }

  // we have a successfully completed usage API request
  usage_request_complete_ = true;

  StopSendingUsageRequests();
}

// Service Manager methods

void ServiceImpl::OnDefaultServiceUpdate(bool is_default_service) {
  DCHECK(is_default_service_ == !is_default_service);
  LOG(INFO) << path_ << ": OnDefaultServiceUpdate: is_default_service = "
      << is_default_service;
  is_default_service_ = is_default_service;
  if (is_default_service_) {
    // we just became the default service
    // cross-check with parent's idea of default technology
    if (parent_->GetDefaultTechnology() != kTypeCellular) {
      LOG(WARNING) << path_ << ": OnDefaultServiceUpdate: "
          << "service manager doesn't think default technology is cellular";
    }
  }
  ReconsiderSendingUsageRequests();
}

// Private methods

// static
Service::State ServiceImpl::StateFromString(const std::string& state) {
  if (state == kFlimflamServiceStateIdle) {
    return kStateIdle;
  }
  if (state == kFlimflamServiceStateCarrier) {
    return kStateCarrier;
  }
  if (state == kFlimflamServiceStateAssociation) {
    return kStateAssociation;
  }
  if (state == kFlimflamServiceStateConfiguration) {
    return kStateConfiguration;
  }
  if (state == kFlimflamServiceStateReady) {
    return kStateReady;
  }
  if (state == kFlimflamServiceStatePortal) {
    return kStatePortal;
  }
  if (state == kFlimflamServiceStateOnline) {
    return kStateOnline;
  }
  if (state == kFlimflamServiceStateDisconnect) {
    return kStateDisconnect;
  }
  if (state == kFlimflamServiceStateFailure) {
    return kStateFailure;
  }
  if (state == kFlimflamServiceStateActivationFailure) {
    return kStateActivationFailure;
  }
  return kStateUnknown;
}

void ServiceImpl::OnDeviceUpdate(const DBus::Path& device_path) {
  LOG(INFO) << path_ << ": OnDeviceUpdate: device_path = " << device_path;

  // if there's an existing device with a non-matching path: destroy it and
  // fall through to make a new one below.
  if (device_ != NULL && device_->GetPath() != device_path) {
    LOG(WARNING) << path_ << ": OnDeviceUpdate: device path changed from "
        << device_->GetPath() << " to " << device_path;
    delete device_;
    device_ = NULL;
  }

  // if there's an existing device with matching path: all is well
  if (device_ != NULL) {
    return;
  }

  // if this is not a cellular service, do not bother.
  if (type_ != kTypeCellular) {
    return;
  }

  // if there's no existing device: make one
  device_ = Device::NewDevice(this, connection_, device_path);
  if (device_ == NULL) {
    LOG(ERROR) << path_ << ": OnDeviceUpdate: couldn't create device for "
        << device_path;
    return;
  }
  LOG(INFO) << path_ << ": OnDeviceUpdate: created device for "
      << device_path;

  // start the byte counter for the new device
  if (device_ != NULL && IsConnected() && !device_->StartByteCounter()) {
    LOG(WARNING) << path_ << ": OnDeviceUpdate: could not start byte counter";
  }

  // we need to make a new request
  usage_request_complete_ = false;
  ReconsiderSendingUsageRequests();
}

void ServiceImpl::OnStateUpdate(const std::string& state) {
  LOG(INFO) << path_ << ": OnStateUpdate: state = " << state;
  Service::State old_state = state_;
  state_ = StateFromString(state);
  if (!IsConnectedState(old_state) && IsConnected()) {
    OnConnected();
  } else if (IsConnectedState(old_state) && !IsConnected()) {
    OnDisconnected();
  }
}

void ServiceImpl::OnTypeUpdate(const std::string& type) {
  LOG(INFO) << path_ << ": OnTypeUpdate: type = " << type;
  type_ = TypeFromString(type);
  if (type_ != kTypeCellular) {
    LOG(WARNING) << path_ << ": OnTypeUpdate: type is not cellular!";
  }
}

void ServiceImpl::OnUsageUrlUpdate(const std::string& usage_url) {
  LOG(INFO) << path_ << ": OnUsageUrlUpdate: url = " << usage_url;
  if (usage_url == usage_url_) {
    return;
  }

  // we need to make a new request
  usage_request_complete_ = false;

  usage_url_ = usage_url;
  if (provider_ != NULL) {
    DCHECK(usage_url_ != provider_->GetUsageUrl());
    StopSendingUsageRequests();
    provider_->SetUsageUrl(usage_url_);
    ReconsiderSendingUsageRequests();
  }
}

void ServiceImpl::OnStickyHostRouteUpdate(const std::string& sticky_route) {
  LOG(INFO) << path_ << ": OnStickyHostRouteUpdate: sticky route = "
      << sticky_route;
  if (sticky_route == sticky_host_route_) {
    return;
  }
  sticky_host_route_ = sticky_route;
  ReconsiderSendingUsageRequests();
}

void ServiceImpl::OnHTTPProxyPortUpdate(uint16_t port) {
  LOG(INFO) << path_ << ": OnHTTPProxyPortUpdate: port = " << port;
  std::ostringstream http_proxy;
  if (port > 0) {
    http_proxy << "localhost:" << port;
  }
  if (http_proxy_ == http_proxy.str())
    return;
  http_proxy_ = http_proxy.str();
  if (provider_) {
    provider_->SetHttpProxy(http_proxy_);
    ReconsiderSendingUsageRequests();
  }
}

// static
gboolean ServiceImpl::StaticGetServicePropertiesCallback(gpointer data) {
  ServiceImpl *service = reinterpret_cast<ServiceImpl*>(data);
  CHECK_NOTNULL(service);
  DCHECK_NE(service->get_properties_source_id_, 0);
  if (!service->GetServiceProperties()) {
    // call failed, so try again later
    LOG(WARNING) << service->GetPath()
        << ": StaticGetServicePropertiesCallback: "
        << "GetServiceProperties failed, will retry in "
        << kGetServicePropertiesIntervalSeconds << " secs";
    // if we've arrived here from our first g_idle_add call, set up a timer
    if (!service->retrying_get_properties_) {
      guint source_id =
          g_timeout_add_seconds(kGetServicePropertiesIntervalSeconds,
                                StaticGetServicePropertiesCallback, service);
      service->get_properties_source_id_ = source_id;
      if (source_id == 0) {
        LOG(ERROR) << service->GetPath()
            << ": StaticGetServicePropertiesCallback: "
            << "g_timeout_add_seconds failed";
        return FALSE;  // get rid of old glib source and give up
      }
      service->retrying_get_properties_ = true;
      // get rid of our old glib source, but our new timer will call us
      return FALSE;
    }
    // otherwise, our timer is already set up
    // return TRUE to indicate that we want to be called again later
    return TRUE;
  }
  service->get_properties_source_id_ = 0;
  service->retrying_get_properties_ = false;
  return FALSE;  // we succeeded, so don't repeat
}

bool ServiceImpl::GetServiceProperties() {
  LOG(INFO) << path_ << ": GetServiceProperties";
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
    return false;
  } catch (...) {  // NOLINT
    LOG(WARNING) << path_
        << ": GetServiceProperties: GetProperties() -> Exception";
    return false;
  }
  LOG(INFO) << path_ << ": GetServiceProperties: Received "
      << properties.size() << " properties";

  // Grab the properties in which we're interested.  Order is somewhat
  // important.  Type should be fetched first because it is used by
  // OnDeviceUpdate().
  PropertyMap::const_iterator it;
  it = properties.find(kFlimflamServiceTypeProperty);
  if (it != properties.end()) {
    const DBus::Variant& value = static_cast<DBus::Variant>(it->second);
    OnTypeUpdate(value.reader().get_string());
  } else {
    LOG(WARNING) << path_ << ": GetServiceProperties: no Type property";
  }
  it = properties.find(kFlimflamServiceDeviceProperty);
  if (it != properties.end()) {
    const DBus::Variant& value = static_cast<DBus::Variant>(it->second);
    OnDeviceUpdate(value.reader().get_path());
  } else {
    LOG(WARNING) << path_ << ": GetServiceProperties: no Device property";
  }
  it = properties.find(kFlimflamServiceStateProperty);
  if (it != properties.end()) {
    const DBus::Variant& value = static_cast<DBus::Variant>(it->second);
    OnStateUpdate(value.reader().get_string());
  } else {
    LOG(WARNING) << path_ << ": GetServiceProperties: no State property";
  }
  it = properties.find(kFlimflamServiceStickyHostRouteProperty);
  if (it != properties.end()) {
    const DBus::Variant& value = static_cast<DBus::Variant>(it->second);
    OnStickyHostRouteUpdate(value.reader().get_string());
  } else {
    LOG(WARNING) << path_
        << ": GetServiceProperties: no StickyHostRoute property";
  }
  it = properties.find(kShillServiceHTTPProxyPortProperty);
  if (it != properties.end()) {
    const DBus::Variant& value = static_cast<DBus::Variant>(it->second);
    OnHTTPProxyPortUpdate(value.reader().get_uint16());
  } else {
    LOG(WARNING) << path_
        << ": GetServiceProperties: no HTTPProxyPort property";
  }

  // don't expect to find Cellular.* properties if we're not a cellular service
  if (type_ != kTypeCellular) {
    return true;
  }
  it = properties.find(kFlimflamServiceUsageUrlProperty);
  if (it != properties.end()) {
    const DBus::Variant& value = static_cast<DBus::Variant>(it->second);
    OnUsageUrlUpdate(value.reader().get_string());
  } else {
    LOG(WARNING) << path_
        << ": GetServiceProperties: no Cellular.UsageUrl property";
  }
  return true;
}

void ServiceImpl::DeleteDataPlans(DataPlanList *data_plans) {
  DCHECK(data_plans != NULL);
  while (!data_plans->empty()) {
    DataPlan *plan = *data_plans->begin();
    DCHECK(plan != NULL);
    LOG(INFO) << path_ << ": DeleteDataPlans: deleting plan: "
        << plan->GetName();
    delete plan;
    data_plans->erase(data_plans->begin());
  }
}

void ServiceImpl::AddHardcodedDataPlan() {
  const std::string name = "Chromium OS Test Plan";
  DataPlan::Type type = DataPlan::kTypeMeteredPaid;
  base::Time update_time = base::Time::Now();
  base::TimeDelta twelve_hours = base::TimeDelta::FromHours(12);
  base::Time start_time = base::Time::Now() - twelve_hours;
  base::Time end_time = base::Time::Now() + twelve_hours;
  ByteCount data_bytes_max = 100*1024*1024;  // 100 MB
  ByteCount data_bytes_used = 30*1024*1024;  // 30 MB

  DataPlan *plan = new(std::nothrow) DataPlan(name, type, update_time,
                                              start_time, end_time,
                                              data_bytes_max, data_bytes_used);
  if (plan == NULL) {
    LOG(ERROR) << path_ << ": AddHardcodedDataPlan: could not create plan";
    return;
  }

  data_plans_.push_back(plan);
}

void ServiceImpl::RequestUsageUpdate() {
  DCHECK(ShouldSendUsageRequests());
  if (request_in_progress_) {
    LOG(WARNING) << path_
        << ": RequestUsageUpdate: request already in progress";
    return;
  }
  request_in_progress_ = true;
  if (!provider_->RequestUsageUpdate()) {
    LOG(WARNING) << path_ << ": RequestUsageUpdate: request failed";
    request_in_progress_ = false;
  } else {
    LOG(INFO) << path_ << ": RequestUsageUpdate: request succeeded";
  }
}

void ServiceImpl::CancelPendingRequests() {
  if (provider_ != NULL && request_in_progress_) {
    provider_->CancelPendingRequests();
    request_in_progress_ = false;
  }
}

gboolean ServiceImpl::UpdateTimeoutCallback() {
  LOG(INFO) << path_ << ": UpdateTimeoutCallback";
  RequestUsageUpdate();
  return TRUE;
}

// static
gboolean ServiceImpl::StaticUpdateTimeoutCallback(gpointer data) {
  ServiceImpl *service = reinterpret_cast<ServiceImpl*>(data);
  CHECK_NOTNULL(service);
  return service->UpdateTimeoutCallback();
}

bool ServiceImpl::CreateUpdateTimer() {
  DCHECK(provider_ != NULL);
  DCHECK(policy_ != NULL);
  guint idle_seconds = policy_->GetUpdateTimerIdleSecs(data_plans_);
  LOG(INFO) << path_ << ": CreateUpdateTimer: idle_seconds = " << idle_seconds;
  if (update_timeout_source_ != NULL) {
    LOG(WARNING) << path_ << ": CreateUpdateTimer: timer already running";
    return false;
  }
  update_timeout_source_ = g_timeout_source_new_seconds(idle_seconds);
  if (update_timeout_source_ == NULL) {
    LOG(WARNING) << path_
        << ": CreateUpdateTimer: could not create timeout source";
    return false;
  }
  g_source_set_callback(update_timeout_source_, StaticUpdateTimeoutCallback,
                        this, NULL);
  g_source_attach(update_timeout_source_, NULL);
  return true;
}

void ServiceImpl::DestroyUpdateTimer() {
  LOG(INFO) << path_ << ": DestroyUpdateTimer";
  if (update_timeout_source_ != NULL) {
    g_source_destroy(update_timeout_source_);
    update_timeout_source_ = NULL;
  }
}

void ServiceImpl::DeleteCarrierState() {
  LOG(INFO) << path_ << ": DeleteCarrierState";
  if (policy_ != NULL) {
    LOG(INFO) << path_ << ": DeleteCarrierState: deleting policy";
    delete policy_;
    policy_ = NULL;
  }
  StopSendingUsageRequests();
  if (provider_ != NULL) {
    LOG(INFO) << path_ << ": DeleteCarrierState: deleting data plan provider";
    delete provider_;
    provider_ = NULL;
  }
}

bool ServiceImpl::IsValidCrosUsageStatus(const std::string& status) {
  if (status == kCrosUsageStatusOk ||
      status == kCrosUsageStatusError ||
      status == kCrosUsageStatusMalformedRequest ||
      status == kCrosUsageStatusInternalError ||
      status == kCrosUsageStatusServiceUnavailable ||
      status == kCrosUsageStatusRequestRefused ||
      status == kCrosUsageStatusUnknownDevice) {
    return true;
  }
  return false;
}

MetricsManager::UsageRequestStatus ServiceImpl::MetricsEnumFromStatusString(
    const std::string& status) {
  DCHECK(IsValidCrosUsageStatus(status));
  if (status == kCrosUsageStatusOk) {
    return MetricsManager::kUsageRequestStatusOk;
  }
  if (status == kCrosUsageStatusError) {
    return MetricsManager::kUsageRequestStatusError;
  }
  if (status == kCrosUsageStatusMalformedRequest) {
    return MetricsManager::kUsageRequestStatusMalformedRequest;
  }
  if (status == kCrosUsageStatusInternalError) {
    return MetricsManager::kUsageRequestStatusInternalError;
  }
  if (status == kCrosUsageStatusServiceUnavailable) {
    return MetricsManager::kUsageRequestStatusServiceUnavailable;
  }
  if (status == kCrosUsageStatusRequestRefused) {
    return MetricsManager::kUsageRequestStatusRequestRefused;
  }
  if (status == kCrosUsageStatusUnknownDevice) {
    return MetricsManager::kUsageRequestStatusUnknownDevice;
  }
  LOG(WARNING) << "MetricsEnumFromStatusString: Unknown status: " << status;
  return MetricsManager::kUsageRequestStatusError;
}

void ServiceImpl::OnCrosUsageErrorResult(const std::string& status) {
  LOG(WARNING) << path_ << ": OnCrosUsageErrorResult: " << status;
  // TODO(vlaviano): hook for future use
}

// static
bool ServiceImpl::IsConnectedState(State state) {
  return (state == kStateReady ||
          state == kStateOnline ||
          state == kStatePortal);
}

bool ServiceImpl::IsConnected() const {
  return IsConnectedState(state_);
}

void ServiceImpl::OnConnected() {
  LOG(INFO) << path_ << ": OnConnected";
  DCHECK(IsConnected());
  DCHECK(!usage_request_complete_);

  // start byte counter
  if (device_ != NULL && !device_->ByteCounterRunning() &&
      !device_->StartByteCounter()) {
    LOG(WARNING) << path_ << ": OnConnected: could not start byte counter";
  }

  // Make sure the name servers are set
  if (device_ != NULL && provider_ != NULL) {
    vector<std::string> nameservers;
    device_->GetNameServers(&nameservers);
    provider_->SetNameServers(nameservers);
  } else {
    LOG(WARNING) << path_ << ": OnConnected: missing device_ or provider_";
  }

  ReconsiderSendingUsageRequests();
}

void ServiceImpl::OnDisconnected() {
  LOG(INFO) << path_ << ": OnDisconnected";
  DCHECK(!IsConnected());

  // stop byte counter
  if (device_ != NULL) {
    device_->StopByteCounter();
  }

  // we'll issue a new usage API request when we reconnect
  usage_request_complete_ = false;

  ReconsiderSendingUsageRequests();
}

bool ServiceImpl::IsSendingUsageRequests() const {
  return request_in_progress_ || update_timeout_source_ != NULL;
}

bool ServiceImpl::ShouldSendUsageRequests() const {
  if (usage_request_complete_) {
    LOG(INFO) << path_ <<
        ": ShouldSendUsageRequests: no: request already completed";
    return false;
  }
  if (provider_ == NULL) {
    LOG(INFO) << path_ << ": ShouldSendUsageRequests: no: no provider";
    DCHECK(policy_ == NULL);
    return false;
  }
  DCHECK(device_ != NULL);
  DCHECK(!device_->GetCarrier().empty());
  DCHECK(policy_ != NULL);
  if (usage_url_.empty()) {
    LOG(INFO) << path_ << ": ShouldSendUsageRequests: no: no usage url";
    return false;
  }
  if (!IsConnected()) {
    LOG(INFO) << path_ << ": ShouldSendUsageRequests: no: not connected";
    return false;
  }
  if (sticky_host_route_.empty() && http_proxy_.empty()
      && !IsDefaultService()) {
    LOG(INFO) << path_ << ": ShouldSendUsageRequests: "
        << "no: no sticky host route, no proxy and not default service";
    return false;
  }
  LOG(INFO) << path_ << ": ShouldSendUsageRequests: yes";
  return true;
}

void ServiceImpl::StopSendingUsageRequests() {
  LOG(INFO) << path_ << ": StopSendingUsageRequests";
  DestroyUpdateTimer();
  CancelPendingRequests();
}

void ServiceImpl::StartSendingUsageRequests() {
  LOG(INFO) << path_ << ": StartSendingUsageRequests";
  DCHECK(!IsSendingUsageRequests());
  RequestUsageUpdate();
  CreateUpdateTimer();
}

void ServiceImpl::ReconsiderSendingUsageRequests() {
  LOG(INFO) << path_ << ": ReconsiderSendingUsageRequests";
  if (!IsSendingUsageRequests() && ShouldSendUsageRequests()) {
    StartSendingUsageRequests();
  } else if (IsSendingUsageRequests() && !ShouldSendUsageRequests()) {
    StopSendingUsageRequests();
  }
}

void ServiceImpl::MaybeEmitDataPlansUpdate() {
  DCHECK(policy_ != NULL);
  if (policy_->ShouldEmitDataPlansUpdate(data_plans_)) {
    LOG(INFO) << path_ << ": MaybeEmitDataPlansUpdate: sending update";
    parent_->EmitDataPlansUpdate(*this);
  } else {
    LOG(INFO) << path_ << ": MaybeEmitDataPlansUpdate: not sending update";
  }
}

}  // namespace cashew
