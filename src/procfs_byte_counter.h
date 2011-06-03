// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_PROCFS_BYTE_COUNTER_H_
#define SRC_PROCFS_BYTE_COUNTER_H_

#include <stdio.h>

#include <glib.h>
#include <sys/socket.h>
#include <linux/netdevice.h>

#include <string>

#include <base/basictypes.h>  // NOLINT

#include "src/byte_counter.h"

namespace cashew {

// a procfs-based implementation of the ByteCounter interface
class ProcfsByteCounter: public ByteCounter {
  public:
    explicit ProcfsByteCounter(const std::string& interface);
    virtual ~ProcfsByteCounter();
    virtual const std::string& GetInterface() const;
    virtual uint64 GetRxBytes() const;
    virtual void SetRxBytes(uint64 rx_bytes);
    virtual uint64 GetTxBytes() const;
    virtual void SetTxBytes(uint64 tx_bytes);
    virtual void SetDelegate(ByteCounterDelegate *delegate);

  private:
    // interface name
    const std::string interface_;

    // file pointer for open /proc/net/dev virtual file
    // can be NULL
    FILE *proc_net_dev_fp_;

    // ReadStats timer glib source id
    guint read_stats_source_id_;

    // counter: received bytes
    uint64 counter_rx_bytes_;

    // counter: transmitted byes
    uint64 counter_tx_bytes_;

    // This used to be exported by the kernel, but isn't any more, so we make
    // our own definition here.
    struct netdev_stats {
        unsigned long   rx_packets;
        unsigned long   tx_packets;
        unsigned long   rx_bytes;
        unsigned long   tx_bytes;
        unsigned long   rx_errors;
        unsigned long   tx_errors;
        unsigned long   rx_dropped;
        unsigned long   tx_dropped;
        unsigned long   multicast;
        unsigned long   collisions;
        unsigned long   rx_length_errors;
        unsigned long   rx_over_errors;
        unsigned long   rx_crc_errors;
        unsigned long   rx_frame_errors;
        unsigned long   rx_fifo_errors;
        unsigned long   rx_missed_errors;
        unsigned long   tx_aborted_errors;
        unsigned long   tx_carrier_errors;
        unsigned long   tx_fifo_errors;
        unsigned long   tx_heartbeat_errors;
        unsigned long   tx_window_errors;
        unsigned long   rx_compressed;
        unsigned long   tx_compressed;
    };

    // baseline stats against which we'll compare our next update
    // updated with each sample so as to better handle wraparound
    //
    // these stats are maintained by the device driver starting from kernel boot
    // or module load time and have aribitrary values relative to when our
    // logical |counter_rx_bytes_| and |counter_tx_bytes_| counters were started
    struct netdev_stats baseline_stats_;

    // have our baseline device stats been initialized?
    // this becomes true after the first call to OnStatsUpdate
    bool baseline_initialized_;

    // delegate
    ByteCounterDelegate *delegate_;

    // open /proc/net/dev
    // stores resulting fp in |proc_net_dev_fp_|
    // returns true on success and false on failure
    bool OpenProcNetDev();

    // close /proc/net/dev
    // clears |proc_net_dev_fp_|
    // ok to call even if file isn't open
    void CloseProcNetDev();

    // glib integration: static wrapper for ReadStats
    // takes object ptr as data and invokes object->ReadStats
    static gboolean StaticReadStatsCallback(gpointer data);

    // read stats for |interface_| from /proc/net/dev
    // returns true on success and false on failure
    bool ReadStats();

    // new stats are available, update counter
    void OnStatsUpdate(const struct netdev_stats& stats);

    DISALLOW_COPY_AND_ASSIGN(ProcfsByteCounter);
};

}  // namespace cashew

#endif  // SRC_PROCFS_BYTE_COUNTER_H_
