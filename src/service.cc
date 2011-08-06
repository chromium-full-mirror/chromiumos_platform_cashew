// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/service.h"

#include <glog/logging.h>

#include "src/service_impl.h"

namespace cashew {

// static
Service::Type Service::TypeFromString(const std::string& type) {
  return ServiceImpl::TypeFromString(type);
}

// static
Service* Service::NewService(ServiceManager * const parent,
                             DBus::Connection& connection,  // NOLINT
                             MetricsManager * const metrics_manager,
                             Aggregator * const aggregator,
                             const DBus::Path& path) {
  DCHECK(parent != NULL);
  return new(std::nothrow) ServiceImpl(parent, connection, metrics_manager,
                                       aggregator, path);
}

}  // namespace cashew
