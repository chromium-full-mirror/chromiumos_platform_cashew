// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_BYTE_COUNTER_H_
#define SRC_BYTE_COUNTER_H_

#include <string>

#include <base/basictypes.h>  // NOLINT

namespace cashew {

class ByteCounterDelegate;

// interface for byte counter
class ByteCounter {
  public:
    virtual ~ByteCounter() {}

    // to what interface is this counter attached?
    virtual const std::string& GetInterface() const = 0;

    // received bytes
    virtual uint64 GetRxBytes() const = 0;
    virtual void SetRxBytes(uint64 rx_bytes) = 0;

    // transmitted bytes
    virtual uint64 GetTxBytes() const = 0;
    virtual void SetTxBytes(uint64 tx_bytes) = 0;

    // set delegate interface that will receive counter updates
    // it's ok to clear this by setting it to NULL
    virtual void SetDelegate(ByteCounterDelegate *delegate) = 0;

    // factory
    // return appropriate concrete impl
    // caller owns returned ByteCounter and is responsible for deleting it
    static ByteCounter* NewByteCounter(const std::string& interface);
};

// interface to be implemented by delegates
class ByteCounterDelegate {
  public:
    virtual ~ByteCounterDelegate() {}

    // called when byte counter is updated
    virtual void OnByteCounterUpdate(const ByteCounter *counter,
                                     uint64 rx_bytes, uint64 tx_bytes) = 0;
};

}  // namespace cashew

#endif  // SRC_BYTE_COUNTER_H_
