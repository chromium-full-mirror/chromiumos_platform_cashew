// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/metrics_manager.h"

#include "src/metrics_manager_impl.h"

namespace cashew {

// static
MetricsManager* MetricsManager::NewMetricsManager() {
  return new(std::nothrow) MetricsManagerImpl();
}

}  // namespace cashew
