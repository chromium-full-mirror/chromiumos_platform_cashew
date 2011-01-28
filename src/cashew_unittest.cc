// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <glog/logging.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

int main(int argc, char *argv[]) {
  ::google::InitGoogleLogging(argv[0]);
  ::testing::InitGoogleTest(&argc, argv);
  ::testing::FLAGS_gtest_death_test_style = "fast";
  ::testing::InitGoogleMock(&argc, argv);
  return RUN_ALL_TESTS();
}
