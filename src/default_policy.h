// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_DEFAULT_POLICY_H_
#define SRC_DEFAULT_POLICY_H_

#include <string>

#include <base/basictypes.h>  // NOLINT

#include "src/policy.h"

namespace cashew {

// a bare-bones default carrier policy
class DefaultPolicy: public Policy {
  public:

    explicit DefaultPolicy(const std::string& carrier);
    virtual ~DefaultPolicy();

    virtual const std::string& GetCarrier() const;

    virtual guint GetUpdateTimerIdleSecs(const DataPlanList& plans) const;

    virtual bool ShouldEmitDataPlansUpdate(const DataPlanList& old_plans,
        const DataPlanList& new_plans) const;

  private:
    // carrier
    const std::string carrier_;

    DISALLOW_COPY_AND_ASSIGN(DefaultPolicy);
};

}  // namespace cashew

#endif  // SRC_DEFAULT_POLICY_H_
