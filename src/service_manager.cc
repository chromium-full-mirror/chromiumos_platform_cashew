// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/service_manager.h"

#include <glog/logging.h>

#include "src/cashew_server.h"
#include "src/service.h"

namespace cashew {

// Flimflam D-Bus indentifiers
static const char *kFlimflamManagerName = "org.chromium.flimflam";
static const char *kFlimflamManagerPath = "/";

// Flimflam property names
static const char *kDefaultTechnologyProperty = "DefaultTechnology";
static const char *kServicesProperty = "Services";
static const char *kStateProperty = "State";

// Flimflam on-the-wire State values
static const char *kFlimflamStateOffline = "offline";
static const char *kFlimflamStateConnected = "connected";
static const char *kFlimflamStateOnline = "online";

// Flimflam service types
static const char *kTypeCellular = "cellular";

ServiceManager::ServiceManager(DBus::Connection& connection)  // NOLINT
    : DBus::ObjectProxy(connection, kFlimflamManagerPath,
                        kFlimflamManagerName),
      connection_(connection), cashew_server_(NULL),
      default_technology_(Service::kTypeUnknown),
      default_cellular_service_(NULL),
      connectivity_state_(kConnectivityStateUnknown) {
  // init our state with a GetProperties() call to Flimflam
  // we'll update this state by monitoring PropertyChanged signals
  GetFlimflamProperties();
}

ServiceManager::~ServiceManager() {
  ClearDefaultCellularService();
  DeleteServices(&services_);
}

const Service* ServiceManager::GetService(const std::string& service_path)
    const {
  ServiceMap::const_iterator it = services_.find(service_path);
  if (it == services_.end()) {
    return NULL;
  }
  DCHECK(it->second != NULL);
  return static_cast<const Service *>(it->second);
}

void ServiceManager::SetCashewServer(CashewServer *cashew_server) {
  DLOG(INFO) << "SetCashewServer";
  cashew_server_ = cashew_server;
}

Service::Type ServiceManager::GetDefaultTechnology() const {
  return default_technology_;
}

ServiceManager::ConnectivityState ServiceManager::GetConnectivityState()
    const {
  return connectivity_state_;
}

// static
bool ServiceManager::IsOfflineConnectivityState(
    ServiceManager::ConnectivityState state) {
  if (state == kConnectivityStateUnknown ||
      state == kConnectivityStateOffline) {
    return true;
  }
  return false;
}

// static
bool ServiceManager::IsOnlineConnectivityState(
    ServiceManager::ConnectivityState state) {
  if (state == kConnectivityStateConnected ||
      state == kConnectivityStateOnline) {
    return true;
  }
  return false;
}

// Flimflam Manager D-Bus Proxy methods

void ServiceManager::PropertyChanged(const std::string& property_name,
                                     const DBus::Variant& new_value) {
  DLOG(INFO) << "PropertyChanged: property_name = " << property_name;
  if (property_name == kDefaultTechnologyProperty) {
    OnDefaultTechnologyUpdate(new_value.reader().get_string());
  } else if (property_name == kServicesProperty) {
    // interpret new_value as a vector of service paths
    ServicePathList paths;
    DBus::MessageIter reader = new_value.reader();
    reader >> paths;
    OnServicesUpdate(paths);
  } else if (property_name == kStateProperty) {
    // this has its own signal, StateChanged, but just in case...
    StateChanged(new_value.reader().get_string());
  } else {
    // we don't care about this property
  }
}

void ServiceManager::StateChanged(const std::string& new_state_string) {
  DLOG(INFO) << "StateChanged: new_state = " << new_state_string;
  ConnectivityState old_state = connectivity_state_;
  connectivity_state_ = ConnectivityStateFromString(new_state_string);
  // see if we're coming online or going offline
  // NOTE: we consider "unknown" to be offline
  if (IsOfflineConnectivityState(old_state) &&
      IsOnlineConnectivityState(connectivity_state_)) {
    OnFlimflamOnline();
  } else if (IsOnlineConnectivityState(old_state) &&
      IsOfflineConnectivityState(connectivity_state_)) {
    OnFlimflamOffline();
  }
}

// Service methods

void ServiceManager::EmitDataPlansUpdate(const Service& service) {
  DLOG(INFO) << "EmitDataPlansUpdate: service = " << service.GetPath();
  if (cashew_server_ == NULL) {
    DLOG(WARNING) << "EmitDataPlansUpdate: no Cashew server";
    return;
  }
  cashew_server_->EmitDataPlansUpdate(service);
}

// Private methods

void ServiceManager::DeleteServices(ServiceMap *service_map) {
  DCHECK(service_map != NULL);
  while (!service_map->empty()) {
    const std::string& path =
        static_cast<std::string>(service_map->begin()->first);
    DLOG(INFO) << "DeleteServices: deleting service: " << path;
    Service *service = static_cast<Service *>(service_map->begin()->second);
    DCHECK(service != NULL);
    DCHECK(service != default_cellular_service_);
    DCHECK(*service_map == services_ || GetService(service->GetPath()) == NULL);
    delete service;
    service_map->erase(service_map->begin());
  }
}

void ServiceManager::GetFlimflamProperties() {
  DLOG(INFO) << "GetFlimflamProperties";
  PropertyMap properties;
  // dbus-c++ throws exceptions
  // invoke the "Existing Non-conformant Code" clause of the style guide and
  // isolate the rest of the system from this
  try {
    // TODO(vlaviano): make this call asynchronous
    properties = GetProperties();
  } catch (DBus::Error& error) {  // NOLINT
    LOG(WARNING) << "GetFlimflamProperties: GetProperties() -> Exception: "
        << error.name() << ": " << error.message();
    // TODO(vlaviano): schedule another attempt later
    return;
  } catch (...) {  // NOLINT
    LOG(WARNING) << "GetFlimflamProperties: GetProperties() -> Exception";
    // TODO(vlaviano): schedule another attempt later
    return;
  }
  DLOG(INFO) << "GetFlimflamProperties: Received " << properties.size()
      << " properties";

  // grab the properties in which we're interested
  PropertyMap::const_iterator it = properties.find(kDefaultTechnologyProperty);
  if (it != properties.end()) {
    const DBus::Variant& value = static_cast<DBus::Variant>(it->second);
    OnDefaultTechnologyUpdate(value.reader().get_string());
  } else {
    DLOG(WARNING) << "GetFlimflamProperties: no DefaultTechnology property";
  }
  it = properties.find(kServicesProperty);
  if (it != properties.end()) {
    const DBus::Variant& value = static_cast<DBus::Variant>(it->second);
    // interpret value as vector of service paths
    ServicePathList paths;
    DBus::MessageIter reader = value.reader();
    reader >> paths;
    OnServicesUpdate(paths);
  } else {
    DLOG(WARNING) << "GetFlimflamProperties: no Services property";
  }
  it = properties.find(kStateProperty);
  if (it != properties.end()) {
    const DBus::Variant& value = static_cast<DBus::Variant>(it->second);
    StateChanged(value.reader().get_string());
  } else {
    DLOG(WARNING) << "GetFlimflamProperties: no State property";
  }
}

void ServiceManager::OnDefaultTechnologyUpdate(
    const std::string& default_technology) {
  DLOG(INFO) << "OnDefaultTechnologyUpdate: default technology = "
      << default_technology;
  default_technology_ = Service::TypeFromString(default_technology);
  if (default_technology_ == Service::kTypeUnknown) {
    LOG(WARNING) << "OnDefaultTechnologyUpdate: unknown default technology: "
        << default_technology;
  }
  if (default_technology_ != Service::kTypeCellular &&
      default_cellular_service_ != NULL) {
    // if the default technology isn't cellular and yet we think that one of
    // our cellular services is the default service, then we need to update
    // our view of the world.
    ClearDefaultCellularService();
  }
}

void ServiceManager::OnServicesUpdate(const ServicePathList& paths) {
  // Remember old set of services. This is the baseline from which we will
  // determine what has changed.
  ServiceMap old_services = services_;
  services_.clear();

  // Flimflam service types are embedded in their path names, so we can use
  // the pattern "/cellular_" to decide which paths are cellular services.
  static const std::string cellular_pattern = std::string("/") + kTypeCellular
      + "_";

  // Walk paths in this update and see if they're new to us
  DLOG(INFO) << "OnServicesUpdate: " << paths.size() << " service paths";
  ServicePathList::const_iterator it;
  for (it = paths.begin(); it != paths.end(); ++it) {
    const DBus::Path& path = *it;

    // We only care about cellular services
    if (path.find(cellular_pattern) == std::string::npos) {
      continue;
    }

    ServiceMap::iterator old_it = old_services.find(path);
    if (old_it != old_services.end()) {
      // service is old: move it from old_services to services_
      Service *service = static_cast<Service *>(old_it->second);
      DCHECK(service != NULL);
      old_services.erase(old_it);
      services_[path] = service;
    } else {
      // service is new: create a Service obj for it and add it to services_
      OnNewService(path);
    }
  }

  // clear default service if it failed to appear in the update and we're
  // about to delete it
  if (default_cellular_service_ != NULL) {
    ServiceMap::const_iterator it =
        old_services.find(default_cellular_service_->GetPath());
    if (it != old_services.end()) {
      ClearDefaultCellularService();
    }
  }

  // services left in old_services failed to appear in the update: delete them
  DeleteServices(&old_services);

  // Flimflam always places the default service first in its list of services,
  // so we'll take a look at the first element in the paths list and see if the
  // default service has changed. Note that |paths| can be empty. We still want
  // to know about this, since it means that there is no default service.
  const DBus::Path *default_service_path = NULL;
  if (!paths.empty()) {
    default_service_path = &*paths.begin();
  }
  OnDefaultServiceUpdate(default_service_path);
}

void ServiceManager::OnNewService(const DBus::Path& path) {
  DLOG(INFO) << "OnNewService: " << path;
  DCHECK(GetService(path) == NULL);
  Service *service = new(std::nothrow) Service(this, connection_, path);
  if (service == NULL) {
    LOG(ERROR) << "OnNewService: couldn't create service for " << path;
    return;
  }
  services_[path] = service;
}

void ServiceManager::OnDefaultServiceUpdate(const DBus::Path *path) {
  DLOG(INFO) << "OnDefaultServiceUpdate: new default service = "
      << ((path == NULL) ? "None" : *path);
  // check if new default service path (if any) corresponds to one of our
  // cellular services
  Service *new_default_cellular_service = NULL;
  if (path != NULL) {
    new_default_cellular_service = const_cast<Service*>(GetService(*path));
  }
  if (new_default_cellular_service != NULL) {
    SetDefaultCellularService(new_default_cellular_service);
  } else {
    ClearDefaultCellularService();
  }
}

void ServiceManager::ClearDefaultCellularService() {
  DLOG(INFO) << "ClearDefaultCellularService";
  if (default_cellular_service_ != NULL) {
    DCHECK(GetService(default_cellular_service_->GetPath()) != NULL);
    DCHECK(default_cellular_service_->IsDefaultService());
    default_cellular_service_->OnDefaultServiceUpdate(false);
    default_cellular_service_ = NULL;
  }
}

void ServiceManager::SetDefaultCellularService(Service *service) {
  DLOG(INFO) << "SetDefaultCellularService: "
      << ((service == NULL) ? "None" : service->GetPath());
  if (default_cellular_service_ == service) {
    return;
  }
  if (default_cellular_service_ != NULL) {
    ClearDefaultCellularService();
  }
  DCHECK(default_cellular_service_ == NULL);
  if (service != NULL) {
    DCHECK(!service->IsDefaultService());
    service->OnDefaultServiceUpdate(true);
    default_cellular_service_ = service;
  }
}

// static
ServiceManager::ConnectivityState ServiceManager::ConnectivityStateFromString(
    const std::string& state) {
  if (state == kFlimflamStateOffline) {
    return kConnectivityStateOffline;
  }
  if (state == kFlimflamStateConnected) {
    return kConnectivityStateConnected;
  }
  if (state == kFlimflamStateOnline) {
    return kConnectivityStateOnline;
  }
  return kConnectivityStateUnknown;
}

void ServiceManager::OnFlimflamOnline() {
  DLOG(INFO) << "OnFlimflamOnline";
  // notify our child services
  ServiceMap::iterator it;
  for (it = services_.begin(); it != services_.end(); ++it) {
    Service *service = static_cast<Service*>(it->second);
    DCHECK(service != NULL);
    service->OnFlimflamOnline();
  }
}

void ServiceManager::OnFlimflamOffline() {
  DLOG(INFO) << "OnFlimflamOffline";
  // notify our child services
  ServiceMap::iterator it;
  for (it = services_.begin(); it != services_.end(); ++it) {
    Service *service = static_cast<Service*>(it->second);
    DCHECK(service != NULL);
    service->OnFlimflamOffline();
  }
}

}  // namespace cashew
