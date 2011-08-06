// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/service_manager.h"

#include <glog/logging.h>

#include "src/service_manager_impl.h"

namespace cashew {

// static
ServiceManager* ServiceManager::NewServiceManager(
                                       DBus::Connection& connection,  // NOLINT
                                       GMainLoop * const main_loop,
                                       MetricsManager * const metrics_manager,
                                       Aggregator * const aggregator) {
  return new(std::nothrow) ServiceManagerImpl(connection, main_loop,
                                              metrics_manager, aggregator);
}

}  // namespace cashew
