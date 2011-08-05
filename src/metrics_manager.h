// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_METRICS_MANAGER_H_
#define SRC_METRICS_MANAGER_H_

#include <base/basictypes.h>

namespace cashew {

// MetricsManager interface
// Accepts metrics from cashew components for recording/analysis
class MetricsManager {
  public:
    MetricsManager() {}
    virtual ~MetricsManager() {}

    // Network.Cellular.UsageRequestStatus UMA enum
    typedef enum {
      kUsageRequestStatusFailed = 0,
      kUsageRequestStatusOk,
      kUsageRequestStatusError,
      kUsageRequestStatusMalformedRequest,
      kUsageRequestStatusInternalError,
      kUsageRequestStatusServiceUnavailable,
      kUsageRequestStatusRequestRefused,
      kUsageRequestStatusUnknownDevice,
      kUsageRequestStatusMax
    } UsageRequestStatus;

    // A usage API request completed with status |status|
    virtual void OnUsageRequestStatusSample(UsageRequestStatus status) = 0;

    // factory
    static MetricsManager* NewMetricsManager();

  private:
    DISALLOW_COPY_AND_ASSIGN(MetricsManager);
};

}  // namespace cashew

#endif  // SRC_METRICS_MANAGER_H_
