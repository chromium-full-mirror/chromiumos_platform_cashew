// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_IPCONFIG_H_
#define SRC_IPCONFIG_H_

#include <string>
#include <vector>

#include <base/basictypes.h>  // NOLINT
#include <dbus-c++/dbus.h>  // NOLINT

namespace cashew {

class Device;

// Ipconfig interface
// represents a Flimflam ipconfig
class Ipconfig {
  public:
    Ipconfig() {}
    virtual ~Ipconfig() {}

    // get D-Bus path for this ipconfig
    virtual const DBus::Path& GetPath() const = 0;

    // get the list of nameservers for ths ipconfig
    // returns empty list if we don't know
    virtual const std::vector<std::string>& GetNameServers() = 0;

    // factory
    static Ipconfig* NewIpconfig(DBus::Connection& connection,  // NOLINT
                                 const DBus::Path& path);

  private:
    DISALLOW_COPY_AND_ASSIGN(Ipconfig);
};

}  // namespace cashew

#endif  // SRC_IPCONFIG_H_
