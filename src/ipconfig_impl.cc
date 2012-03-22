// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/ipconfig_impl.h"

#include <glog/logging.h>

#include "src/device.h"
#include "src/service.h"

using std::vector;

namespace cashew {

// Flimflam Ipconfig D-Bus identifiers
static const char *kFlimflamIpconfigName = "org.chromium.flimflam";

// Flimflam Ipconfig property names
static const char *kFlimflamIpconfigNameServersProperty = "NameServers";

IpconfigImpl::IpconfigImpl(DBus::Connection& connection,  // NOLINT
                           const DBus::Path& path)
    : DBus::ObjectProxy(connection, path, kFlimflamIpconfigName), path_(path) {
}

IpconfigImpl::~IpconfigImpl() {
}

const DBus::Path& IpconfigImpl::GetPath() const {
  return path_;
}

const vector<std::string>& IpconfigImpl::GetNameServers() {
  GetIpconfigProperties();
  return nameservers_;
}

// Private methods

bool IpconfigImpl::GetIpconfigProperties() {
  LOG(INFO) << path_ << ": GetIpconfigProperties";
  PropertyMap properties;
  // dbus-c++ throws exceptions
  // invoke the "Existing Non-conformant Code" clause of the style guide and
  // isolate the rest of the system from this
  try {
    // TODO(jglasgow): make this call asynchronous
    properties = GetProperties();
  } catch (DBus::Error& error) {  // NOLINT
    LOG(WARNING) << path_
        << ": GetIpconfigProperties: GetProperties() -> Exception: "
        << error.name() << ": " << error.message();
    return false;
  } catch (...) {  // NOLINT
    LOG(WARNING) << path_
        << ": GetIpconfigProperties: GetProperties() -> Exception";
    return false;
  }
  LOG(INFO) << "Received " << properties.size() << " properties";

  // grab the properties in which we're interested
  PropertyMap::const_iterator it(
      properties.find(kFlimflamIpconfigNameServersProperty));
  if (it != properties.end()) {
    const DBus::Variant& value = static_cast<DBus::Variant>(it->second);
    nameservers_ = value.operator vector<std::string>();
  } else {
    LOG(WARNING) << path_ << ": GetIpconfigProperties: no NameServers property";
  }
  return true;
}

}  // namespace cashew
