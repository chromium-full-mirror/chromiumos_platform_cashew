// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/scoped_ptr.h>
#include <gtest/gtest.h>

#include "src/data_plan.h"
#include "src/json_reader.h"
#include "src/policy.h"

namespace cashew {

// test fixture for DataPlan unit tests
class DataPlanTest: public ::testing::Test {
  protected:
    DataPlanTest() : policy_(NULL), current_unlimited_plan_(NULL),
        current_available_metered_plan_(NULL) {}

    virtual ~DataPlanTest() {
      EXPECT_TRUE(policy_ == NULL);
      EXPECT_TRUE(current_unlimited_plan_ == NULL);
      EXPECT_TRUE(current_available_metered_plan_ == NULL);
    }

    virtual void SetUp() {
      EXPECT_TRUE(policy_ == NULL);
      policy_ = Policy::GetPolicy(kTestCarrierName);
      ASSERT_TRUE(policy_ != NULL);

      EXPECT_TRUE(current_unlimited_plan_ == NULL);
      DataPlan::Type type = DataPlan::kTypeUnlimited;
      base::TimeDelta twelve_hours = base::TimeDelta::FromHours(12);
      base::Time now = base::Time::Now();
      base::Time start_time = now - twelve_hours;
      base::Time end_time = now + twelve_hours;
      Bytes max_bytes = 0;
      Bytes used_bytes = 5 * 1024 * 1024;
      current_unlimited_plan_ = new(std::nothrow) DataPlan(
          "Current Unlimited Plan", type, now, start_time, end_time, max_bytes,
          used_bytes);
      ASSERT_TRUE(current_unlimited_plan_ != NULL);

      EXPECT_TRUE(current_available_metered_plan_ == NULL);
      type = DataPlan::kTypeMeteredFree;
      max_bytes = 100 * 1024 * 1024;
      current_available_metered_plan_ = new(std::nothrow) DataPlan(
          "Current Available Metered Plan", type, now, start_time, end_time,
          max_bytes, used_bytes);
      ASSERT_TRUE(current_available_metered_plan_ != NULL);
    }

    virtual void TearDown() {
      delete current_available_metered_plan_;
      current_available_metered_plan_ = NULL;
      delete current_unlimited_plan_;
      current_unlimited_plan_ = NULL;
      delete policy_;
      policy_ = NULL;
    }

    // hardcoded JSON data plan representations
    static const char *kMeteredWithUsedBytesJSON;
    static const char *kMeteredWithNoUsedBytesJSON;
    static const char *kUnlimitedWithUsedBytesJSON;
    static const char *kUnlimitedWithNoUsedBytesJSON;
    static const char *k32BitSignedOverflowingMaxBytesJSON;
    static const char *k32BitUnsignedOverflowingMaxBytesJSON;

    // test carrier for constructing policy
    static const char *kTestCarrierName;

    // test policy used by FromDictionaryValue tests
    Policy *policy_;

    // current unlimited plan shared by multiple tests
    DataPlan *current_unlimited_plan_;

    // current available metered plan shared by multiple tests
    DataPlan *current_available_metered_plan_;
};

typedef DataPlanTest DataPlanDeathTest;

const char *DataPlanTest::kMeteredWithUsedBytesJSON =
    "{\"lastUpdate\" : \"2010-09-20T12:01:00Z\","
     "\"planName\" : \"Super-Awesome Chromium OS Plan\","
     "\"planType\" : \"METERED_PAID\","
     "\"maxBytes\" : 104857600,"
     "\"usedBytes\" : 52428800,"
     "\"expirationTime\" : \"2010-09-30T23:59:59\","
     "\"startTime\" : \"2010-09-01T00:00:00\""
    "}";

const char *DataPlanTest::kMeteredWithNoUsedBytesJSON =
    "{\"lastUpdate\" : \"2010-09-20T12:01:00Z\","
     "\"planName\" : \"Super-Awesome Chromium OS Plan\","
     "\"planType\" : \"METERED_PAID\","
     "\"maxBytes\" : 104857600,"
     "\"expirationTime\" : \"2010-09-30T23:59:59\","
     "\"startTime\" : \"2010-09-01T00:00:00\""
    "}";

const char *DataPlanTest::kUnlimitedWithUsedBytesJSON =
    "{\"lastUpdate\" : \"2010-09-20T12:01:00Z\","
     "\"planName\" : \"All-You-Can-Eat Day Pass\","
     "\"planType\" : \"UNLIMITED\","
     "\"usedBytes\" : 52428800,"
     "\"expirationTime\" : \"2010-09-20T23:59:59\","
     "\"startTime\" : \"2010-09-20T00:00:00\""
    "}";

const char *DataPlanTest::kUnlimitedWithNoUsedBytesJSON =
    "{\"lastUpdate\" : \"2010-09-20T12:01:00Z\","
     "\"planName\" : \"All-You-Can-Eat Day Pass\","
     "\"planType\" : \"UNLIMITED\","
     "\"expirationTime\" : \"2010-09-20T23:59:59\","
     "\"startTime\" : \"2010-09-20T00:00:00\""
    "}";

const char *DataPlanTest::k32BitSignedOverflowingMaxBytesJSON =
    "{\"lastUpdate\" : \"2010-09-20T12:01:00Z\","
     "\"planName\" : \"Super-Awesome Chromium OS Plan\","
     "\"planType\" : \"METERED_PAID\","
     "\"maxBytes\" : 2500000000,"
     "\"usedBytes\" : 0,"
     "\"expirationTime\" : \"2010-09-30T23:59:59\","
     "\"startTime\" : \"2010-09-01T00:00:00\""
    "}";

const char *DataPlanTest::k32BitUnsignedOverflowingMaxBytesJSON =
    "{\"lastUpdate\" : \"2010-09-20T12:01:00Z\","
     "\"planName\" : \"Super-Awesome Chromium OS Plan\","
     "\"planType\" : \"METERED_PAID\","
     "\"maxBytes\" : 5000000000,"
     "\"usedBytes\" : 0,"
     "\"expirationTime\" : \"2010-09-30T23:59:59\","
     "\"startTime\" : \"2010-09-01T00:00:00\""
    "}";

const char *DataPlanTest::kTestCarrierName = "Test Carrier";

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

TEST_F(DataPlanTest, FromDictionaryValueMeteredWithUsedBytes) {
  scoped_ptr<Value> value(JSONReader::Read(kMeteredWithUsedBytesJSON, true));
  ASSERT_TRUE(value != NULL);
  ASSERT_TRUE(value->IsType(Value::TYPE_DICTIONARY));
  const DictionaryValue *dict_value =
      static_cast<const DictionaryValue*>(value.get());
  DataPlan *plan = DataPlan::FromDictionaryValue(dict_value, policy_);
  // we expect the call to have succeeeded, with the new data plan's used bytes
  // property set to the value that was present in the JSON representation
  ASSERT_TRUE(plan != NULL);
  EXPECT_EQ(52428800, plan->GetDataBytesUsed());
}

TEST_F(DataPlanTest, FromDictonaryValueMeteredWithNoUsedBytes) {
  scoped_ptr<Value> value(JSONReader::Read(kMeteredWithNoUsedBytesJSON, true));
  ASSERT_TRUE(value != NULL);
  ASSERT_TRUE(value->IsType(Value::TYPE_DICTIONARY));
  const DictionaryValue *dict_value =
      static_cast<const DictionaryValue*>(value.get());
  DataPlan *plan = DataPlan::FromDictionaryValue(dict_value, policy_);
  // the JSON representation for metered plans must include used bytes, so we
  // expect the call to have failed
  EXPECT_TRUE(plan == NULL);
}

TEST_F(DataPlanTest, FromDictionaryValueUnlimitedWithUsedBytes) {
  scoped_ptr<Value> value(JSONReader::Read(kUnlimitedWithUsedBytesJSON, true));
  ASSERT_TRUE(value != NULL);
  ASSERT_TRUE(value->IsType(Value::TYPE_DICTIONARY));
  const DictionaryValue *dict_value =
      static_cast<const DictionaryValue*>(value.get());
  DataPlan *plan = DataPlan::FromDictionaryValue(dict_value, policy_);
  // we expect the call to have succeeeded, with the new data plan's used bytes
  // property set to the value that was present in the JSON representation
  ASSERT_TRUE(plan != NULL);
  EXPECT_EQ(52428800, plan->GetDataBytesUsed());
}

TEST_F(DataPlanTest, FromDictionaryValueUnlimitedWithNoUsedBytes) {
  scoped_ptr<Value> value(JSONReader::Read(kUnlimitedWithNoUsedBytesJSON,
                                           true));
  ASSERT_TRUE(value != NULL);
  ASSERT_TRUE(value->IsType(Value::TYPE_DICTIONARY));
  const DictionaryValue *dict_value =
      static_cast<const DictionaryValue*>(value.get());
  DataPlan *plan = DataPlan::FromDictionaryValue(dict_value, policy_);
  // it's optional for the JSON representation for unlimited plans to include
  // used bytes, so we expect the call to have succeeded. The new data plan's
  // used bytes property should have been set to the default of 0.
  ASSERT_TRUE(plan != NULL);
  EXPECT_EQ(0, plan->GetDataBytesUsed());
}

TEST_F(DataPlanTest, FromDictionaryValue32BitSignedOverflowingMaxBytes) {
  scoped_ptr<Value> value(JSONReader::Read(k32BitSignedOverflowingMaxBytesJSON,
                                           true));
  ASSERT_TRUE(value != NULL);
  ASSERT_TRUE(value->IsType(Value::TYPE_DICTIONARY));
  const DictionaryValue *dict_value =
      static_cast<const DictionaryValue*>(value.get());
  DataPlan *plan = DataPlan::FromDictionaryValue(dict_value, policy_);
  // we expect the call to have succeeded and max bytes to be set properly
  ASSERT_TRUE(plan != NULL);
  EXPECT_EQ(2500000000LL, plan->GetDataBytesMax());
}

TEST_F(DataPlanTest, FromDictionaryValue32BitUnsignedOverflowingMaxBytes) {
  scoped_ptr<Value> value(JSONReader::Read(
      k32BitUnsignedOverflowingMaxBytesJSON, true));
  ASSERT_TRUE(value != NULL);
  ASSERT_TRUE(value->IsType(Value::TYPE_DICTIONARY));
  const DictionaryValue *dict_value =
      static_cast<const DictionaryValue*>(value.get());
  DataPlan *plan = DataPlan::FromDictionaryValue(dict_value, policy_);
  // we expect the call to have succeeded and max bytes to be set properly
  ASSERT_TRUE(plan != NULL);
  EXPECT_EQ(5000000000LL, plan->GetDataBytesMax());
}

TEST_F(DataPlanTest, TotalBytesAfterCtor) {
  // this test uses |current_unlimited_plan_| created in fixture SetUp
  // we expect plan to be initialized with local bytes == 0 and total bytes ==
  // |used_bytes|
  EXPECT_EQ(5 * 1024 * 1024, current_unlimited_plan_->GetDataBytesUsed());
  EXPECT_EQ(0, current_unlimited_plan_->GetLocalBytesUsed());
  EXPECT_EQ(5 * 1024 * 1024, current_unlimited_plan_->GetTotalBytesUsed());
}

TEST_F(DataPlanTest, TotalBytesAfterSetLocalBytes) {
  // this test uses |current_unlimited_plan_| created in fixture SetUp
  Bytes local_bytes = 10 * 1024 * 1024;
  current_unlimited_plan_->SetLocalBytesUsed(local_bytes);
  // we expect that, after we set local bytes, total bytes == data bytes +
  // local bytes
  EXPECT_EQ(5 * 1024 * 1024, current_unlimited_plan_->GetDataBytesUsed());
  EXPECT_EQ(local_bytes, current_unlimited_plan_->GetLocalBytesUsed());
  EXPECT_EQ(15 * 1024 * 1024, current_unlimited_plan_->GetTotalBytesUsed());
}

TEST_F(DataPlanTest, TotalBytesAfterOverflow) {
  // this test uses |current_unlimited_plan_| created in fixture SetUp
  Bytes local_bytes = kint64max;
  current_unlimited_plan_->SetLocalBytesUsed(local_bytes);
  // we expect that, after we set local bytes to a value that will cause the
  // total bytes computation to overflow, the overflow will be detected and
  // total bytes will be set to kint64max.
  EXPECT_EQ(5 * 1024 * 1024, current_unlimited_plan_->GetDataBytesUsed());
  EXPECT_EQ(local_bytes, current_unlimited_plan_->GetLocalBytesUsed());
  EXPECT_EQ(kint64max, current_unlimited_plan_->GetTotalBytesUsed());
}

TEST_F(DataPlanTest, IsActiveCurrentPlan) {
  std::string name = "Current Plan";
  DataPlan::Type type = DataPlan::kTypeMeteredFree;
  base::TimeDelta twelve_hours = base::TimeDelta::FromHours(12);
  base::Time now = base::Time::Now();
  base::Time start_time = now - twelve_hours;
  base::Time end_time = now + twelve_hours;
  Bytes max_bytes = 100 * 1024 * 1024;
  Bytes used_bytes = 5 * 1024 * 1024;
  DataPlan *plan = new(std::nothrow) DataPlan(name, type, now, start_time,
                                              end_time, max_bytes, used_bytes);
  ASSERT_TRUE(plan != NULL);
  // we expect that a current plan will be considered active
  EXPECT_TRUE(plan->IsActive());
  delete plan;
}

TEST_F(DataPlanTest, IsActiveFuturePlan) {
  std::string name = "Future Plan";
  DataPlan::Type type = DataPlan::kTypeMeteredFree;
  base::TimeDelta twelve_hours = base::TimeDelta::FromHours(12);
  base::Time now = base::Time::Now();
  base::Time start_time = now + twelve_hours;
  base::Time end_time = now + twelve_hours + twelve_hours;
  Bytes max_bytes = 100 * 1024 * 1024;
  Bytes used_bytes = 5 * 1024 * 1024;
  DataPlan *plan = new(std::nothrow) DataPlan(name, type, now, start_time,
                                              end_time, max_bytes, used_bytes);
  ASSERT_TRUE(plan != NULL);
  // we expect that a future plan will not be considered active
  EXPECT_FALSE(plan->IsActive());
  delete plan;
}

TEST_F(DataPlanTest, IsActiveExpiredPlan) {
  std::string name = "Expired Plan";
  DataPlan::Type type = DataPlan::kTypeMeteredFree;
  base::TimeDelta twelve_hours = base::TimeDelta::FromHours(12);
  base::Time now = base::Time::Now();
  base::Time start_time = now - twelve_hours - twelve_hours;
  base::Time end_time = now - twelve_hours;
  Bytes max_bytes = 100 * 1024 * 1024;
  Bytes used_bytes = 5 * 1024 * 1024;
  DataPlan *plan = new(std::nothrow) DataPlan(name, type, now, start_time,
                                              end_time, max_bytes, used_bytes);
  ASSERT_TRUE(plan != NULL);
  // we expect that an expired plan will not be considered active
  EXPECT_FALSE(plan->IsActive());
  delete plan;
}

TEST_F(DataPlanTest, IsActiveJustStartedPlan) {
  std::string name = "Just Started Plan";
  DataPlan::Type type = DataPlan::kTypeMeteredFree;
  base::TimeDelta twelve_hours = base::TimeDelta::FromHours(12);
  base::Time now = base::Time::Now();
  base::Time start_time = now;
  base::Time end_time = now + twelve_hours;
  Bytes max_bytes = 100 * 1024 * 1024;
  Bytes used_bytes = 5 * 1024 * 1024;
  DataPlan *plan = new(std::nothrow) DataPlan(name, type, now, start_time,
                                              end_time, max_bytes, used_bytes);
  ASSERT_TRUE(plan != NULL);
  // we expect that a plan with start time == now will be considered active
  EXPECT_TRUE(plan->IsActive());
  delete plan;
}

TEST_F(DataPlanTest, IsActiveJustExpiredPlan) {
  std::string name = "Just Expired Plan";
  DataPlan::Type type = DataPlan::kTypeMeteredFree;
  base::TimeDelta twelve_hours = base::TimeDelta::FromHours(12);
  base::Time now = base::Time::Now();
  base::Time start_time = now - twelve_hours;
  base::Time end_time = now;
  Bytes max_bytes = 100 * 1024 * 1024;
  Bytes used_bytes = 5 * 1024 * 1024;
  DataPlan *plan = new(std::nothrow) DataPlan(name, type, now, start_time,
                                              end_time, max_bytes, used_bytes);
  ASSERT_TRUE(plan != NULL);
  // we expect that a plan with end time == now will not be considered active
  EXPECT_FALSE(plan->IsActive());
  delete plan;
}

TEST_F(DataPlanTest, IsActiveAvailableMeteredPlan) {
  // this test uses |current_available_metered_plan_| created in fixture SetUp
  // we expect that a current metered plan that is not yet consumed will be
  // considered active
  EXPECT_TRUE(current_available_metered_plan_->IsActive());
}

TEST_F(DataPlanTest, IsActiveConsumedMeteredPlan) {
  std::string name = "Consumed Metered Plan";
  DataPlan::Type type = DataPlan::kTypeMeteredFree;
  base::TimeDelta twelve_hours = base::TimeDelta::FromHours(12);
  base::Time now = base::Time::Now();
  base::Time start_time = now - twelve_hours;
  base::Time end_time = now + twelve_hours;
  Bytes max_bytes = 100 * 1024 * 1024;
  Bytes used_bytes = 100 * 1024 * 1024;
  DataPlan *plan = new(std::nothrow) DataPlan(name, type, now, start_time,
                                              end_time, max_bytes, used_bytes);
  ASSERT_TRUE(plan != NULL);
  // we expect that a current metered plan that is consumed will not be
  // considered active
  EXPECT_FALSE(plan->IsActive());
  delete plan;
}

TEST_F(DataPlanTest, IsActiveConsumedLocalMeteredPlan) {
  // this test uses |current_available_metered_plan_| created in fixture SetUp
  current_available_metered_plan_->SetLocalBytesUsed(95 * 1024 * 1024);
  // we expect that a current metered plan that has used bytes < max bytes but
  // is consumed by virtue of local bytes + used bytes being >= max bytes will
  // not be considered active
  EXPECT_FALSE(current_available_metered_plan_->IsActive());
}

TEST_F(DataPlanTest, IsActiveUnlimitedPlan) {
  // this test uses |current_unlimited_plan_| created in fixture SetUp
  // we expect that a current unlimited plan will be considered active, even if
  // its total bytes is greater than its (meaningless) max bytes
  EXPECT_TRUE(current_unlimited_plan_->IsActive());
}

}  // namespace cashew
