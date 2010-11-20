// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/byte_counter.h"

#include <glog/logging.h>

#include "src/procfs_byte_counter.h"

namespace cashew {

// static
ByteCounter* ByteCounter::NewByteCounter(const std::string& interface) {
  DCHECK(!interface.empty());
  return new(std::nothrow) ProcfsByteCounter(interface);
}

}  // namespace cashew
