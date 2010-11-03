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

// GetDeviceProperties retry interval
static const guint kSecondsPerMinute = 60;
static const guint kGetDevicePropertiesIntervalSeconds = 1 * kSecondsPerMinute;

Device::Device(Service * const parent, DBus::Connection& connection,  // NOLINT
               const DBus::Path& path)
    : DBus::ObjectProxy(connection, path, kFlimflamDeviceName),
      parent_(CHECK_NOTNULL(parent)), path_(path), type_(kTypeUnknown),
      carrier_(kCarrierUnknown), get_properties_source_id_(0),
      retrying_get_properties_(false) {
  // schedule a GetProperties() call to our Flimflam service path to init state
  // we'll keep trying periodically until we succeed
  // we'll subsequently update this state by monitoring PropertyChanged signals
  get_properties_source_id_ =
      g_idle_add(StaticGetDevicePropertiesCallback, this);
  if (get_properties_source_id_ == 0) {
    LOG(ERROR) << path_ << ": ctor: g_idle_add failed";
  }
}

Device::~Device() {
  if (get_properties_source_id_ != 0 &&
      !g_source_remove(get_properties_source_id_)) {
    DLOG(WARNING) << path_ << ": dtor: g_source_remove failed";
  }
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

// glib integration interface

guint Device::GetGetPropertiesSourceId() const {
  return get_properties_source_id_;
}

void Device::SetGetPropertiesSourceId(guint source_id) {
  get_properties_source_id_ = source_id;
}

bool Device::RetryingGetProperties() const {
  return retrying_get_properties_;
}

void Device::OnRetryingGetProperties(bool retrying) {
  retrying_get_properties_ = retrying;
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

// static
gboolean Device::StaticGetDevicePropertiesCallback(gpointer data) {
  Device *device = reinterpret_cast<Device*>(data);
  CHECK_NOTNULL(device);
  DCHECK_NE(device->GetGetPropertiesSourceId(), 0);
  if (!device->GetDeviceProperties()) {
    // call failed, so try again later
    DLOG(WARNING) << device->GetPath()
        << ": StaticGetDevicePropertiesCallback: "
        << "GetDeviceProperties failed, will retry in "
        << kGetDevicePropertiesIntervalSeconds << " secs";
    // if we've arrived here from our first g_idle_add call, set up a timer
    if (!device->RetryingGetProperties()) {
      guint source_id =
          g_timeout_add_seconds(kGetDevicePropertiesIntervalSeconds,
                                StaticGetDevicePropertiesCallback, device);
      device->SetGetPropertiesSourceId(source_id);
      if (source_id == 0) {
          LOG(ERROR) << device->GetPath()
              << ": StaticGetDevicePropertiesCallback: "
              << "g_timeout_add_seconds failed";
        return FALSE;  // get rid of old glib source and give up
      }
      device->OnRetryingGetProperties(true);
      // get rid of our old glib source, but our new timer will call us
      return FALSE;
    }
    // otherwise, our timer is already set up
    // return TRUE to indicate that we want to be called again later
    return TRUE;
  }
  device->SetGetPropertiesSourceId(0);
  device->OnRetryingGetProperties(false);
  return FALSE;  // we succeeded, so don't repeat
}

bool Device::GetDeviceProperties() {
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
    return false;
  } catch (...) {  // NOLINT
    LOG(WARNING) << path_
        << ": GetDeviceProperties: GetProperties() -> Exception";
    return false;
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
    return true;
  }
  it = properties.find(kFlimflamDeviceCarrierProperty);
  if (it != properties.end()) {
    const DBus::Variant& value = static_cast<DBus::Variant>(it->second);
    OnCarrierUpdate(value.reader().get_string());
  } else {
    DLOG(WARNING) << path_
        << ": GetDeviceProperties: no Cellular.Carrier property";
  }
  return true;
}

}  // namespace cashew
