// Copyright (c) 2010, 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/device_impl.h"

#include <glog/logging.h>

#include "src/byte_counter.h"
#include "src/service.h"

namespace cashew {

// Flimflam Device D-Bus identifiers
static const char *kFlimflamDeviceName = "org.chromium.flimflam";

// Flimflam Device property names
static const char *kFlimflamDeviceCarrierProperty = "Cellular.Carrier";
static const char *kFlimflamDeviceInterfaceProperty = "Interface";
static const char *kFlimflamDeviceTypeProperty = "Type";

// Flimflam Device on-the-wire Type values
static const char *kFlimflamDeviceTypeEthernet = "ethernet";
static const char *kFlimflamDeviceTypeWifi = "wifi";
static const char *kFlimflamDeviceTypeWimax = "wimax";
static const char *kFlimflamDeviceTypeBluetooth = "bluetooth";
static const char *kFlimflamDeviceTypeGPS = "gps";
static const char *kFlimflamDeviceTypeCellular = "cellular";
// TODO(vlaviano): what does vendor look like?
static const char *kFlimflamDeviceTypeVendor = "";

// GetDeviceProperties retry interval
static const guint kSecondsPerMinute = 60;
static const guint kGetDevicePropertiesIntervalSeconds = 1 * kSecondsPerMinute;

DeviceImpl::DeviceImpl(Service * const parent,
                       DBus::Connection& connection,  // NOLINT
                       const DBus::Path& path)
    : DBus::ObjectProxy(connection, path, kFlimflamDeviceName),
      parent_(CHECK_NOTNULL(parent)), path_(path), type_(kTypeUnknown),
      get_properties_source_id_(0), retrying_get_properties_(false),
      byte_counter_(NULL), byte_counter_running_(false) {
  // schedule a GetProperties() call to our Flimflam service path to init state
  // we'll keep trying periodically until we succeed
  // we'll subsequently update this state by monitoring PropertyChanged signals
  get_properties_source_id_ =
      g_idle_add(StaticGetDevicePropertiesCallback, this);
  if (get_properties_source_id_ == 0) {
    LOG(ERROR) << path_ << ": ctor: g_idle_add failed";
  }
  property_changed_handler_.delegate(this);
}

DeviceImpl::~DeviceImpl() {
  StopByteCounter();
  if (get_properties_source_id_ != 0 &&
      !g_source_remove(get_properties_source_id_)) {
    DLOG(WARNING) << path_ << ": dtor: g_source_remove failed";
  }
}

const DBus::Path& DeviceImpl::GetPath() const {
  return path_;
}

Device::Type DeviceImpl::GetType() const {
  return type_;
}

const std::string& DeviceImpl::GetCarrier() const {
  return carrier_;
}

const std::string& DeviceImpl::GetInterface() const {
  return interface_;
}

// Flimflam Device D-Bus Proxy methods

void DeviceImpl::PropertyChanged(const std::string& property_name,
                                 const DBus::Variant& new_value) {
  DLOG(INFO) << path_ << ": PropertyChanged: property_name = " << property_name;
  // Queue a tuple representing this signal for later processing from the glib
  // main loop. We do this to avoid libdbus-c++ deadlocks that can occur when
  // sending a dbus message from within a dbus callback like this one.
  PropertyChangedSignal signal(property_name, new_value);
  property_changed_handler_.EnqueueSignal(signal);
}

// PropertyChangedDelegate methods

void DeviceImpl::OnPropertyChanged(const PropertyChangedHandler *handler,
                                   const std::string& property_name,
                                   const DBus::Variant& new_value) {
  DCHECK(handler == &property_changed_handler_);
  DLOG(INFO) << path_ << ": OnPropertyChanged: property_name = "
      << property_name;
  if (property_name == kFlimflamDeviceCarrierProperty) {
    OnCarrierUpdate(new_value.reader().get_string());
  } else if (property_name == kFlimflamDeviceInterfaceProperty) {
    OnInterfaceUpdate(new_value.reader().get_string());
  } else if (property_name == kFlimflamDeviceTypeProperty) {
    OnTypeUpdate(new_value.reader().get_string());
  } else {
    // we don't care about this property
  }
}

// Service methods

bool DeviceImpl::StartByteCounter() {
  if (byte_counter_running_) {
    DLOG(WARNING) << path_ << ": StartByteCounter: counter already running";
    return false;
  }
  byte_counter_running_ = true;
  if (interface_.empty()) {
    // if we don't yet know our interface name, we report success but hold off
    // creating a byte counter object; we'll do so later in OnInterfaceUpdate
    DLOG(INFO) << path_
        << ": StartByteCounter: no interface name, delaying counter creation";
    return true;
  }
  if (!CreateByteCounter()) {
    DLOG(WARNING) << path_
      << ": StartByteCounter: couldn't create byte counter for " << interface_;
    byte_counter_running_ = false;
    return false;
  }
  DLOG(INFO) << path_ << ": StartByteCounter: created byte counter for "
      << interface_;
  return true;
}

void DeviceImpl::StopByteCounter() {
  if (byte_counter_running_) {
    DeleteByteCounter();
    byte_counter_running_ = false;
  }
  DCHECK(byte_counter_ == NULL);
}

bool DeviceImpl::ByteCounterRunning() const {
  return byte_counter_running_;
}

// ByteCounterDelegate methods

void DeviceImpl::OnByteCounterUpdate(const ByteCounter *counter,
                                     uint64 rx_bytes, uint64 tx_bytes) {
  DCHECK(byte_counter_running_);
  DCHECK(byte_counter_ != NULL);
  DCHECK(counter == byte_counter_);
  DLOG(INFO) << path_ << ": OnByteCounterUpdate: rx_bytes = " << rx_bytes
      << ", tx_bytes = " << tx_bytes;
  parent_->OnByteCounterUpdate(rx_bytes, tx_bytes);
}

// Private methods

Device::Type DeviceImpl::TypeFromString(const std::string& type) const {
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

void DeviceImpl::OnCarrierUpdate(const std::string& carrier) {
  DLOG(INFO) << path_ << ": OnCarrierUpdate: carrier = " << carrier;
  // only propagate this to parent Service if there's been a change
  // we expect to see only one such change (from "Unknown")
  if (carrier_ == carrier) {
    return;
  }
  if (!carrier_.empty()) {
    LOG(WARNING) << path_ << ": OnCarrierUpdate: carrier change from "
        << carrier_ << " to " << carrier << "!";
  }
  carrier_ = carrier;
  parent_->OnCarrierUpdate(carrier);
}

void DeviceImpl::OnInterfaceUpdate(const std::string& interface) {
  DLOG(INFO) << path_ << ": OnInterfaceUpdate: interface = " << interface;
  if (interface_ == interface) {
    return;
  }
  if (!interface_.empty()) {
    LOG(WARNING) << path_ << ": OnInterfaceUpdate: interface change from "
        << interface_ << " to " << interface << "!";
  }
  const std::string old_interface = interface_;
  interface_ = interface;
  if (!byte_counter_running_) {
    return;
  }
  // get rid of byte counter for old interface
  StopByteCounter();
  // if we don't have new interface info, we can't make a new byte counter yet
  if (interface_.empty()) {
    return;
  }
  // create byte counter for new interface
  // TODO(vlaviano): creating new counter on interface change may confuse our
  // parent service.
  if (!StartByteCounter()) {
    LOG(ERROR) << path_
        << ": OnInterfaceUpdate: could not create byte counter for "
        << interface_;
    return;
  }
}

void DeviceImpl::OnTypeUpdate(const std::string& type) {
  DLOG(INFO) << path_ << ": OnTypeUpdate: type = " << type;
  type_ = TypeFromString(type);
  if (type_ != kTypeCellular) {
    LOG(WARNING) << path_ << ": OnTypeUpdate: type is not cellular!";
  }
}

// static
gboolean DeviceImpl::StaticGetDevicePropertiesCallback(gpointer data) {
  DeviceImpl *device = reinterpret_cast<DeviceImpl*>(data);
  CHECK_NOTNULL(device);
  DCHECK_NE(device->get_properties_source_id_, 0);
  if (!device->GetDeviceProperties()) {
    // call failed, so try again later
    DLOG(WARNING) << device->GetPath()
        << ": StaticGetDevicePropertiesCallback: "
        << "GetDeviceProperties failed, will retry in "
        << kGetDevicePropertiesIntervalSeconds << " secs";
    // if we've arrived here from our first g_idle_add call, set up a timer
    if (!device->retrying_get_properties_) {
      guint source_id =
          g_timeout_add_seconds(kGetDevicePropertiesIntervalSeconds,
                                StaticGetDevicePropertiesCallback, device);
      device->get_properties_source_id_ = source_id;
      if (source_id == 0) {
          LOG(ERROR) << device->GetPath()
              << ": StaticGetDevicePropertiesCallback: "
              << "g_timeout_add_seconds failed";
        return FALSE;  // get rid of old glib source and give up
      }
      device->retrying_get_properties_ = true;
      // get rid of our old glib source, but our new timer will call us
      return FALSE;
    }
    // otherwise, our timer is already set up
    // return TRUE to indicate that we want to be called again later
    return TRUE;
  }
  device->get_properties_source_id_ = 0;
  device->retrying_get_properties_ = false;
  return FALSE;  // we succeeded, so don't repeat
}

bool DeviceImpl::GetDeviceProperties() {
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
  it = properties.find(kFlimflamDeviceInterfaceProperty);
  if (it != properties.end()) {
    const DBus::Variant& value = static_cast<DBus::Variant>(it->second);
    OnInterfaceUpdate(value.reader().get_string());
  } else {
    DLOG(WARNING) << path_ << ": GetDeviceProperties: no Interface property";
  }
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

bool DeviceImpl::CreateByteCounter() {
  if (byte_counter_ != NULL) {
    DLOG(WARNING) << path_ << ": CreateByteCounter: counter already exists";
    return false;
  }
  if (interface_.empty()) {
    DLOG(WARNING) << path_ << ": CreateByteCounter: interface name unknown";
    return false;
  }
  byte_counter_ = ByteCounter::NewByteCounter(interface_);
  if (byte_counter_ == NULL) {
    LOG(ERROR) << path_
        << ": CreateByteCounter: could not create byte counter for "
        << interface_;
    return false;
  }
  byte_counter_->SetDelegate(this);
  return true;
}

void DeviceImpl::DeleteByteCounter() {
  DLOG(INFO) << path_ << ": DeleteByteCounter";
  if (byte_counter_ != NULL) {
    delete byte_counter_;
    byte_counter_ = NULL;
  }
}

}  // namespace cashew
