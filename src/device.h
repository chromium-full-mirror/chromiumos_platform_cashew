// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_DEVICE_H_
#define SRC_DEVICE_H_

#include <glib.h>

#include <string>

#include <base/basictypes.h>  // NOLINT
#include <dbus-c++/dbus.h>  // NOLINT

#include "src/byte_counter.h"
#include "src/flimflam_device_client_glue.h"

namespace cashew {

class ByteCounter;
class Service;

// represents a cellular device and monitors Flimflam state for that device
class Device : public org::chromium::flimflam::Device_proxy,
               public DBus::IntrospectableProxy,
               public DBus::ObjectProxy,
               public ByteCounterDelegate {
  public:
    Device(Service * const parent, DBus::Connection& connection,  // NOLINT
           const DBus::Path& path);
    virtual ~Device();

    // get D-Bus path for this device
    virtual const DBus::Path& GetPath() const;

    // valid type values for this device
    typedef enum {
      kTypeUnknown = 0,
      kTypeEthernet,
      kTypeWifi,
      kTypeWimax,
      kTypeBluetooth,
      kTypeGPS,
      kTypeCellular,
      kTypeVendor,
    } Type;

    // get type for this device
    // NOTE: for now, should always be kTypeCellular
    virtual Type GetType() const;

    // get cellular carrier for this device
    // returns empty string if we don't know
    virtual const std::string& GetCarrier() const;

    // get interface name for this device (e.g., "usb0")
    // returns empty string if we don't know
    virtual const std::string& GetInterface() const;

    // Flimflam Device D-Bus Proxy methods

    virtual void PropertyChanged(const std::string& property_name,
                                 const DBus::Variant& new_value);

    // Service methods

    // start a new byte counter from 0
    // there must not be an existing byte counter
    // |parent_| will receive periodic updates via Service::OnByteCounterUpdate
    // returns true on success and false on failure
    virtual bool StartByteCounter();

    // destroy any existing byte counter
    // |parent_| will no longer receive updates after this call
    virtual void StopByteCounter();

    // is the byte counter running?
    virtual bool ByteCounterRunning() const;

    // glib integration interface

    // get idle/timer source id
    virtual guint GetGetPropertiesSourceId() const;

    // set idle/timer source id
    virtual void SetGetPropertiesSourceId(guint source_id);

    // are we in the process of retrying our GetProperties call?
    virtual bool RetryingGetProperties() const;

    // set the retrying flag
    virtual void OnRetryingGetProperties(bool retrying);

    // ByteCounterDelegate methods

    // called when bye counter is updated
    virtual void OnByteCounterUpdate(const ByteCounter *counter,
                                     uint64 rx_bytes, uint64 tx_bytes);

  private:
    // back pointer to our parent Service
    Service * const parent_;

    // D-Bus path for Flimflam device that we represent
    // this is our unique identifier
    const DBus::Path path_;

    // device type
    Type type_;

    // cellular carrier
    std::string carrier_;

    // interface name
    std::string interface_;

    // GetProperties timer glib source id
    // 0 means no source
    guint get_properties_source_id_;

    // are we in the process of retrying our GetProperties call?
    // this flag exists to distinguish between our initial g_idle_add call
    // and our subsequent timer calls
    bool retrying_get_properties_;

    // byte counter for this device
    // can be NULL
    ByteCounter *byte_counter_;

    // is the byte counter running?
    // this is distinct from whether or not byte_counter_ == NULL, since the
    // counter may be logically running but we may not yet know our interface
    // name and therefore may not yet have created a byte counter object.
    bool byte_counter_running_;

    // convert type string to Type enum value
    Type TypeFromString(const std::string& type) const;

    // we've received updated Cellular.Carrier info from Flimflam
    void OnCarrierUpdate(const std::string& carrier);

    // we've received updated Interface info from Flimflam
    void OnInterfaceUpdate(const std::string& interface);

    // we've received updated Type info from Flimflam
    void OnTypeUpdate(const std::string& type);

    // glib integration: static wrapper for GetDeviceProperties
    // takes object ptr as data and invokes object->GetDeviceProperties
    static gboolean StaticGetDevicePropertiesCallback(gpointer data);

    // get device properties from Flimflam
    // returns true on success and false on failure
    bool GetDeviceProperties();

    // create a byte counter object and assign it to |byte_counter_|
    bool CreateByteCounter();

    // delete byte counter object pointed to by |byte_counter_| if any
    void DeleteByteCounter();

    DISALLOW_COPY_AND_ASSIGN(Device);
};

}  // namespace cashew

#endif  // SRC_DEVICE_H_
