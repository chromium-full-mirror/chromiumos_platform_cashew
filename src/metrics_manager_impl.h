// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_METRICS_MANAGER_IMPL_H_
#define SRC_METRICS_MANAGER_IMPL_H_

#include <base/basictypes.h>
#include <metrics/metrics_library.h>

#include "src/metrics_manager.h"

namespace cashew {

class MetricsManagerImpl : public MetricsManager {
  public:
    MetricsManagerImpl();
    virtual ~MetricsManagerImpl();

    // Network.Cellular.UsageRequestStatus
    virtual void OnUsageRequestStatusSample(UsageRequestStatus status);

  private:
    // ChromeOS metrics library interface
    MetricsLibrary metrics_library_;

    DISALLOW_COPY_AND_ASSIGN(MetricsManagerImpl);
};

}  // namespace cashew

#endif  // SRC_METRICS_MANAGER_IMPL_H_
