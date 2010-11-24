// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(vlaviano): update accessor style (GetFoo/SetFoo -> foo)

#ifndef SRC_SERVICE_H_
#define SRC_SERVICE_H_

#include <glib.h>

#include <map>
#include <string>
#include <vector>

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

// vector of Device objects
typedef std::vector<Device*> DeviceList;

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

    // convert type string to Type enum value
    static Type TypeFromString(const std::string& type);

    // get Device object for this service
    // this can return NULL if we don't yet have Device info
    virtual Device* GetDevice() const;

    // get data plan info, formatted for D-Bus
    // filters out inactive (future, expired, or consumed) plans
    virtual DBusDataPlanList GetDBusDataPlans() const;

    // do we think that we're the default service at this time?
    virtual bool IsDefaultService() const;

    typedef enum {
      kConnectivityStateUnknown = 0,
      kConnectivityStateRestricted,
      kConnectivityStateUnrestricted,
      kConnectivityStateNone,
    } ConnectivityState;

    // get connectivity state for this service
    virtual ConnectivityState GetConnectivityState() const;

    // Flimflam Service D-Bus Proxy methods

    // receive incoming PropertyChanged D-Bus signal from Flimflam service
    virtual void PropertyChanged(const std::string& property_name,
                                 const DBus::Variant& new_value);

    // Device methods

    // we've received updated Cellular.Carrier info from our child Device
    virtual void OnCarrierUpdate(const std::string& carrier);

    // we've received updated byte counter info from our child Device
    virtual void OnByteCounterUpdate(uint64 rx_bytes, uint64 tx_bytes);

    // DataPlanProviderDelegate methods

    // a request to our carrier usage API proxy has completed
    virtual void OnRequestComplete(const DataPlanProvider *provider,
                                   bool successful,
                                   const Value *parsed_usage_update);

    // Service Manager methods

    // we've received an update from our parent about whether or not we're the
    // default service
    virtual void OnDefaultServiceUpdate(bool is_default_service);

    // we've received an update from our parent that Flimflam has come online
    virtual void OnFlimflamOnline();

    // we've received an update from our parent that Flimflam has gone offline
    virtual void OnFlimflamOffline();

    // glib integration interface

    // get idle/timer source id
    virtual guint GetGetPropertiesSourceId() const;

    // set idle/timer source id
    virtual void SetGetPropertiesSourceId(guint source_id);

    // are we in the process of retrying our GetProperties call?
    virtual bool RetryingGetProperties() const;

    // set the retrying flag
    virtual void OnRetryingGetProperties(bool retrying);

    // get deferred device deletion source id
    virtual guint GetDeferredDeviceDeletionSourceId() const;

    // set deferred device deletion source id
    virtual void SetDeferredDeviceDeletionSourceId(guint source_id);

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

    // do we think that we're the default service?
    bool is_default_service_;

    // GetProperties timer glib source id
    // 0 means no source
    guint get_properties_source_id_;

    // are we in the process of retrying our GetProperties call?
    // this flag exists to distinguish between our initial g_idle_add call
    // and our subsequent timer calls
    bool retrying_get_properties_;

    // Service connectivity state
    // finer-grained connectivity info applicable when |state_| == kStateReady
    ConnectivityState connectivity_state_;

    // Device objects pending deletion from the main loop
    // We need to do this to work around a dbus-c++ deadlock issue that arises
    // when we send a D-Bus message from a D-Bus callback
    DeviceList devices_pending_deletion_;

    // glib source id for deferred device deletion
    guint deferred_device_deletion_source_id_;

    // deletes members of |devices_pending_deletion_| immediately
    void DeletePendingDevices();

    // schedule a device for later deletion from the main loop
    void ScheduleDeviceForLaterDeletion(Device *device);

    // glib integration: static wrapper for DeletePendingDevices
    // takes object ptr as data and invokes object->DeletePendingDevices()
    static gboolean StaticDeletePendingDevicesCallback(gpointer data);

    // convert connectivity state string to ConnectivityState enum value
    static ConnectivityState ConnectivityStateFromString(
        const std::string& connectivity_state);

    // convert state string to State enum value
    static State StateFromString(const std::string& state);

    // we've received updated Device info from Flimflam
    void OnDeviceUpdate(const DBus::Path& device_path);

    // we've received updated Connectivity State info from Flimflam
    void OnConnectivityStateUpdate(const std::string& connectivity_state);

    // we've received updated State info from Flimflam
    void OnStateUpdate(const std::string& state);

    // we've received updated Type info from Flimflam
    void OnTypeUpdate(const std::string& type);

    // we've received updated Cellular.UsageUrl info from Flimflam
    void OnUsageUrlUpdate(const std::string& usage_url);

    // glib integration: static wrapper for GetServiceProperties
    // takes object ptr as data and invokes object->GetServiceProperties()
    static gboolean StaticGetServicePropertiesCallback(gpointer data);

    // get service properties from Flimflam
    // returns true on success and false on failure
    bool GetServiceProperties();

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

    // do we think that we're connected at this time?
    bool IsConnected() const;

    // we've just become connected
    void OnConnected();

    // we've just become disconnected
    // NOTE: this means we've entered a state other than kStateReady, not
    // that we're necessarily in kStateDisconnect
    void OnDisconnected();

    // are we currently sending periodic usage API requests?
    bool IsSendingUsageRequests() const;

    // based on our best current knowledge and our policy, should we be
    // sending periodic usage API requests?
    bool ShouldSendUsageRequests() const;

    // stop sending periodic usage API requests
    void StopSendingUsageRequests();

    // start sending periodic usage API requests
    void StartSendingUsageRequests();

    // something has changed, so reevaluate whether or not we should be sending
    // periodic usage API requests and adjust our behavior if necessary
    void ReconsiderSendingUsageRequests();

    // consult policy to determine if we should send an unsolicited update
    void MaybeEmitDataPlansUpdate();

    DISALLOW_COPY_AND_ASSIGN(Service);
};

}  // namespace cashew

#endif  // SRC_SERVICE_H_
