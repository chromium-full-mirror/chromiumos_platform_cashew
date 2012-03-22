// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_IPCONFIG_IMPL_H_
#define SRC_IPCONFIG_IMPL_H_

#include <glib.h>

#include <string>
#include <vector>

#include <base/basictypes.h>  // NOLINT
#include <dbus-c++/dbus.h>  // NOLINT

#include "src/ipconfig.h"
#include "src/flimflam_ipconfig_client_glue.h"
#include "src/property_changed_handler.h"

namespace cashew {

// represents an ipconfig and provides access to ipconfig properties
// flimflam does not send change signals for ipconfig objects so
// nothing is cached
class IpconfigImpl : public Ipconfig,
                     public org::chromium::flimflam::IPConfig_proxy,
                     public DBus::IntrospectableProxy,
                     public DBus::ObjectProxy {
  public:
    IpconfigImpl(DBus::Connection& connection,  // NOLINT
                 const DBus::Path& path);
    virtual ~IpconfigImpl();

    // Ipconfig methods
    virtual const DBus::Path& GetPath() const;
    virtual const std::vector<std::string>& GetNameServers();

  private:
    // D-Bus path for Flimflam ipconfig that we represent
    // this is our unique identifier
    const DBus::Path path_;

    // name servers
    std::vector<std::string> nameservers_;

    // get ipconfig properties from Flimflam
    // returns true on success and false on failure
    bool GetIpconfigProperties();

    DISALLOW_COPY_AND_ASSIGN(IpconfigImpl);
};

}  // namespace cashew

#endif  // SRC_IPCONFIG_IMPL_H_
