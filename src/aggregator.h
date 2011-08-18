// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_AGGREGATOR_H_
#define SRC_AGGREGATOR_H_

#include <map>
#include <string>

#include <base/basictypes.h>  // NOLINT

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

    // takes a map of strings to int64s and sets it to the dates for the past
    // year and their corresponding byte count.
    virtual void GetBytesPerDay(
                       std::map<std::string, int64_t>& day_usage)= 0;  //NOLINT

    // factory
    static Aggregator* NewAggregator();

  private:
    DISALLOW_COPY_AND_ASSIGN(Aggregator);
};  // Aggregator

}  // namespace cashew

#endif  // SRC_AGGREGATOR_H_
