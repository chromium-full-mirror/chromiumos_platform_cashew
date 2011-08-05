// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/metrics_manager_impl.h"

#include <glog/logging.h>

namespace cashew {

// UMA metric names
static const char *kUsageRequestStatusMetric =
    "Network.Cellular.UsageRequestStatus";

MetricsManagerImpl::MetricsManagerImpl() {
  metrics_library_.Init();
}

MetricsManagerImpl::~MetricsManagerImpl() {
}

void MetricsManagerImpl::OnUsageRequestStatusSample(UsageRequestStatus status) {
  DCHECK_LE(0, status);
  DCHECK_LT(status, kUsageRequestStatusMax);
  LOG(INFO) << "OnUsageRequestStatusSample: status = " << status;
  if (metrics_library_.AreMetricsEnabled() &&
      !metrics_library_.SendEnumToUMA(kUsageRequestStatusMetric, status,
                                      kUsageRequestStatusMax)) {
    LOG(WARNING) << "OnUsageRequestStatusSample: couldn't send sample to UMA";
  }
}

}  // namespace cashew
