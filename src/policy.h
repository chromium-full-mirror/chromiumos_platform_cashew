// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_POLICY_H_
#define SRC_POLICY_H_

#include <glib.h>

#include <string>

#include "src/data_plan.h"

namespace cashew {

// interface for carrier policy
class Policy {
  public:
    virtual ~Policy() {}

    // what carrier is associated with this policy?
    virtual const std::string& GetCarrier() const = 0;

    // how often should a service request updates via the carrier usage API?
    // return value is in seconds
    virtual guint GetUpdateTimerIdleSecs(const DataPlanList& plans) const = 0;

    // should we emit a data plans update for the change from |old_plans|
    // to |new_plans|?
    // NOTE: user might also want to influence this from the Chrome side
    virtual bool ShouldEmitDataPlansUpdate(const DataPlanList& plans) const = 0;

    // should time strings with no timezone suffix be interpreted as being in
    // local time instead of UTC?
    virtual bool ZonelessTimeStringsAreLocal() const = 0;

    // factory
    // return appropriate concrete impl for |carrier|
    // caller owns returned Policy and is responsible for deleting it
    static Policy* GetPolicy(const std::string& carrier);
};

}  // namespace cashew

#endif  // SRC_POLICY_H_
