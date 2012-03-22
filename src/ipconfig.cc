// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/ipconfig.h"

#include <glog/logging.h>

#include "src/ipconfig_impl.h"

namespace cashew {

// static
Ipconfig* Ipconfig::NewIpconfig(DBus::Connection& connection,  // NOLINT
                                const DBus::Path& path) {
  return new(std::nothrow) IpconfigImpl(connection, path);
}

}  // namespace cashew
