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

namespace cashew {

class Service;

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

    // Flimflam Manager D-Bus Proxy methods

    // receive incoming PropertyChanged D-Bus signal from Flimflam
    virtual void PropertyChanged(const std::string& property_name,
                                 const DBus::Variant& new_value);

    // receive incoming StateChanged D-Bus signal from Flimflam
    virtual void StateChanged(const std::string& new_state);

  private:
    // D-Bus connection owned by our creator
    // shared with Service objs that we create
    DBus::Connection& connection_;

    // collection of Service objs representing cellular services
    ServiceMap services_;

    // delete a collection of services
    void DeleteServices(ServiceMap *service_map);

    // get Services info from Flimflam
    void GetFlimflamServices();

    // we've received updated Services info from Flimflam
    void OnServicesUpdate(const ServicePathList& paths);

    // Flimflam has started advertising a new service
    void OnNewService(const DBus::Path& path);

    DISALLOW_COPY_AND_ASSIGN(ServiceManager);
};

}  // namespace cashew

#endif  // SRC_SERVICE_MANAGER_H_
