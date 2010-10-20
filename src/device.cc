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

Device::Device(Service * const parent, DBus::Connection& connection,  // NOLINT
               const DBus::Path& path)
    : DBus::ObjectProxy(connection, path, kFlimflamDeviceName),
      parent_(CHECK_NOTNULL(parent)), path_(path), type_(kTypeUnknown),
      carrier_(kCarrierUnknown) {
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
  if (property_name == kFlimflamDeviceCarrierProperty) {
    OnCarrierUpdate(new_value.reader().get_string());
  } else if (property_name == kFlimflamDeviceTypeProperty) {
    OnTypeUpdate(new_value.reader().get_string());
  } else {
    // we don't care about this property
  }
}

// Private methods

Device::Type Device::TypeFromString(const std::string& type) const {
  if (type == kFlimflamDeviceTypeEthernet) {
    return kTypeEthernet;
  }
  if (type == kFlimflamDeviceTypeWifi) {
    return kTypeWifi;
  }
  if (type == kFlimflamDeviceTypeWimax) {
    return kTypeWimax;
  }
  if (type == kFlimflamDeviceTypeBluetooth) {
    return kTypeBluetooth;
  }
  if (type == kFlimflamDeviceTypeGPS) {
    return kTypeGPS;
  }
  if (type == kFlimflamDeviceTypeCellular) {
    return kTypeCellular;
  }
  if (type == kFlimflamDeviceTypeVendor) {
    return kTypeVendor;
  }
  return kTypeUnknown;
}

void Device::OnCarrierUpdate(const std::string& carrier) {
  DLOG(INFO) << path_ << ": OnCarrierUpdate: carrier = " << carrier;
  // only propagate this to parent Service if there's been a change
  // we expect to see only one such change (from "Unknown")
  if (carrier_ != carrier) {
    if (carrier_ != kCarrierUnknown) {
      LOG(WARNING) << path_ << ": OnCarrierUpdate: carrier change from "
          << carrier_ << " to " << carrier << "!";
    }
    carrier_ = carrier;
    parent_->OnCarrierUpdate(carrier);
  }
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
    DLOG(WARNING) << path_
        << ": GetDeviceProperties: no Cellular.Carrier property";
  }
}

}  // namespace cashew
