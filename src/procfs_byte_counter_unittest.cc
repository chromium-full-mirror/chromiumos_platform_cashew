// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "src/procfs_byte_counter.h"

namespace cashew {

// test fixture for ProcfsByteCounter unit tests
class ProcfsByteCounterTest: public ::testing::Test {
  protected:
    ProcfsByteCounterTest() {}
    virtual ~ProcfsByteCounterTest() {}
    virtual void SetUp() {}
    virtual void TearDown() {}
};

TEST_F(ProcfsByteCounterTest, AlwaysPasses) {
  EXPECT_TRUE(true);
}

// TODO(vlaviano): test ProcfsByteCounter:OnStatsUpdate
//   normal scenarios
//   wraparound scenarios

}  // namespace cashew
