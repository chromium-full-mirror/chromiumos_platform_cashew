// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/default_policy.h"

#include <glog/logging.h>

namespace cashew {

// default update timer interval in seconds
static const guint kSecondsPerMinute = 60;
static const guint kDefaultPolicyUpdateTimerIdleSecs = 10 * kSecondsPerMinute;

DefaultPolicy::DefaultPolicy(const std::string& carrier) : carrier_(carrier) {
}

DefaultPolicy::~DefaultPolicy() {
}

const std::string& DefaultPolicy::GetCarrier() const {
  return carrier_;
}

guint DefaultPolicy::GetUpdateTimerIdleSecs(const DataPlanList& plans) const {
  return kDefaultPolicyUpdateTimerIdleSecs;
}

bool DefaultPolicy::ShouldEmitDataPlansUpdate(const DataPlanList& old_plans,
    const DataPlanList& new_plans) const {
  // unconditionally send an update
  return true;
}

bool DefaultPolicy::UsageRequestsMustBeSentOTA() const {
  return true;
}

bool DefaultPolicy::ZonelessTimeStringsAreLocal() const {
  return true;
}

// private methods

}  // namespace cashew
