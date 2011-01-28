// Copyright (c) 2010, 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/device.h"

#include <glog/logging.h>

#include "src/device_impl.h"

namespace cashew {

// static
Device* Device::NewDevice(Service * const parent,
                          DBus::Connection& connection,  // NOLINT
                          const DBus::Path& path) {
  DCHECK(parent != NULL);
  return new(std::nothrow) DeviceImpl(parent, connection, path);
}

}  // namespace cashew
