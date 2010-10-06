// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_SERVICE_H_
#define SRC_SERVICE_H_

#include <map>
#include <string>

#include <base/basictypes.h>  // NOLINT
#include <dbus-c++/dbus.h>    // NOLINT

#include "src/data_plan.h"
#include "src/flimflam_service_client_glue.h"

namespace cashew {

class Device;

// map of key, value pairs representing service properties received via D-Bus
typedef std::map<std::string, DBus::Variant> PropertyMap;

// represents a cellular service and monitors Flimflam state for that service
class Service : public org::chromium::flimflam::Service_proxy,
                public DBus::IntrospectableProxy,
                public DBus::ObjectProxy {
  public:
    Service(DBus::Connection& connection, const DBus::Path& path);  // NOLINT
    virtual ~Service();

    // get D-Bus path for this service
    virtual const DBus::Path& GetPath() const;

    // valid state values for this service
    typedef enum {
      kStateUnknown = 0,
      kStateIdle,
      kStateCarrier,
      kStateAssociation,
      kStateConfiguration,
      kStateReady,
      kStateDisconnect,
      kStateFailure,
      kStateActivationFailure,
    } State;

    // get state for this service
    virtual State GetState() const;

    // valid type values for this service
    typedef enum {
      kTypeUnknown = 0,
      kTypeEthernet,
      kTypeWifi,
      kTypeWimax,
      kTypeBluetooth,
      kTypeCellular,
    } Type;

    // get type for this service
    // NOTE: for now, should always be kTypeCellular
    virtual Type GetType() const;

    // get Device object for this service
    // this can return NULL if we don't yet have Device info
    virtual Device* GetDevice() const;

    // get data plan info, formatted for D-Bus
    virtual DBusDataPlanList GetDBusDataPlans() const;

    // Flimflam Service D-Bus Proxy methods

    // receive incoming PropertyChanged D-Bus signal from Flimflam service
    virtual void PropertyChanged(const std::string& property_name,
                                 const DBus::Variant& new_value);

  private:
    // D-Bus connection, owned by our creator
    // shared with Device obj that we create
    DBus::Connection& connection_;

    // D-Bus path for Flimflam service that we represent
    // this is our unique identifier
    const DBus::Path path_;

    // Service connection state
    State state_;

    // Service type
    Type type_;

    // Device corresponding to our service
    Device *device_;

    // cached data plan info
    DataPlanList data_plans_;

    // TODO(vlaviano): reference to back end carrier plugin
    // TODO(vlaviano): refresh data plan info from carrier plugin

    // convert state string to State enum value
    State StateFromString(const std::string& state) const;

    // convert type string to Type enum value
    Type TypeFromString(const std::string& type) const;

    // we've received updated Device info from Flimflam
    void OnDeviceUpdate(const DBus::Path& device_path);

    // we've received updated State info from Flimflam
    void OnStateUpdate(const std::string& state);

    // we've received updated Type info from Flimflam
    void OnTypeUpdate(const std::string& type);

    // get service properties from Flimflam
    void GetServiceProperties();

    // clear data_plans_ list and delete data plan objects
    void DeleteDataPlans();

    // create a hardcoded data plan and add it to data_plans_ list
    // for debugging
    void AddHardcodedDataPlan();

    DISALLOW_COPY_AND_ASSIGN(Service);
};

}  // namespace cashew

#endif  // SRC_SERVICE_H_
