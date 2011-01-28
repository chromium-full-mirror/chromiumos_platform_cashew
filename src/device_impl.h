// Copyright (c) 2010, 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_DEVICE_IMPL_H_
#define SRC_DEVICE_IMPL_H_

#include <glib.h>

#include <string>

#include <base/basictypes.h>  // NOLINT
#include <dbus-c++/dbus.h>  // NOLINT

#include "src/byte_counter.h"
#include "src/device.h"
#include "src/flimflam_device_client_glue.h"
#include "src/property_changed_handler.h"

namespace cashew {

// represents a cellular device and monitors Flimflam state for that device
class DeviceImpl : public Device,
                   public org::chromium::flimflam::Device_proxy,
                   public DBus::IntrospectableProxy,
                   public DBus::ObjectProxy,
                   public ByteCounterDelegate,
                   public PropertyChangedDelegate {
  public:
    DeviceImpl(Service * const parent, DBus::Connection& connection,  // NOLINT
               const DBus::Path& path);
    virtual ~DeviceImpl();

    // Device methods
    virtual const DBus::Path& GetPath() const;
    virtual Type GetType() const;
    virtual const std::string& GetCarrier() const;
    virtual const std::string& GetInterface() const;

    // Flimflam Device D-Bus Proxy methods

    // receive incoming PropertyChanged D-Bus signal from Flimflam device and
    // schedule deferred processing
    virtual void PropertyChanged(const std::string& property_name,
                                 const DBus::Variant& new_value);

    // PropertyChangedDelegate methods

    virtual void OnPropertyChanged(const PropertyChangedHandler *handler,
                                   const std::string& property_name,
                                   const DBus::Variant& new_value);

    // Service methods
    virtual bool StartByteCounter();
    virtual void StopByteCounter();
    virtual bool ByteCounterRunning() const;

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

    // Handler for deferred processing of D-Bus PropertyChanged signals.
    // See comments in service_manager.h
    PropertyChangedHandler property_changed_handler_;

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

    DISALLOW_COPY_AND_ASSIGN(DeviceImpl);
};

}  // namespace cashew

#endif  // SRC_DEVICE_IMPL_H_
