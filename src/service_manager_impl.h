// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_SERVICE_MANAGER_IMPL_H_
#define SRC_SERVICE_MANAGER_IMPL_H_

#include <glib.h>

#include <map>
#include <string>
#include <vector>

#include <base/basictypes.h>  // NOLINT
#include <dbus-c++/dbus.h>  // NOLINT

#include "src/flimflam_manager_client_glue.h"
#include "src/property_changed_handler.h"
#include "src/service.h"
#include "src/service_manager.h"

namespace cashew {

// monitors Flimflam and maintains a collection of cellular service objects
class ServiceManagerImpl : public ServiceManager,
                           public org::chromium::flimflam::Manager_proxy,
                           public DBus::IntrospectableProxy,
                           public DBus::ObjectProxy,
                           public PropertyChangedDelegate {
  public:
    ServiceManagerImpl(DBus::Connection& connection,  // NOLINT
                       GMainLoop * const main_loop);
    virtual ~ServiceManagerImpl();

    // ServiceManager methods
    virtual const Service* GetService(const std::string& service_path) const;
    virtual void SetCashewServer(CashewServer *cashew_server);
    virtual Service::Type GetDefaultTechnology() const;

    // Flimflam Manager D-Bus Proxy methods

    // receive incoming PropertyChanged D-Bus signal from Flimflam and
    // schedule deferred processing
    virtual void PropertyChanged(const std::string& property_name,
                                 const DBus::Variant& new_value);

    // receive incoming StateChanged D-Bus signal from Flimflam, convert to a
    // PropertyChanged signal, and schedule deferred processing
    virtual void StateChanged(const std::string& new_state_string);

    // PropertyChangedDelegate methods

    // Perform deferred processing for an incoming PropertyChanged D-Bus signal
    // from Flimflam
    virtual void OnPropertyChanged(const PropertyChangedHandler *handler,
                                   const std::string& property_name,
                                   const DBus::Variant& new_value);

    // Service methods
    virtual void EmitDataPlansUpdate(const Service& service);
    virtual void OnDataPlanInactive(const Service& service,
                                    const DataPlan& plan);

  private:
    // D-Bus connection owned by our creator
    // shared with Service objs that we create
    DBus::Connection& connection_;

    // glib main loop
    GMainLoop * const main_loop_;

    // collection of Service objs representing cellular services
    ServiceMap services_;

    // Cashew server implementing our front-end interface
    CashewServer *cashew_server_;

    // default technology most recently reported by Flimflam
    Service::Type default_technology_;

    // the cellular service, if any, that we think is the default service
    // can be NULL if we don't know the default service or a non-cellular
    // service is the default
    Service *default_cellular_service_;

    // GetProperties timer glib source id
    // 0 means no source
    guint get_properties_source_id_;

    // are we in the process of retrying our GetProperties call?
    // this flag exists to distinguish between our initial g_idle_add call
    // and our subsequent timer calls
    bool retrying_get_properties_;

    // Handler for deferred processing of D-Bus PropertyChanged signals.
    //
    // We construct and enqueue PropertyChangedSignal objects as D-Bus signals
    // arrive, and the handler calls our OnPropertyChanged method (part of our
    // PropertyChangedDelegate interface) later from the glib main loop.
    //
    // This is done to avoid a libdbus-c++ deadlock that can occur when dbus
    // messages are sent from within a dbus callback.
    PropertyChangedHandler property_changed_handler_;

    // delete a collection of services
    void DeleteServices(ServiceMap *service_map);

    // glib integration: static wrapper for GetFlimflamProperties
    // takes object ptr as data and invokes object->GetFlimflamProperties()
    static gboolean StaticGetFlimflamPropertiesCallback(gpointer data);

    // get interesting properties from Flimflam such as Services and
    // DefaultTechnology
    // returns true on success and false on failure
    bool GetFlimflamProperties();

    // we've received updated DefaultTechnology info from Flimflam
    void OnDefaultTechnologyUpdate(const std::string& default_technology);

    // we've received updated Services info from Flimflam
    void OnServicesUpdate(const ServicePathList& paths);

    // Flimflam has started advertising a new service
    void OnNewService(const DBus::Path& path);

    // we've received updated default service info from Flimflam
    // NULL |path| means that there is no default service
    void OnDefaultServiceUpdate(const DBus::Path *path);

    // if |default_cellular_service_| points to a service, tell it
    // that it is no longer the default service and clear our ptr
    void ClearDefaultCellularService();

    // tell |service| that it is now the default service and set our
    //   |default_cellular_service_| ptr to point to it.
    // if there is already a default service, first clear it.
    // NULL |service| is ok and is equivalent to ClearDefaultCellularService
    // no-op if |service| == |default_cellular_service_|
    void SetDefaultCellularService(Service *service);

    DISALLOW_COPY_AND_ASSIGN(ServiceManagerImpl);
};

}  // namespace cashew

#endif  // SRC_SERVICE_MANAGER_IMPL_H_
