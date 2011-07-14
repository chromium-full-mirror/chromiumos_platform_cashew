// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/user_aggregator.h"

#include <glog/logging.h>

#include "src/user_aggregator_impl.h"

namespace cashew {

// static
UserAggregator* UserAggregator::NewUserAggregator() {
  UserAggregatorImpl *aggregator = new(std::nothrow) UserAggregatorImpl();
  if (aggregator == NULL) {
    LOG(ERROR) << "NewUserAggregator: could not create user aggregator";
    return NULL;
  }
  // initialize aggregator
  if (!aggregator->Init()) {
    LOG(ERROR) << "NewUserAggregator: could not initialize user aggregator";
    delete aggregator;
    return NULL;
  }
  return aggregator;
}

}  // namespace cashew
