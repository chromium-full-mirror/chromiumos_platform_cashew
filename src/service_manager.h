// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_SERVICE_MANAGER_H_
#define SRC_SERVICE_MANAGER_H_

#include <map>
#include <string>
#include <vector>

#include <base/basictypes.h>  // NOLINT
#include <dbus-c++/dbus.h>  // NOLINT

#include "src/flimflam_manager_client_glue.h"
#include "src/service.h"

namespace cashew {

class CashewServer;

// map of service path names to Service objects
typedef std::map<std::string, Service*> ServiceMap;

// vector of DBus::Path objects (strings) representing service path names
typedef std::vector<DBus::Path> ServicePathList;

// monitors Flimflam and maintains a collection of cellular service objects
class ServiceManager : public org::chromium::flimflam::Manager_proxy,
                       public DBus::IntrospectableProxy,
                       public DBus::ObjectProxy {
  public:
    explicit ServiceManager(DBus::Connection& connection);  // NOLINT
    virtual ~ServiceManager();

    // look up a service by its path name
    // returns NULL if service is not found
    virtual const Service* GetService(const std::string& service_path) const;

    // set Cashew server
    // we'll talk to it when we want to emit updates to the world on behalf of
    // our child Services.
    // it's ok to clear this by setting it to NULL, in which case our updates
    // will go into a black hole.
    virtual void SetCashewServer(CashewServer *cashew_server);

    // return our latest info on the current default technology
    virtual Service::Type GetDefaultTechnology() const;

    // valid Flimflam global connectivity state values
    typedef enum {
      kConnectivityStateUnknown = 0,
      kConnectivityStateOffline,
      kConnectivityStateConnected,
      kConnectivityStateOnline,
    } ConnectivityState;

    // return our latest info on Flimflam's global connectivity state
    virtual ConnectivityState GetConnectivityState() const;

    // does |state| represent an offline state
    static bool IsOfflineConnectivityState(ConnectivityState state);

    // does |state| represent an online state
    static bool IsOnlineConnectivityState(ConnectivityState state);

    // Flimflam Manager D-Bus Proxy methods

    // receive incoming PropertyChanged D-Bus signal from Flimflam
    virtual void PropertyChanged(const std::string& property_name,
                                 const DBus::Variant& new_value);

    // receive incoming StateChanged D-Bus signal from Flimflam
    virtual void StateChanged(const std::string& new_state_string);

    // Service methods

    // a child Service wants us to emit an update about its data plans
    virtual void EmitDataPlansUpdate(const Service& service);

  private:
    // D-Bus connection owned by our creator
    // shared with Service objs that we create
    DBus::Connection& connection_;

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

    // global connectivity state most recently reported by Flimflam
    ConnectivityState connectivity_state_;

    // delete a collection of services
    void DeleteServices(ServiceMap *service_map);

    // get interesting properties from Flimflam such as Services and
    // DefaultTechnology
    void GetFlimflamProperties();

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

    // convert connectivity state string to ConnectivityState enum value
    static ConnectivityState ConnectivityStateFromString(
        const std::string& state);

    // Flimflam has told us that it has come online
    void OnFlimflamOnline();

    // Flimflam has told us that it has gone offline
    void OnFlimflamOffline();

    DISALLOW_COPY_AND_ASSIGN(ServiceManager);
};

}  // namespace cashew

#endif  // SRC_SERVICE_MANAGER_H_
