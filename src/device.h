// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_DEVICE_H_
#define SRC_DEVICE_H_

#include <string>
#include <vector>

#include <base/basictypes.h>  // NOLINT
#include <dbus-c++/dbus.h>  // NOLINT

namespace cashew {

class ByteCounter;
class Service;

// Device interface
// represents a cellular device and monitors Flimflam state for that device
class Device {
  public:
    Device() {}
    virtual ~Device() {}

    // get D-Bus path for this device
    virtual const DBus::Path& GetPath() const = 0;

    // valid type values for this device
    typedef enum {
      kTypeUnknown = 0,
      kTypeEthernet,
      kTypeWifi,
      kTypeWimax,
      kTypeBluetooth,
      kTypeGPS,
      kTypeCellular,
      kTypeVendor,
    } Type;

    // get type for this device
    // NOTE: for now, should always be kTypeCellular
    virtual Type GetType() const = 0;

    // get cellular carrier for this device
    // returns empty string if we don't know
    virtual const std::string& GetCarrier() const = 0;

    // get interface name for this device (e.g., "usb0")
    // returns empty string if we don't know
    virtual const std::string& GetInterface() const = 0;

    // get nameservers used by this device
    // returns a comma separated list of nameservers
    // empty string if we don't know
    virtual void GetNameServers(
        std::vector<std::string>* nameservers) const = 0;

    // Service methods

    // start a new byte counter from 0
    // there must not be an existing byte counter
    // parent service will receive periodic updates via OnByteCounterUpdate
    // returns true on success and false on failure
    virtual bool StartByteCounter() = 0;

    // destroy any existing byte counter
    // parent service will no longer receive updates after this call
    virtual void StopByteCounter() = 0;

    // calls ReadStats on byte counter
    virtual bool ReadStats() = 0;

    // get the current byte counter stats
    // byte counter should be running
    virtual bool GetByteCounterStats(uint64 *rx_bytes,
                                     uint64 *tx_bytes) const = 0;

    // is the byte counter running?
    virtual bool ByteCounterRunning() const = 0;

    // factory
    static Device* NewDevice(Service * const parent,
                             DBus::Connection& connection,  // NOLINT
                             const DBus::Path& path);

  private:
    DISALLOW_COPY_AND_ASSIGN(Device);
};

}  // namespace cashew

#endif  // SRC_DEVICE_H_
