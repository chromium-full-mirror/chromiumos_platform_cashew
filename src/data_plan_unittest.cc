// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "src/data_plan.h"

namespace cashew {

// test fixture for DataPlan unit tests
class DataPlanTest: public ::testing::Test {
  protected:
    DataPlanTest() {}
    virtual ~DataPlanTest() {}
    virtual void SetUp() {}
    virtual void TearDown() {}
};

typedef DataPlanTest DataPlanDeathTest;

TEST_F(DataPlanTest, UtcTimeString) {
  static const char *kTimeStringToTest = "2010-09-20T23:59:59Z";
  base::Time time;
  bool result = DataPlan::TimeFromIso8601(kTimeStringToTest, &time, false);
  EXPECT_TRUE(result);
  struct base::Time::Exploded exploded;
  time.UTCExplode(&exploded);
  EXPECT_EQ(2010, exploded.year);
  EXPECT_EQ(9, exploded.month);
  EXPECT_EQ(20, exploded.day_of_month);
  EXPECT_EQ(23, exploded.hour);
  EXPECT_EQ(59, exploded.minute);
  EXPECT_EQ(59, exploded.second);
  EXPECT_EQ(0, exploded.millisecond);
}

TEST_F(DataPlanTest, ZonelessTimeStringWithUtcPolicy) {
  static const char *kTimeStringToTest = "2010-09-20T23:59:59";
  base::Time time;
  bool result = DataPlan::TimeFromIso8601(kTimeStringToTest, &time, false);
  EXPECT_TRUE(result);
  struct base::Time::Exploded exploded;
  time.UTCExplode(&exploded);
  EXPECT_EQ(2010, exploded.year);
  EXPECT_EQ(9, exploded.month);
  EXPECT_EQ(20, exploded.day_of_month);
  EXPECT_EQ(23, exploded.hour);
  EXPECT_EQ(59, exploded.minute);
  EXPECT_EQ(59, exploded.second);
  EXPECT_EQ(0, exploded.millisecond);
}

TEST_F(DataPlanTest, ZonelessTimeStringWithLocalPolicy) {
  static const char *kTimeStringToTest = "2010-09-20T23:59:59";
  base::Time time;
  bool result = DataPlan::TimeFromIso8601(kTimeStringToTest, &time, true);
  EXPECT_TRUE(result);
  struct base::Time::Exploded exploded;
  time.LocalExplode(&exploded);
  EXPECT_EQ(2010, exploded.year);
  EXPECT_EQ(9, exploded.month);
  EXPECT_EQ(20, exploded.day_of_month);
  EXPECT_EQ(23, exploded.hour);
  EXPECT_EQ(59, exploded.minute);
  EXPECT_EQ(59, exploded.second);
  EXPECT_EQ(0, exploded.millisecond);
}

TEST_F(DataPlanTest, EmptyTimeString) {
  base::Time time;
  static const char *kTimeStringToTest  = "";
  bool result = DataPlan::TimeFromIso8601(kTimeStringToTest, &time, false);
  EXPECT_FALSE(result);
}

TEST_F(DataPlanDeathTest, NullTimeOutPtr) {
  static const char *kTimeStringToTest = "2010-09-20T23:59:59Z";
  EXPECT_EXIT(DataPlan::TimeFromIso8601(kTimeStringToTest, NULL, false),
              testing::KilledBySignal(SIGABRT),
              "Check failed: 'time_out' Must be non NULL");
}

}  // namespace cashew
