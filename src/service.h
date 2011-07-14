// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(vlaviano): update accessor style (GetFoo/SetFoo -> foo)

#ifndef SRC_SERVICE_H_
#define SRC_SERVICE_H_

#include <map>
#include <string>

#include <base/basictypes.h>  // NOLINT
#include <dbus-c++/dbus.h>    // NOLINT

#include "src/data_plan.h"
#include "src/user_aggregator.h"

namespace cashew {

class Device;
class MetricsManager;
class Policy;
class ServiceManager;

// map of key, value pairs representing service properties received via D-Bus
typedef std::map<std::string, DBus::Variant> PropertyMap;

// Service interface
// represents a cellular service and monitors Flimflam state for that service
class Service {
  public:
    Service() {}
    virtual ~Service() {}

    // get D-Bus path for this service
    virtual const DBus::Path& GetPath() const = 0;

    // valid state values for this service
    typedef enum {
      kStateUnknown = 0,
      kStateIdle,
      kStateCarrier,
      kStateAssociation,
      kStateConfiguration,
      kStateReady,
      kStatePortal,
      kStateOnline,
      kStateDisconnect,
      kStateFailure,
      kStateActivationFailure,
    } State;

    // get state for this service
    virtual State GetState() const = 0;

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
    virtual Type GetType() const = 0;

    // convert type string to Type enum value
    static Type TypeFromString(const std::string& type);

    // get Device object for this service
    // this can return NULL if we don't yet have Device info
    virtual Device* GetDevice() const = 0;

    // get data plan info, formatted for D-Bus
    // filters out inactive (future, expired, or consumed) plans
    virtual DBusDataPlanList GetDBusDataPlans() const = 0;

    // do we think that we're the default service at this time?
    virtual bool IsDefaultService() const = 0;

    // Device methods

    // we've received updated Cellular.Carrier info from our child Device
    virtual void OnCarrierUpdate(const std::string& carrier) = 0;

    // we've received updated byte counter info from our child Device
    virtual void OnByteCounterUpdate(uint64 rx_bytes, uint64 tx_bytes) = 0;

    // Service Manager methods

    // we've received an update from our parent about whether or not we're the
    // default service
    virtual void OnDefaultServiceUpdate(bool is_default_service) = 0;

    // factory
    static Service* NewService(ServiceManager * const parent,
                               DBus::Connection& connection,  // NOLINT
                               MetricsManager * const metrics_manager,
                               UserAggregator * const aggregator,
                               const DBus::Path& path);

  private:
    DISALLOW_COPY_AND_ASSIGN(Service);
};

}  // namespace cashew

#endif  // SRC_SERVICE_H_
