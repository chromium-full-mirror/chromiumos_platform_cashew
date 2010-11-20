// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "src/default_policy.h"

namespace cashew {

// test fixture for DefaultPolicy unit tests
class DefaultPolicyTest: public ::testing::Test {
  protected:
    DefaultPolicyTest() : policy_(NULL) {}

    virtual ~DefaultPolicyTest() {
      EXPECT_TRUE(policy_ == NULL);
      EXPECT_TRUE(plans_.empty());
    }

    virtual void SetUp() {
      EXPECT_TRUE(policy_ == NULL);
      policy_ = new(std::nothrow) DefaultPolicy(kTestCarrierName);
      ASSERT_TRUE(policy_ != NULL);
    }

    virtual void TearDown() {
      delete policy_;
      policy_ = NULL;
    }

    static const char *kTestCarrierName;

    DefaultPolicy *policy_;
    DataPlanList plans_;
};

const char *DefaultPolicyTest::kTestCarrierName = "Test Carrier";

TEST_F(DefaultPolicyTest, GetUpdateTimerIdleSecs) {
  // we expect the default timer value to be 1 min = 60 secs
  // unconditionally (i.e., independent of value of |plans_|) for now
  EXPECT_EQ(60, policy_->GetUpdateTimerIdleSecs(plans_));
}

TEST_F(DefaultPolicyTest, ShouldEmitDataPlansUpdate) {
  // we expect the result to be true unconditionally (i.e., independent of the
  // value of |plans_|) for now
  EXPECT_TRUE(policy_->ShouldEmitDataPlansUpdate(plans_));
}

TEST_F(DefaultPolicyTest, ZonelessTimeStringsAreLocal) {
  EXPECT_TRUE(policy_->ZonelessTimeStringsAreLocal());
}

}  // namespace cashew
