// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/aggregator.h"

#include <glog/logging.h>

#include "src/aggregator_impl.h"

namespace cashew {

// static
Aggregator* Aggregator::NewAggregator() {
  // create aggregator
  AggregatorImpl *aggregator = new(std::nothrow) AggregatorImpl();
  if (aggregator == NULL) {
    LOG(ERROR) << "NewAggregator: failed to create aggregator";
    return NULL;
  }

  // initialize aggregator
  if (!aggregator->Init()) {
    LOG(ERROR) << "NewAggregator: failed to initialize aggregator";
    delete aggregator;
    return NULL;
  }

  return aggregator;
}

}  // namespace cashew
