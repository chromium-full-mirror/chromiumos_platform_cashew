// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/service_manager.h"

#include <glog/logging.h>

#include "src/service.h"

namespace cashew {

// Flimflam D-Bus indentifiers
static const char* kFlimflamManagerName = "org.chromium.flimflam";
static const char* kFlimflamManagerPath = "/";

// Flimflam property names
static const char* kServicesProperty = "Services";

// Flimflam service types
static const char* kTypeCellular = "cellular";

ServiceManager::ServiceManager(DBus::Connection& connection)  // NOLINT
    : DBus::ObjectProxy(connection, kFlimflamManagerPath,
                        kFlimflamManagerName),
      connection_(connection) {
  // init our state with a GetProperties() call to Flimflam
  // we'll update this state by monitoring PropertyChanged signals
  GetFlimflamServices();
}

ServiceManager::~ServiceManager() {
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

// Flimflam Manager D-Bus Proxy methods

void ServiceManager::PropertyChanged(const std::string& property_name,
                                     const DBus::Variant& new_value) {
  DLOG(INFO) << "PropertyChanged: property_name = " << property_name;

  // we only care about Services changes
  if (property_name.compare(kServicesProperty)) {
      return;
  }

  // interpret new_value as a vector of service paths
  ServicePathList paths;
  DBus::MessageIter reader = new_value.reader();
  reader >> paths;

  OnServicesUpdate(paths);
}

void ServiceManager::StateChanged(const std::string& new_state) {
  DLOG(INFO) << "StateChanged: new_state = " << new_state;
  // TODO(vlaviano): switch on state and call private onFlimflamOffline(),
  // onFlimflamOnline() hooks.  If we care.
}

// Private methods

void ServiceManager::DeleteServices(ServiceMap *service_map) {
  DCHECK(service_map != NULL);
  while (!service_map->empty()) {
    const std::string& path =
        static_cast<std::string>(service_map->begin()->first);
    DLOG(INFO) << "DeleteServices: deleting service: " << path;
    Service *service = static_cast<Service *>(service_map->begin()->second);
    delete service;
    service_map->erase(service_map->begin());
  }
}

void ServiceManager::GetFlimflamServices() {
  DLOG(INFO) << "GetFlimflamServices";
  PropertyMap properties;
  // dbus-c++ throws exceptions
  // invoke the "Existing Non-conformant Code" clause of the style guide and
  // isolate the rest of the system from this
  try {
    // TODO(vlaviano): make this call asynchronous
    properties = GetProperties();
  } catch (DBus::Error& error) {  // NOLINT
    LOG(WARNING) << "GetFlimflamServices: GetProperties() -> Exception: "
        << error.name() << ": " << error.message();
    // TODO(vlaviano): schedule another attempt later
    return;
  } catch (...) {  // NOLINT
    LOG(WARNING) << "GetFlimflamServices: GetProperties() -> Exception";
    // TODO(vlaviano): schedule another attempt later
    return;
  }

  // grab Services property from result
  PropertyMap::const_iterator it = properties.find(kServicesProperty);
  if (it == properties.end()) {
    DLOG(WARNING) << "GetFlimflamServices: no Services property";
    return;
  }
  DBus::Variant value = static_cast<DBus::Variant>(it->second);

  // interpret value as vector of service paths
  ServicePathList paths;
  DBus::MessageIter reader = value.reader();
  reader >> paths;

  OnServicesUpdate(paths);
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

  // services left in old_services failed to appear in the update: delete them
  DeleteServices(&old_services);
}

void ServiceManager::OnNewService(const DBus::Path& path) {
  DLOG(INFO) << "OnNewService: " << path;
  DCHECK(GetService(path) == NULL);
  Service *service = new(std::nothrow) Service(connection_, path);
  if (service == NULL) {
    LOG(ERROR) << "OnNewService: couldn't create service for " << path;
    return;
  }
  services_[path] = service;
}

}  // namespace cashew
