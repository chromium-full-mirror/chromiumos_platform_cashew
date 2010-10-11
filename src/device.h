// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_DEVICE_H_
#define SRC_DEVICE_H_

#include <string>

#include <base/basictypes.h>  // NOLINT
#include <dbus-c++/dbus.h>  // NOLINT

#include "src/flimflam_device_client_glue.h"

namespace cashew {

class Service;

// represents a cellular device and monitors Flimflam state for that device
class Device : public org::chromium::flimflam::Device_proxy,
               public DBus::IntrospectableProxy,
               public DBus::ObjectProxy {
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

    static const char *kCarrierUnknown;

    // get cellular carrier for this device
    // returns kCarrierUnknown if we don't know
    virtual const std::string& GetCarrier() const;

    // Flimflam Device D-Bus Proxy methods
    virtual void PropertyChanged(const std::string& property_name,
                                 const DBus::Variant& new_value);

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

    // convert type string to Type enum value
    Type TypeFromString(const std::string& type) const;

    // we've received updated Cellular.Carrier info from Flimflam
    void OnCarrierUpdate(const std::string& carrier);

    // we've received updated Type info from Flimflam
    void OnTypeUpdate(const std::string& type);

    // get device properties from Flimflam
    void GetDeviceProperties();

    DISALLOW_COPY_AND_ASSIGN(Device);
};

}  // namespace cashew

#endif  // SRC_DEVICE_H_
