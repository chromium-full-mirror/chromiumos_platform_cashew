// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_SERVICE_H_
#define SRC_SERVICE_H_

#include <glib.h>

#include <map>
#include <string>

#include <base/basictypes.h>  // NOLINT
#include <dbus-c++/dbus.h>    // NOLINT

#include "src/data_plan.h"
#include "src/data_plan_provider.h"
#include "src/flimflam_service_client_glue.h"

namespace cashew {

class Device;
class Policy;
class ServiceManager;

// map of key, value pairs representing service properties received via D-Bus
typedef std::map<std::string, DBus::Variant> PropertyMap;

// represents a cellular service and monitors Flimflam state for that service
class Service : public org::chromium::flimflam::Service_proxy,
                public DBus::IntrospectableProxy,
                public DBus::ObjectProxy,
                public DataPlanProviderDelegate {
  public:
    Service(ServiceManager * const parent,
            DBus::Connection& connection,  // NOLINT
            const DBus::Path& path);
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

    // Device methods

    // we've received updated Cellular.Carrier info from our child Device
    virtual void OnCarrierUpdate(const std::string& carrier);

    // DataPlanProviderDelegate methods

    // a request to our carrier usage API proxy has completed
    virtual void OnRequestComplete(const DataPlanProvider *provider,
                                   bool successful,
                                   const Value *parsed_usage_update);

  private:
    // back pointer to our parent ServiceManager
    ServiceManager * const parent_;

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

    // carrier usage API proxy
    DataPlanProvider *provider_;

    // carrier usage API URL
    // empty string represents "unknown"
    std::string usage_url_;

    // do we have an outstanding usage API request
    bool request_in_progress_;

    // periodic update timeout
    GSource *update_timeout_source_;

    // carrier policy
    Policy *policy_;

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

    // we've received updated Cellular.UsageUrl info from Flimflam
    void OnUsageUrlUpdate(const std::string& usage_url);

    // get service properties from Flimflam
    void GetServiceProperties();

    // clear a DataPlanList and delete its data plan objects
    void DeleteDataPlans(DataPlanList *data_plans);

    // create a hardcoded data plan and add it to data_plans_ list
    // for debugging
    void AddHardcodedDataPlan();

    // ask provider to request a usage update if it makes sense
    void RequestUsageUpdate();

    // cancel any pending usage API requests that we have
    void CancelPendingRequests();

    // update timeout callback
    gboolean UpdateTimeoutCallback();

    // glib integration: static wrapper for UpdateTimeoutCallback
    // takes object ptr as data and invokes object->UpdateTimeoutCallback()
    static gboolean StaticUpdateTimeoutCallback(gpointer data);

    // create and start an update timer that fires periodically
    // |provider_| and |policy_| must both be non-NULL
    // the timer interval is retrieved from the current policy
    // returns true on success and false on failure
    bool CreateUpdateTimer();

    // stop and destroy the update timer if it exists
    void DestroyUpdateTimer();

    // delete carrier usage API proxy and policy if they exist
    // this also destroys the update timer and cancels pending requests
    // NOTE: this does not delete usage url (it's considered service state)
    void DeleteCarrierState();

    // does |status| contain a valid usage API status result string?
    bool IsValidCrosUsageStatus(const std::string& status) const;

    // usage API request returned an error result
    void OnCrosUsageErrorResult(const std::string& status);

    DISALLOW_COPY_AND_ASSIGN(Service);
};

}  // namespace cashew

#endif  // SRC_SERVICE_H_
