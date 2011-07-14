// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_USER_AGGREGATOR_H_
#define SRC_USER_AGGREGATOR_H_

#include <string>
#include <map>

#include "src/data_plan.h"

namespace cashew {

typedef std::string Username;
typedef std::map<Username, ByteCount> BytesPerUser;

// UserAggregator interface
class UserAggregator {
  public:
    UserAggregator() {}
    virtual ~UserAggregator() {}

    // called when a new DataPlan object is created
    virtual void OnNewDataPlan(DataPlan* data_plan) = 0;

    // factory
    static UserAggregator* NewUserAggregator();

  private:
    DISALLOW_COPY_AND_ASSIGN(UserAggregator);
};  // UserAggregator

}  // namespace cashew

#endif  // SRC_USER_AGGREGATOR_H_
