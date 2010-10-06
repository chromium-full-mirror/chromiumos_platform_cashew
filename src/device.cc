// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/device.h"

#include <glog/logging.h>

#include "src/service.h"

namespace cashew {

const char* Device::kCarrierUnknown = "Unknown";

// Flimflam Device D-Bus identifiers
static const char* kFlimflamDeviceName = "org.chromium.flimflam";

// Flimflam Device property names
static const char* kFlimflamDeviceCarrierProperty = "Cellular.Carrier";
static const char* kFlimflamDeviceTypeProperty = "Type";

// Flimflam Device on-the-wire Type values
static const char* kFlimflamDeviceTypeEthernet = "ethernet";
static const char* kFlimflamDeviceTypeWifi = "wifi";
static const char* kFlimflamDeviceTypeWimax = "wimax";
static const char* kFlimflamDeviceTypeBluetooth = "bluetooth";
static const char* kFlimflamDeviceTypeGPS = "gps";
static const char* kFlimflamDeviceTypeCellular = "cellular";
// TODO(vlaviano): what does vendor look like?
static const char* kFlimflamDeviceTypeVendor = "";

Device::Device(DBus::Connection& connection,  // NOLINT
               const DBus::Path& path)
    : DBus::ObjectProxy(connection, path, kFlimflamDeviceName),
      path_(path), type_(kTypeUnknown), carrier_(kCarrierUnknown) {
  // init our state with a GetProperties() call to our Flimflam service path
  // we'll update this state by monitoring PropertyChanged signals
  GetDeviceProperties();
}

Device::~Device() {
}

const DBus::Path& Device::GetPath() const {
  return path_;
}

Device::Type Device::GetType() const {
  return type_;
}

const std::string& Device::GetCarrier() const {
  return carrier_;
}

// Flimflam Device D-Bus Proxy methods
void Device::PropertyChanged(const std::string& property_name,
                             const DBus::Variant& new_value) {
  DLOG(INFO) << path_ << ": PropertyChanged: property_name = " << property_name;
  if (property_name.compare(kFlimflamDeviceCarrierProperty) == 0) {
    OnCarrierUpdate(new_value.reader().get_string());
  } else if (property_name.compare(kFlimflamDeviceTypeProperty) == 0) {
    OnTypeUpdate(new_value.reader().get_string());
  } else {
    // we don't care about this property
  }
}

// Private methods

Device::Type Device::TypeFromString(const std::string& type) const {
  if (type.compare(kFlimflamDeviceTypeEthernet) == 0) {
    return kTypeEthernet;
  }
  if (type.compare(kFlimflamDeviceTypeWifi) == 0) {
    return kTypeWifi;
  }
  if (type.compare(kFlimflamDeviceTypeWimax) == 0) {
    return kTypeWimax;
  }
  if (type.compare(kFlimflamDeviceTypeBluetooth) == 0) {
    return kTypeBluetooth;
  }
  if (type.compare(kFlimflamDeviceTypeGPS) == 0) {
    return kTypeGPS;
  }
  if (type.compare(kFlimflamDeviceTypeCellular) == 0) {
    return kTypeCellular;
  }
  if (type.compare(kFlimflamDeviceTypeVendor) == 0) {
    return kTypeVendor;
  }
  return kTypeUnknown;
}

void Device::OnCarrierUpdate(const std::string& carrier) {
  DLOG(INFO) << path_ << ": OnCarrierUpdate: carrier = " << carrier;
  if (carrier_.compare(kCarrierUnknown) && carrier_.compare(carrier)) {
    LOG(WARNING) << path_ << ": OnCarrierUpdate: carrier change from "
        << carrier_ << " to " << carrier << "!";
  }
  // TODO(vlaviano): load backend module for this carrier if needed
  carrier_ = carrier;
}

void Device::OnTypeUpdate(const std::string& type) {
  DLOG(INFO) << path_ << ": OnTypeUpdate: type = " << type;
  type_ = TypeFromString(type);
  if (type_ != kTypeCellular) {
    LOG(WARNING) << path_ << ": OnTypeUpdate: type is not cellular!";
  }
}

void Device::GetDeviceProperties() {
  DLOG(INFO) << path_ << ": GetDeviceProperties";
  PropertyMap properties;
  // dbus-c++ throws exceptions
  // invoke the "Existing Non-conformant Code" clause of the style guide and
  // isolate the rest of the system from this
  try {
    // TODO(vlaviano): make this call asynchronous
    properties = GetProperties();
  } catch (DBus::Error& error) {  // NOLINT
    LOG(WARNING) << path_
        << ": GetDeviceProperties: GetProperties() -> Exception: "
        << error.name() << ": " << error.message();
    // TODO(vlaviano): schedule another attempt later
    return;
  } catch (...) {  // NOLINT
    LOG(WARNING) << path_
        << ": GetDeviceProperties: GetProperties() -> Exception";
    // TODO(vlaviano): schedule another attempt later
    return;
  }
  DLOG(INFO) << "Received " << properties.size() << " properties";

  // grab the properties in which we're interested
  PropertyMap::const_iterator it;
  it = properties.find(kFlimflamDeviceTypeProperty);
  if (it != properties.end()) {
    const DBus::Variant& value = static_cast<DBus::Variant>(it->second);
    OnTypeUpdate(value.reader().get_string());
  } else {
    DLOG(WARNING) << path_ << ": GetDeviceProperties: no Type property";
  }
  // don't expect to find Cellular.* properties if we're not a cellular device
  if (type_ != kTypeCellular) {
    return;
  }
  it = properties.find(kFlimflamDeviceCarrierProperty);
  if (it != properties.end()) {
    const DBus::Variant& value = static_cast<DBus::Variant>(it->second);
    OnCarrierUpdate(value.reader().get_string());
  } else {
    DLOG(WARNING) << path_ << ": GetDeviceProperties: no Carrier property";
  }
}

}  // namespace cashew
