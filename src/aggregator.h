// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_AGGREGATOR_H_
#define SRC_AGGREGATOR_H_

#include <base/basictypes.h>

namespace cashew {

class Service;

// Aggregator interface
class Aggregator {
  public:
    Aggregator() {}
    virtual ~Aggregator() {}

    // called when |service| is updated with byte counter info
    // we are given the bytes used since the last byte counter update
    virtual void OnByteCounterUpdate(const Service *service,
        uint64 delta_rx_bytes, uint64 delta_tx_bytes) = 0;

    // factory
    static Aggregator* NewAggregator();

  private:
    DISALLOW_COPY_AND_ASSIGN(Aggregator);
};  // Aggregator

}  // namespace cashew

#endif  // SRC_AGGREGATOR_H_
