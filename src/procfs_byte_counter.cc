// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/procfs_byte_counter.h"

#include <stdio.h>

#include <glog/logging.h>

namespace cashew {

static const char *kProcNetDevPath = "/proc/net/dev";
static const int kLineBufferSize = 256;

// /proc/net/dev output format
// must remain consistent with kernel (see net/core/dev.c)
static const char *kProcNetDevExpectedLine1 =
    "Inter-|   Receive                            "
    "                    |  Transmit\n";
static const char *kProcNetDevExpectedLine2 =
    " face |bytes    packets errs drop fifo frame "
    "compressed multicast|bytes    packets errs "
    "drop fifo colls carrier compressed\n";
static const char *kProcNetDevLineFormat =
    "%s %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu\n";
static const int kProcNetDevLineFormatNumElements = 17;

// ReadStats timer interval
// TODO(vlaviano): can reduce this once we implement a smarter ShouldEmitSignal
// policy than 'return true'
static const guint kReadStatsIntervalSeconds = 60;

ProcfsByteCounter::ProcfsByteCounter(const std::string& interface)
    : interface_(interface), proc_net_dev_fp_(NULL), read_stats_source_id_(0),
      counter_rx_bytes_(0), counter_tx_bytes_(0), baseline_stats_(),
      baseline_initialized_(false), delegate_(NULL) {
  DCHECK(!interface_.empty());
  // initialize our counter state
  if (!ReadStats()) {
    LOG(WARNING) << interface_ << ": ctor: ReadStats failed";
    // fall through so that we'll try again later
  }
  // schedule periodic ReadStats() calls to update counter state in the future
  read_stats_source_id_ = g_timeout_add_seconds(kReadStatsIntervalSeconds,
                                                StaticReadStatsCallback, this);
  if (read_stats_source_id_ == 0) {
    LOG(ERROR) << interface_ << ": ctor: g_idle_add failed";
    return;
  }
}

ProcfsByteCounter::~ProcfsByteCounter() {
  DCHECK(proc_net_dev_fp_ == NULL);
  if (read_stats_source_id_ != 0 && !g_source_remove(read_stats_source_id_)) {
    DLOG(WARNING) << interface_ << ": dtor: g_source_remove_failed";
  }
}

const std::string& ProcfsByteCounter::GetInterface() const {
  return interface_;
}

uint64 ProcfsByteCounter::GetRxBytes() const {
  return counter_rx_bytes_;
}

uint64 ProcfsByteCounter::GetTxBytes() const {
  return counter_tx_bytes_;
}

void ProcfsByteCounter::SetDelegate(ByteCounterDelegate *delegate) {
  DLOG(INFO) << interface_ << ": SetDelegate";
  delegate_ = delegate;
}

// private methods

bool ProcfsByteCounter::OpenProcNetDev() {
  DCHECK(proc_net_dev_fp_ == NULL);
  if ((proc_net_dev_fp_ = fopen(kProcNetDevPath, "r")) == NULL) {
    PLOG(ERROR) << interface_ << ": OpenProcNetDev: couldn't open "
        << kProcNetDevPath;
    return false;
  }
  return true;
}

void ProcfsByteCounter::CloseProcNetDev() {
  if (proc_net_dev_fp_ != NULL) {
    if (fclose(proc_net_dev_fp_)) {
      PLOG(WARNING) << interface_ << ": CloseProcNetDev: fclose failed";
    }
    proc_net_dev_fp_ = NULL;
  }
  DCHECK(proc_net_dev_fp_ == NULL);
}

// static
gboolean ProcfsByteCounter::StaticReadStatsCallback(gpointer data) {
  ProcfsByteCounter *counter = reinterpret_cast<ProcfsByteCounter*>(data);
  CHECK_NOTNULL(counter);
  if (!counter->ReadStats()) {
    DLOG(WARNING) << counter->GetInterface() << ": StaticReadStatsCallback: "
        << "ReadStats failed";
  }
  return TRUE;  // we want to be called again later
}

bool ProcfsByteCounter::ReadStats() {
  // we open and close the virtual file each time through, because we get stale
  // data if we keep the file open and rewind/read repeatedly
  if (!OpenProcNetDev()) {
    LOG(ERROR) << interface_ << ": ReadStats: OpenProcDevNet failed";
    return false;
  }
  char line_buffer[kLineBufferSize];
  for (int line_number = 1;  // use 1-based counting to make log msgs sensible
       fgets(line_buffer, sizeof(line_buffer), proc_net_dev_fp_) != NULL;
       ++line_number) {
    // the first two lines contain human-friendly column headers
    // skip them, while ensuring that they are as we expect
    // this provides some confidence that the format hasn't changed on us
    if (line_number == 1 || line_number == 2) {
      if (strncmp(line_buffer, (line_number == 1) ? kProcNetDevExpectedLine1 :
                  kProcNetDevExpectedLine2, sizeof(line_buffer))) {
        LOG(ERROR) << interface_ << ": ReadStats: line " << line_number
            << " is not as we expect";
        CloseProcNetDev();
        return false;
      }
      continue;
    }
    // each subsequent line contains the name and stats for a network interface
    // parse it, reading stats, and see if the name matches |interface_|
    //
    // replace first colon in string (immediately after device name) with a
    // space so that sscanf greedy string parsing doesn't cause the colon and
    // possibly the rx byte count following it to all be consumed as part of the
    // device name.
    char *colon_ptr = strchr(line_buffer, ':');
    if (colon_ptr == NULL) {
      LOG(ERROR) << interface_ << ": ReadStats: line " << line_number
          << ": strchr failed to find colon";
      CloseProcNetDev();
      return false;
    }
    *colon_ptr = ' ';
    char device_name[IFNAMSIZ];
    struct net_device_stats stats;
    memset(&stats, 0, sizeof(stats));
    // cpplint tells us that sscanf, while not prohibited, is sketchy
    // we'll risk it since this data is from the kernel proc fs and we provide
    //   the format string
    //
    // note that some fields of net_device_stats are combined when producing the
    //   contents of /proc/net/dev
    // we assign these combined values to the field named in the column header
    int result = sscanf(line_buffer, kProcNetDevLineFormat,  // NOLINT
                        device_name, &stats.rx_bytes, &stats.rx_packets,
                        &stats.rx_errors, &stats.rx_dropped,
                        &stats.rx_fifo_errors, &stats.rx_frame_errors,
                        &stats.rx_compressed, &stats.multicast, &stats.tx_bytes,
                        &stats.tx_packets, &stats.tx_errors, &stats.tx_dropped,
                        &stats.tx_fifo_errors, &stats.collisions,
                        &stats.tx_carrier_errors, &stats.tx_compressed);
    if (result == EOF && ferror(proc_net_dev_fp_)) {
      PLOG(ERROR) << interface_ << ": ReadStats: line " << line_number
          << ": sscanf failed";
      CloseProcNetDev();
      return false;
    }
    if (result != kProcNetDevLineFormatNumElements) {
      LOG(ERROR) << interface_ << ": ReadStats: line " << line_number
          << ": sscanf matched " << ((result == EOF) ? 0 : result)
          << " elements (" << kProcNetDevLineFormatNumElements << " expected)";
      CloseProcNetDev();
      return false;
    }
    if (device_name != interface_) {
      continue;
    }
    // we found a match
    // update our counter based on new stats
    OnStatsUpdate(stats);
    CloseProcNetDev();
    return true;
  }
  LOG(ERROR) << interface_ << ": ReadStats: no matching device";
  CloseProcNetDev();
  return false;
}

void ProcfsByteCounter::OnStatsUpdate(const struct net_device_stats& stats) {
  DLOG(INFO) << interface_ << ": OnStatsUpdate: stats: rx_bytes = "
      << stats.rx_bytes << ", tx_bytes = " << stats.tx_bytes;
  if (baseline_initialized_) {
    DLOG(INFO) << interface_ << ": OnStatsUpdate: baseline: rx_bytes = "
        << baseline_stats_.rx_bytes << ", tx_bytes = "
        << baseline_stats_.tx_bytes;
    uint64 old_counter_rx_bytes = counter_rx_bytes_;
    uint64 old_counter_tx_bytes = counter_tx_bytes_;
    DLOG(INFO) << interface_ << ": OnStatsUpdate: old counter: rx_bytes = "
        << old_counter_rx_bytes << ", tx_bytes = " << old_counter_tx_bytes;
    // these seemingly naive computations handle wraparound due to the modular
    // nature of unsigned subtraction
    counter_rx_bytes_ += stats.rx_bytes - baseline_stats_.rx_bytes;
    counter_tx_bytes_ += stats.tx_bytes - baseline_stats_.tx_bytes;
    DLOG(INFO) << interface_ << ": OnStatsUpdate: new counter: rx_bytes = "
        << counter_rx_bytes_ << ", tx_bytes = " << counter_tx_bytes_;
    bool counter_changed = counter_rx_bytes_ != old_counter_rx_bytes ||
        counter_tx_bytes_ != old_counter_tx_bytes;
    if (delegate_ != NULL && counter_changed) {
      delegate_->OnByteCounterUpdate(this, counter_rx_bytes_,
                                     counter_tx_bytes_);
    }
  } else {
    baseline_initialized_ = true;
  }
  // We update the baseline on each new sample so that we aren't confused about
  // how many times the device counter has wrapped around since we've started.
  // This works as long as the sample interval is short enough to prevent wrap
  // around between consecutive samples.
  baseline_stats_ = stats;
}

}  // namespace cashew
