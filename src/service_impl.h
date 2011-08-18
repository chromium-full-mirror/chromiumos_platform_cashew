// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(vlaviano): update accessor style (GetFoo/SetFoo -> foo)

#ifndef SRC_SERVICE_IMPL_H_
#define SRC_SERVICE_IMPL_H_

#include <glib.h>

#include <map>
#include <string>

#include <base/basictypes.h>  // NOLINT
#include <dbus-c++/dbus.h>    // NOLINT

#include "src/data_plan.h"
#include "src/data_plan_provider.h"
#include "src/flimflam_service_client_glue.h"
#include "src/metrics_manager.h"
#include "src/property_changed_handler.h"
#include "src/service.h"

namespace cashew {

class Aggregator;

// represents a cellular service and monitors Flimflam state for that service
class ServiceImpl : public Service,
                    public org::chromium::flimflam::Service_proxy,
                    public DBus::IntrospectableProxy,
                    public DBus::ObjectProxy,
                    public DataPlanProviderDelegate,
                    public PropertyChangedDelegate {
  public:
    ServiceImpl(ServiceManager * const parent,
                DBus::Connection& connection,  // NOLINT
                MetricsManager * const metrics_manager,
                Aggregator * const aggregator,
                const DBus::Path& path);
    virtual ~ServiceImpl();

    // Service methods
    virtual const DBus::Path& GetPath() const;
    virtual State GetState() const;
    virtual Type GetType() const;
    static Type TypeFromString(const std::string& type);
    virtual Device* GetDevice() const;
    virtual DBusDataPlanList GetDBusDataPlans() const;
    virtual bool IsDefaultService() const;

    // Flimflam Service D-Bus Proxy methods

    // receive incoming PropertyChanged D-Bus signal from Flimflam service and
    // schedule deferred processing
    virtual void PropertyChanged(const std::string& property_name,
                                 const DBus::Variant& new_value);

    // PropertyChangedDelegate methods

    virtual void OnPropertyChanged(const PropertyChangedHandler *handler,
                                   const std::string& property_name,
                                   const DBus::Variant& new_value);

    // Device methods

    // we've received updated Cellular.Carrier info from our child Device
    virtual void OnCarrierUpdate(const std::string& carrier);

    // we've received updated byte counter info from our child Device
    virtual void OnByteCounterUpdate(uint64 rx_bytes, uint64 tx_bytes);

    // we've received a notification from our child Device that the
    //     byte counter has been stopped
    virtual void OnByteCounterStopped();

    // DataPlan methods

    // we've received a notification that a data plan for this service expired
    virtual bool OnDataPlanExpired(DataPlan *data_plan);

    // DataPlanProviderDelegate methods

    // a request to our carrier usage API proxy has completed
    virtual void OnRequestComplete(const DataPlanProvider *provider,
                                   bool successful,
                                   const Value *parsed_usage_update);

    // Service Manager methods

    // we've received an update from our parent about whether or not we're the
    // default service
    virtual void OnDefaultServiceUpdate(bool is_default_service);

  private:
    // back pointer to our parent ServiceManager
    ServiceManager * const parent_;

    // D-Bus connection, owned by our creator
    // shared with Device obj that we create
    DBus::Connection& connection_;

    // metrics manager to which we provide interesting stats
    MetricsManager * const metrics_manager_;

    // aggregator
    Aggregator * const aggregator_;

    // byte counts reported on the previous byte counter update
    uint64 prev_rx_bytes_;
    uint64 prev_tx_bytes_;

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

    // Handler for deferred processing of D-Bus PropertyChanged signals.
    // See comments in service_manager.h
    PropertyChangedHandler property_changed_handler_;

    // convert state string to State enum value
    static State StateFromString(const std::string& state);

    // we've received updated Device info from Flimflam
    void OnDeviceUpdate(const DBus::Path& device_path);

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
    static bool IsValidCrosUsageStatus(const std::string& status);

    // convert a usage API status result string to an UsageRequestStatus enum
    // value for the Metrics Manager
    static MetricsManager::UsageRequestStatus MetricsEnumFromStatusString(
        const std::string& status);

    // usage API request returned an error result
    void OnCrosUsageErrorResult(const std::string& status);

    // does |state| represent a connected state?
    static bool IsConnectedState(State state);

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

    DISALLOW_COPY_AND_ASSIGN(ServiceImpl);
};

}  // namespace cashew

#endif  // SRC_SERVICE_IMPL_H_
