// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/service_manager_impl.h"

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

// Flimflam service types
static const char *kTypeCellular = "cellular";

// GetFlimflamProperties retry interval
static const guint kSecondsPerMinute = 60;
static const guint kGetFlimflamPropertiesIntervalSeconds =
    1 * kSecondsPerMinute;

ServiceManagerImpl::ServiceManagerImpl(DBus::Connection& connection,  // NOLINT
                                       GMainLoop * const main_loop,
                                       MetricsManager * const metrics_manager,
                                       Aggregator * const aggregator)
    : DBus::ObjectProxy(connection, kFlimflamManagerPath,
                        kFlimflamManagerName),
      connection_(connection), main_loop_(CHECK_NOTNULL(main_loop)),
      metrics_manager_(CHECK_NOTNULL(metrics_manager)),
      aggregator_(CHECK_NOTNULL(aggregator)), cashew_server_(NULL),
      default_technology_(Service::kTypeUnknown),
      default_cellular_service_(NULL), get_properties_source_id_(0),
      retrying_get_properties_(false) {
  // schedule a GetProperties() call to Flimflam to init our state
  // we'll keep trying periodically until we succeed
  // we'll subsequently update this state by monitoring PropertyChanged signals
  get_properties_source_id_ =
      g_idle_add(StaticGetFlimflamPropertiesCallback, this);
  if (get_properties_source_id_ == 0) {
    LOG(ERROR) << "ctor: g_idle_add failed";
  }
  property_changed_handler_.delegate(this);
}

ServiceManagerImpl::~ServiceManagerImpl() {
  DCHECK(!g_main_loop_is_running(main_loop_));
  ClearDefaultCellularService();
  DeleteServices(&services_);
  if (get_properties_source_id_ != 0 &&
      !g_source_remove(get_properties_source_id_)) {
    LOG(WARNING) << "dtor: g_source_remove failed";
  }
}

const Service* ServiceManagerImpl::GetService(const std::string& service_path)
    const {
  ServiceMap::const_iterator it = services_.find(service_path);
  if (it == services_.end()) {
    return NULL;
  }
  DCHECK(it->second != NULL);
  return static_cast<const Service *>(it->second);
}

void ServiceManagerImpl::SetCashewServer(CashewServer *cashew_server) {
  LOG(INFO) << "SetCashewServer";
  cashew_server_ = cashew_server;
}

Service::Type ServiceManagerImpl::GetDefaultTechnology() const {
  return default_technology_;
}

// Flimflam Manager D-Bus Proxy methods

void ServiceManagerImpl::PropertyChanged(const std::string& property_name,
                                         const DBus::Variant& new_value) {
  LOG(INFO) << "PropertyChanged: property_name = " << property_name;
  // Queue a tuple representing this signal for later processing from the glib
  // main loop.  We do this to avoid libdbus-c++ deadlocks that can occur when
  // sending a dbus message from within a dbus callback like this one.
  PropertyChangedSignal signal(property_name, new_value);
  property_changed_handler_.EnqueueSignal(signal);
}

void ServiceManagerImpl::StateChanged(const std::string& new_state_string) {
  LOG(INFO) << "StateChanged: new_state_string = " << new_state_string;
  // Queue a tuple representing this signal for later processing from the glib
  // main loop. See comments in PropertyChanged above.
  DBus::Variant new_value;
  new_value.writer().append_string(new_state_string.c_str());
  // StateChanged is a special case of PropertyChanged, so we just construct a
  // PropertyChanged signal containing the new state info
  PropertyChangedSignal signal(kStateProperty, new_value);
  property_changed_handler_.EnqueueSignal(signal);
}

// PropertyChangedDelegate methods

void ServiceManagerImpl::OnPropertyChanged(
                                          const PropertyChangedHandler *handler,
                                          const std::string& property_name,
                                          const DBus::Variant& new_value) {
  DCHECK(handler == &property_changed_handler_);
  LOG(INFO) << "OnPropertyChanged: property_name = " << property_name;
  if (property_name == kDefaultTechnologyProperty) {
    OnDefaultTechnologyUpdate(new_value.reader().get_string());
  } else if (property_name == kServicesProperty) {
    // interpret new_value as a vector of service paths
    ServicePathList paths;
    DBus::MessageIter reader = new_value.reader();
    reader >> paths;
    OnServicesUpdate(paths);
  } else {
    // we don't care about this property
  }
}

// Service methods

void ServiceManagerImpl::EmitDataPlansUpdate(const Service& service) {
  LOG(INFO) << "EmitDataPlansUpdate: service = " << service.GetPath();
  if (cashew_server_ == NULL) {
    LOG(WARNING) << "EmitDataPlansUpdate: no Cashew server";
    return;
  }
  cashew_server_->EmitDataPlansUpdate(service);
}

void ServiceManagerImpl::OnDataPlanInactive(const Service& service,
                                            const DataPlan& plan) {
  LOG(INFO) << "OnDataPlanInactive: service = " << service.GetPath()
      << ", plan = " << plan.GetName();
  DCHECK(!plan.IsActive());
  // TODO(vlaviano): hook for future use
  // we could emit a PlanExpired D-Bus signal for Chrome to consume
}

// Private methods

void ServiceManagerImpl::DeleteServices(ServiceMap *service_map) {
  DCHECK(service_map != NULL);
  while (!service_map->empty()) {
    const std::string& path =
        static_cast<std::string>(service_map->begin()->first);
    LOG(INFO) << "DeleteServices: deleting service: " << path;
    Service *service = static_cast<Service *>(service_map->begin()->second);
    DCHECK(service != NULL);
    DCHECK(service != default_cellular_service_);
    DCHECK(*service_map == services_ || GetService(service->GetPath()) == NULL);
    delete service;
    service_map->erase(service_map->begin());
  }
}

// static
gboolean ServiceManagerImpl::StaticGetFlimflamPropertiesCallback(
                                                                gpointer data) {
  ServiceManagerImpl *service_manager =
      reinterpret_cast<ServiceManagerImpl*>(data);
  CHECK_NOTNULL(service_manager);
  DCHECK_NE(service_manager->get_properties_source_id_, 0);
  if (!service_manager->GetFlimflamProperties()) {
    // call failed, so try again later
    LOG(WARNING) << "StaticGetFlimflamPropertiesCallback: "
        << "GetFlimflamProperties failed, will retry in "
        << kGetFlimflamPropertiesIntervalSeconds << " secs";
    // if we've arrived here from our first g_idle_add call, set up a timer
    if (!service_manager->retrying_get_properties_) {
      guint source_id =
          g_timeout_add_seconds(kGetFlimflamPropertiesIntervalSeconds,
                                StaticGetFlimflamPropertiesCallback,
                                service_manager);
      service_manager->get_properties_source_id_ = source_id;
      if (source_id == 0) {
        LOG(ERROR) << "StaticGetFlimflamPropertiesCallback: "
            << "g_timeout_add_seconds failed";
        return FALSE;  // get rid of old glib source and give up
      }
      service_manager->retrying_get_properties_ = true;
      // get rid of our old glib source, but our new timer will call us
      return FALSE;
    }
    // otherwise, our timer is already set up
    // return TRUE to indicate that we want to be called again later
    return TRUE;
  }
  service_manager->get_properties_source_id_ = 0;
  service_manager->retrying_get_properties_ = false;
  return FALSE;  // we succeeded, so don't repeat
}

bool ServiceManagerImpl::GetFlimflamProperties() {
  LOG(INFO) << "GetFlimflamProperties";
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
    return false;
  } catch (...) {  // NOLINT
    LOG(WARNING) << "GetFlimflamProperties: GetProperties() -> Exception";
    return false;
  }
  LOG(INFO) << "GetFlimflamProperties: Received " << properties.size()
      << " properties";

  // grab the properties in which we're interested
  PropertyMap::const_iterator it = properties.find(kDefaultTechnologyProperty);
  if (it != properties.end()) {
    const DBus::Variant& value = static_cast<DBus::Variant>(it->second);
    OnDefaultTechnologyUpdate(value.reader().get_string());
  } else {
    LOG(WARNING) << "GetFlimflamProperties: no DefaultTechnology property";
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
    LOG(WARNING) << "GetFlimflamProperties: no Services property";
  }
  return true;
}

void ServiceManagerImpl::OnDefaultTechnologyUpdate(
    const std::string& default_technology) {
  LOG(INFO) << "OnDefaultTechnologyUpdate: default technology = "
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

void ServiceManagerImpl::OnServicesUpdate(const ServicePathList& paths) {
  // Remember old set of services. This is the baseline from which we will
  // determine what has changed.
  ServiceMap old_services = services_;
  services_.clear();

  // Flimflam service types are embedded in their path names, so we can use
  // the pattern "/cellular_" to decide which paths are cellular services.
  static const std::string cellular_pattern = std::string("/") + kTypeCellular
      + "_";

  // Walk paths in this update and see if they're new to us
  LOG(INFO) << "OnServicesUpdate: " << paths.size() << " service paths";
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

void ServiceManagerImpl::OnNewService(const DBus::Path& path) {
  LOG(INFO) << "OnNewService: " << path;
  DCHECK(GetService(path) == NULL);
  Service *service = Service::NewService(this, connection_, metrics_manager_,
                                         aggregator_, path);
  if (service == NULL) {
    LOG(ERROR) << "OnNewService: couldn't create service for " << path;
    return;
  }
  services_[path] = service;
}

void ServiceManagerImpl::OnDefaultServiceUpdate(const DBus::Path *path) {
  LOG(INFO) << "OnDefaultServiceUpdate: new default service = "
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

void ServiceManagerImpl::ClearDefaultCellularService() {
  LOG(INFO) << "ClearDefaultCellularService";
  if (default_cellular_service_ != NULL) {
    DCHECK(default_cellular_service_->IsDefaultService());
    default_cellular_service_->OnDefaultServiceUpdate(false);
    default_cellular_service_ = NULL;
  }
}

void ServiceManagerImpl::SetDefaultCellularService(Service *service) {
  LOG(INFO) << "SetDefaultCellularService: "
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

}  // namespace cashew
