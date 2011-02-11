// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/scoped_ptr.h>
#include <gtest/gtest.h>

#include "src/data_plan.h"
#include "src/device_mock.h"
#include "src/json_reader.h"
#include "src/policy.h"
#include "src/service_manager_mock.h"
#include "src/service_mock.h"

namespace cashew {

// test fixture for DataPlan unit tests
class DataPlanTest: public ::testing::Test {
  protected:
    DataPlanTest() : policy_(NULL), current_unlimited_plan_(NULL),
        current_available_metered_plan_(NULL), plan_one_(NULL), plan_two_(NULL),
        plan_three_(NULL) {}

    virtual ~DataPlanTest() {
      EXPECT_TRUE(policy_ == NULL);
      EXPECT_TRUE(current_unlimited_plan_ == NULL);
      EXPECT_TRUE(current_available_metered_plan_ == NULL);
      EXPECT_TRUE(plan_one_ == NULL);
      EXPECT_TRUE(plan_two_ == NULL);
      EXPECT_TRUE(plan_three_ == NULL);
    }

    static const int kBytesPerMegabyte = 1024 * 1024;

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
      ByteCount max_bytes = 0;
      ByteCount used_bytes = 5 * kBytesPerMegabyte;
      current_unlimited_plan_ = new(std::nothrow) DataPlan(
          "Current Unlimited Plan", type, now, start_time, end_time, max_bytes,
          used_bytes);
      ASSERT_TRUE(current_unlimited_plan_ != NULL);

      EXPECT_TRUE(current_available_metered_plan_ == NULL);
      type = DataPlan::kTypeMeteredFree;
      max_bytes = 100 * kBytesPerMegabyte;
      current_available_metered_plan_ = new(std::nothrow) DataPlan(
          "Current Available Metered Plan", type, now, start_time, end_time,
          max_bytes, used_bytes);
      ASSERT_TRUE(current_available_metered_plan_ != NULL);

      EXPECT_TRUE(plan_one_ == NULL);
      used_bytes = 0;
      plan_one_ = new(std::nothrow) DataPlan("Plan One", type, now, start_time,
                                             end_time, max_bytes, used_bytes);
      ASSERT_TRUE(plan_one_ != NULL);

      EXPECT_TRUE(plan_two_ == NULL);
      plan_two_ = new(std::nothrow) DataPlan("Plan Two", type, now, start_time,
                                             end_time, max_bytes, used_bytes);
      ASSERT_TRUE(plan_two_ != NULL);

      EXPECT_TRUE(plan_three_ == NULL);
      plan_three_ = new(std::nothrow) DataPlan("Plan Three", type, now,
                                               start_time, end_time, max_bytes,
                                               used_bytes);
      ASSERT_TRUE(plan_three_ != NULL);
    }

    virtual void TearDown() {
      delete plan_three_;
      plan_three_ = NULL;
      delete plan_two_;
      plan_two_ = NULL;
      delete plan_one_;
      plan_one_ = NULL;
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

    // three metered plans shared by multiple tests
    DataPlan *plan_one_;
    DataPlan *plan_two_;
    DataPlan *plan_three_;

    // mocks
    ServiceMock service_mock_;
    ServiceManagerMock service_manager_mock_;
    DeviceMock device_mock_;
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
  EXPECT_EQ(5 * kBytesPerMegabyte, current_unlimited_plan_->GetDataBytesUsed());
  EXPECT_EQ(0, current_unlimited_plan_->GetLocalBytesUsed());
  EXPECT_EQ(5 * kBytesPerMegabyte,
            current_unlimited_plan_->GetTotalBytesUsed());
}

TEST_F(DataPlanTest, TotalBytesAfterSetLocalBytes) {
  // this test uses |current_unlimited_plan_| created in fixture SetUp
  ByteCount local_bytes = 10 * kBytesPerMegabyte;
  current_unlimited_plan_->SetLocalBytesUsed(local_bytes);
  // we expect that, after we set local bytes, total bytes == data bytes +
  // local bytes
  EXPECT_EQ(5 * kBytesPerMegabyte, current_unlimited_plan_->GetDataBytesUsed());
  EXPECT_EQ(local_bytes, current_unlimited_plan_->GetLocalBytesUsed());
  EXPECT_EQ(15 * kBytesPerMegabyte,
            current_unlimited_plan_->GetTotalBytesUsed());
}

TEST_F(DataPlanTest, TotalBytesAfterOverflow) {
  // this test uses |current_unlimited_plan_| created in fixture SetUp
  ByteCount local_bytes = kint64max;
  current_unlimited_plan_->SetLocalBytesUsed(local_bytes);
  // we expect that, after we set local bytes to a value that will cause the
  // total bytes computation to overflow, the overflow will be detected and
  // total bytes will be set to kint64max.
  EXPECT_EQ(5 * kBytesPerMegabyte, current_unlimited_plan_->GetDataBytesUsed());
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
  ByteCount max_bytes = 100 * kBytesPerMegabyte;
  ByteCount used_bytes = 5 * kBytesPerMegabyte;
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
  ByteCount max_bytes = 100 * kBytesPerMegabyte;
  ByteCount used_bytes = 5 * kBytesPerMegabyte;
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
  ByteCount max_bytes = 100 * kBytesPerMegabyte;
  ByteCount used_bytes = 5 * kBytesPerMegabyte;
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
  ByteCount max_bytes = 100 * kBytesPerMegabyte;
  ByteCount used_bytes = 5 * kBytesPerMegabyte;
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
  ByteCount max_bytes = 100 * kBytesPerMegabyte;
  ByteCount used_bytes = 5 * kBytesPerMegabyte;
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
  ByteCount max_bytes = 100 * kBytesPerMegabyte;
  ByteCount used_bytes = 100 * kBytesPerMegabyte;
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
  current_available_metered_plan_->SetLocalBytesUsed(95 * kBytesPerMegabyte);
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

// tests for local byte counter maintenance across plan transitions

// empty list, we should stop the counter
TEST_F(DataPlanTest, NoActivePlans) {
  // create empty list
  DataPlanList data_plans;
  ASSERT_TRUE(data_plans.empty());

  // set up mock expectations and method return values
  {
    testing::InSequence expect_calls_in_order;
    EXPECT_CALL(device_mock_, ByteCounterRunning())
        .WillRepeatedly(testing::Return(true));
    EXPECT_CALL(device_mock_, StopByteCounter()).Times(1);
  }

  // simulate a byte counter update
  uint64 rx_bytes = 25 * kBytesPerMegabyte;
  uint64 tx_bytes = 25 * kBytesPerMegabyte;
  bool result = DataPlan::OnByteCounterUpdate(&data_plans, &service_mock_,
                                              &service_manager_mock_,
                                              &device_mock_, rx_bytes,
                                              tx_bytes);

  // check if test passed
  // failure will most likely manifest as violated mock expectation
  EXPECT_FALSE(result);
}

// single plan, usage doesn't consume
TEST_F(DataPlanTest, OnePlanNotConsumed) {
  DataPlanList data_plans;
  data_plans.push_back(plan_one_);

  // set up mock expectations and method return values
  EXPECT_CALL(device_mock_, ByteCounterRunning())
      .WillRepeatedly(testing::Return(true));

  // simulate a byte counter update
  uint64 rx_bytes = 25 * kBytesPerMegabyte;
  uint64 tx_bytes = 25 * kBytesPerMegabyte;
  bool result = DataPlan::OnByteCounterUpdate(&data_plans, &service_mock_,
                                              &service_manager_mock_,
                                              &device_mock_, rx_bytes,
                                              tx_bytes);

  // check if test passed
  EXPECT_TRUE(result);
  EXPECT_EQ(plan_one_->GetDataBytesMax(), 100 * kBytesPerMegabyte);
  EXPECT_EQ(plan_one_->GetDataBytesUsed(), 0);
  EXPECT_EQ(plan_one_->GetLocalBytesUsed(), 50 * kBytesPerMegabyte);
  EXPECT_EQ(plan_one_->GetTotalBytesUsed(), 50 * kBytesPerMegabyte);
  EXPECT_TRUE(plan_one_->IsActive());
}

// single plan, usage exactly consumes
TEST_F(DataPlanTest, OnePlanConsumed) {
  DataPlanList data_plans;
  data_plans.push_back(plan_one_);

  // set up mock expectations and method return values
  {
    testing::InSequence expect_calls_in_order;
    EXPECT_CALL(device_mock_, ByteCounterRunning())
        .WillRepeatedly(testing::Return(true));
    EXPECT_CALL(service_manager_mock_,
                OnDataPlanInactive(testing::Ref(service_mock_),
                                   testing::Ref(*plan_one_))).Times(1);
    EXPECT_CALL(device_mock_, ResetByteCounter(0, 0)).Times(1);
    EXPECT_CALL(device_mock_, StopByteCounter()).Times(1);
  }

  // simulate a byte counter update
  uint64 rx_bytes = 50 * kBytesPerMegabyte;
  uint64 tx_bytes = 50 * kBytesPerMegabyte;
  bool result = DataPlan::OnByteCounterUpdate(&data_plans, &service_mock_,
                                              &service_manager_mock_,
                                              &device_mock_, rx_bytes,
                                              tx_bytes);

  // check if test passed
  EXPECT_TRUE(result);
  EXPECT_EQ(plan_one_->GetDataBytesMax(), 100 * kBytesPerMegabyte);
  EXPECT_EQ(plan_one_->GetDataBytesUsed(), 0);
  EXPECT_EQ(plan_one_->GetLocalBytesUsed(), 100 * kBytesPerMegabyte);
  EXPECT_EQ(plan_one_->GetTotalBytesUsed(), 100 * kBytesPerMegabyte);
  EXPECT_FALSE(plan_one_->IsActive());
}

// single plan, usage consumes and leaves overage but no active plan
TEST_F(DataPlanTest, OnePlanConsumedWithOverage) {
  DataPlanList data_plans;
  data_plans.push_back(plan_one_);

  // set up mock expectations and method return values
  {
    testing::InSequence expect_calls_in_order;
    EXPECT_CALL(device_mock_, ByteCounterRunning())
        .WillRepeatedly(testing::Return(true));
    EXPECT_CALL(service_manager_mock_,
                OnDataPlanInactive(testing::Ref(service_mock_),
                                   testing::Ref(*plan_one_))).Times(1);
    EXPECT_CALL(device_mock_, ResetByteCounter(10 * kBytesPerMegabyte,
                                               0)).Times(1);
    EXPECT_CALL(device_mock_, StopByteCounter()).Times(1);
  }

  // simulate a byte counter update
  uint64 rx_bytes = 55 * kBytesPerMegabyte;
  uint64 tx_bytes = 55 * kBytesPerMegabyte;
  bool result = DataPlan::OnByteCounterUpdate(&data_plans, &service_mock_,
                                              &service_manager_mock_,
                                              &device_mock_, rx_bytes,
                                              tx_bytes);

  // check if test passed
  EXPECT_TRUE(result);
  EXPECT_EQ(plan_one_->GetDataBytesMax(), 100 * kBytesPerMegabyte);
  EXPECT_EQ(plan_one_->GetDataBytesUsed(), 0);
  EXPECT_EQ(plan_one_->GetLocalBytesUsed(), 100 * kBytesPerMegabyte);
  EXPECT_EQ(plan_one_->GetTotalBytesUsed(), 100 * kBytesPerMegabyte);
  EXPECT_FALSE(plan_one_->IsActive());
}

// two plans, first is inactive because quota consumed and second is active
// usage is applied to but does not consume second plan
TEST_F(DataPlanTest, TwoPlansFirstInactiveConsumed) {
  // create consumed plan and put it in list
  std::string name = "Consumed Plan";
  DataPlan::Type type = DataPlan::kTypeMeteredFree;
  base::TimeDelta twelve_hours = base::TimeDelta::FromHours(12);
  base::Time now = base::Time::Now();
  base::Time start_time = now - twelve_hours;
  base::Time end_time = now + twelve_hours;
  ByteCount max_bytes = 100 * kBytesPerMegabyte;
  ByteCount used_bytes = 100 * kBytesPerMegabyte;
  DataPlan *consumed_plan = new(std::nothrow) DataPlan(name, type, now,
                                                       start_time, end_time,
                                                       max_bytes, used_bytes);
  ASSERT_TRUE(consumed_plan != NULL);
  DataPlanList data_plans;
  data_plans.push_back(consumed_plan);

  // use active plan from test fixture
  data_plans.push_back(plan_two_);

  // set up mock expectations and method return values
  EXPECT_CALL(device_mock_, ByteCounterRunning())
      .WillRepeatedly(testing::Return(true));

  // simulate a byte counter update
  uint64 rx_bytes = 25 * kBytesPerMegabyte;
  uint64 tx_bytes = 25 * kBytesPerMegabyte;
  bool result = DataPlan::OnByteCounterUpdate(&data_plans, &service_mock_,
                                              &service_manager_mock_,
                                              &device_mock_, rx_bytes,
                                              tx_bytes);

  // check if test passed
  EXPECT_TRUE(result);
  EXPECT_EQ(consumed_plan->GetDataBytesMax(), 100 * kBytesPerMegabyte);
  EXPECT_EQ(consumed_plan->GetDataBytesUsed(), 100 * kBytesPerMegabyte);
  EXPECT_EQ(consumed_plan->GetLocalBytesUsed(), 0);
  EXPECT_EQ(consumed_plan->GetTotalBytesUsed(), 100 * kBytesPerMegabyte);
  EXPECT_FALSE(consumed_plan->IsActive());
  EXPECT_EQ(plan_two_->GetDataBytesMax(), 100 * kBytesPerMegabyte);
  EXPECT_EQ(plan_two_->GetDataBytesUsed(), 0);
  EXPECT_EQ(plan_two_->GetLocalBytesUsed(), 50 * kBytesPerMegabyte);
  EXPECT_EQ(plan_two_->GetTotalBytesUsed(), 50 * kBytesPerMegabyte);
  EXPECT_TRUE(plan_two_->IsActive());

  // clean up
  delete consumed_plan;
}

// two plans, first is inactive because it has expired and second is active
// usage is applied to but does not consume second plan
TEST_F(DataPlanTest, TwoPlansFirstInactiveExpired) {
  // create expired plan and put it in list
  std::string name = "Expired Plan";
  DataPlan::Type type = DataPlan::kTypeMeteredFree;
  base::TimeDelta twelve_hours = base::TimeDelta::FromHours(12);
  base::Time now = base::Time::Now();
  base::Time start_time = now - 2 * twelve_hours;
  base::Time end_time = now - twelve_hours;
  ByteCount max_bytes = 100 * kBytesPerMegabyte;
  ByteCount used_bytes = 0;
  DataPlan *expired_plan = new(std::nothrow) DataPlan(name, type, now,
                                                      start_time, end_time,
                                                      max_bytes, used_bytes);
  ASSERT_TRUE(expired_plan != NULL);
  DataPlanList data_plans;
  data_plans.push_back(expired_plan);

  // use active plan from test fixture
  data_plans.push_back(plan_two_);

  // set up mock expectations and method return values
  EXPECT_CALL(device_mock_, ByteCounterRunning())
      .WillRepeatedly(testing::Return(true));

  // simulate a byte counter update
  uint64 rx_bytes = 25 * kBytesPerMegabyte;
  uint64 tx_bytes = 25 * kBytesPerMegabyte;
  bool result = DataPlan::OnByteCounterUpdate(&data_plans, &service_mock_,
                                              &service_manager_mock_,
                                              &device_mock_, rx_bytes,
                                              tx_bytes);

  // check if test passed
  EXPECT_TRUE(result);
  EXPECT_EQ(expired_plan->GetDataBytesMax(), 100 * kBytesPerMegabyte);
  EXPECT_EQ(expired_plan->GetDataBytesUsed(), 0);
  EXPECT_EQ(expired_plan->GetLocalBytesUsed(), 0);
  EXPECT_EQ(expired_plan->GetTotalBytesUsed(), 0);
  EXPECT_FALSE(expired_plan->IsActive());
  EXPECT_EQ(plan_two_->GetDataBytesMax(), 100 * kBytesPerMegabyte);
  EXPECT_EQ(plan_two_->GetDataBytesUsed(), 0);
  EXPECT_EQ(plan_two_->GetLocalBytesUsed(), 50 * kBytesPerMegabyte);
  EXPECT_EQ(plan_two_->GetTotalBytesUsed(), 50 * kBytesPerMegabyte);
  EXPECT_TRUE(plan_two_->IsActive());

  // clean up
  delete expired_plan;
}

// two plans, first is unlimited and should absorb all usage
// second plan should remain untouched
TEST_F(DataPlanTest, TwoPlansFirstUnlimited) {
  // use plans from test fixture
  DataPlanList data_plans;
  data_plans.push_back(current_unlimited_plan_);
  data_plans.push_back(plan_two_);

  // set up mock expectations and method return values
  EXPECT_CALL(device_mock_, ByteCounterRunning())
      .WillRepeatedly(testing::Return(true));

  // simulate a byte counter update
  uint64 rx_bytes = 100 * kBytesPerMegabyte;
  uint64 tx_bytes = 100 * kBytesPerMegabyte;
  bool result = DataPlan::OnByteCounterUpdate(&data_plans, &service_mock_,
                                              &service_manager_mock_,
                                              &device_mock_, rx_bytes,
                                              tx_bytes);

  // check if test passed
  EXPECT_TRUE(result);
  EXPECT_EQ(current_unlimited_plan_->GetDataBytesMax(), 0);
  EXPECT_EQ(current_unlimited_plan_->GetDataBytesUsed(), 5 * kBytesPerMegabyte);
  EXPECT_EQ(current_unlimited_plan_->GetLocalBytesUsed(),
            200 * kBytesPerMegabyte);
  EXPECT_EQ(current_unlimited_plan_->GetTotalBytesUsed(),
            205 * kBytesPerMegabyte);
  EXPECT_TRUE(current_unlimited_plan_->IsActive());
  EXPECT_EQ(plan_two_->GetDataBytesMax(), 100 * kBytesPerMegabyte);
  EXPECT_EQ(plan_two_->GetDataBytesUsed(), 0);
  EXPECT_EQ(plan_two_->GetLocalBytesUsed(), 0);
  EXPECT_EQ(plan_two_->GetTotalBytesUsed(), 0);
  EXPECT_TRUE(plan_two_->IsActive());
}

// two plans, the first is metered and the second is unlimited
// usage consumes the first plan, and the overage is absorbed by the second
TEST_F(DataPlanTest, TwoPlansSecondUnlimited) {
  // use plans from test fixture
  DataPlanList data_plans;
  data_plans.push_back(plan_one_);
  data_plans.push_back(current_unlimited_plan_);

  // set up mock expectations and method return values
  {
    testing::InSequence expect_calls_in_order;
    EXPECT_CALL(device_mock_, ByteCounterRunning())
        .WillRepeatedly(testing::Return(true));
    EXPECT_CALL(service_manager_mock_,
                OnDataPlanInactive(testing::Ref(service_mock_),
                                   testing::Ref(*plan_one_))).Times(1);
    EXPECT_CALL(device_mock_, ResetByteCounter(50 * kBytesPerMegabyte,
                                               0)).Times(1);
  }

  // simulate a byte counter update
  uint64 rx_bytes = 75 * kBytesPerMegabyte;
  uint64 tx_bytes = 75 * kBytesPerMegabyte;
  bool result = DataPlan::OnByteCounterUpdate(&data_plans, &service_mock_,
                                              &service_manager_mock_,
                                              &device_mock_, rx_bytes,
                                              tx_bytes);

  // check if test passed
  EXPECT_TRUE(result);
  EXPECT_EQ(plan_one_->GetDataBytesMax(), 100 * kBytesPerMegabyte);
  EXPECT_EQ(plan_one_->GetDataBytesUsed(), 0);
  EXPECT_EQ(plan_one_->GetLocalBytesUsed(), 100 * kBytesPerMegabyte);
  EXPECT_EQ(plan_one_->GetTotalBytesUsed(), 100 * kBytesPerMegabyte);
  EXPECT_FALSE(plan_one_->IsActive());
  EXPECT_EQ(current_unlimited_plan_->GetDataBytesMax(), 0);
  EXPECT_EQ(current_unlimited_plan_->GetDataBytesUsed(), 5 * kBytesPerMegabyte);
  EXPECT_EQ(current_unlimited_plan_->GetLocalBytesUsed(),
            50 * kBytesPerMegabyte);
  EXPECT_EQ(current_unlimited_plan_->GetTotalBytesUsed(),
            55 * kBytesPerMegabyte);
  EXPECT_TRUE(current_unlimited_plan_->IsActive());
}

// two plans, usage doesn't consume first
TEST_F(DataPlanTest, TwoPlansFirstNotConsumed) {
  DataPlanList data_plans;
  data_plans.push_back(plan_one_);
  data_plans.push_back(plan_two_);

  // set up mock expectations and method return values
  EXPECT_CALL(device_mock_, ByteCounterRunning())
      .WillRepeatedly(testing::Return(true));

  // simulate a byte counter update
  uint64 rx_bytes = 25 * kBytesPerMegabyte;
  uint64 tx_bytes = 25 * kBytesPerMegabyte;
  bool result = DataPlan::OnByteCounterUpdate(&data_plans, &service_mock_,
                                              &service_manager_mock_,
                                              &device_mock_, rx_bytes,
                                              tx_bytes);

  // check if test passed
  EXPECT_TRUE(result);
  EXPECT_EQ(plan_one_->GetDataBytesMax(), 100 * kBytesPerMegabyte);
  EXPECT_EQ(plan_one_->GetDataBytesUsed(), 0);
  EXPECT_EQ(plan_one_->GetLocalBytesUsed(), 50 * kBytesPerMegabyte);
  EXPECT_EQ(plan_one_->GetTotalBytesUsed(), 50 * kBytesPerMegabyte);
  EXPECT_TRUE(plan_one_->IsActive());
  EXPECT_EQ(plan_two_->GetDataBytesMax(), 100 * kBytesPerMegabyte);
  EXPECT_EQ(plan_two_->GetDataBytesUsed(), 0);
  EXPECT_EQ(plan_two_->GetLocalBytesUsed(), 0);
  EXPECT_EQ(plan_two_->GetTotalBytesUsed(), 0);
  EXPECT_TRUE(plan_two_->IsActive());
}

// two plans, usage consumes first, doesn't consume second
TEST_F(DataPlanTest, TwoPlansSecondNotConsumed) {
  DataPlanList data_plans;
  data_plans.push_back(plan_one_);
  data_plans.push_back(plan_two_);

  // set up mock expectations and method return values
  {
    testing::InSequence expect_calls_in_order;
    EXPECT_CALL(device_mock_, ByteCounterRunning())
        .WillRepeatedly(testing::Return(true));
    EXPECT_CALL(service_manager_mock_,
                OnDataPlanInactive(testing::Ref(service_mock_),
                                   testing::Ref(*plan_one_))).Times(1);
    EXPECT_CALL(device_mock_, ResetByteCounter(50 * kBytesPerMegabyte,
                                               0)).Times(1);
  }

  // simulate a byte counter update
  uint64 rx_bytes = 75 * kBytesPerMegabyte;
  uint64 tx_bytes = 75 * kBytesPerMegabyte;
  bool result = DataPlan::OnByteCounterUpdate(&data_plans, &service_mock_,
                                              &service_manager_mock_,
                                              &device_mock_, rx_bytes,
                                              tx_bytes);

  // check if test passed
  EXPECT_TRUE(result);
  EXPECT_EQ(plan_one_->GetDataBytesMax(), 100 * kBytesPerMegabyte);
  EXPECT_EQ(plan_one_->GetDataBytesUsed(), 0);
  EXPECT_EQ(plan_one_->GetLocalBytesUsed(), 100 * kBytesPerMegabyte);
  EXPECT_EQ(plan_one_->GetTotalBytesUsed(), 100 * kBytesPerMegabyte);
  EXPECT_FALSE(plan_one_->IsActive());
  EXPECT_EQ(plan_two_->GetDataBytesMax(), 100 * kBytesPerMegabyte);
  EXPECT_EQ(plan_two_->GetDataBytesUsed(), 0);
  EXPECT_EQ(plan_two_->GetLocalBytesUsed(), 50 * kBytesPerMegabyte);
  EXPECT_EQ(plan_two_->GetTotalBytesUsed(), 50 * kBytesPerMegabyte);
  EXPECT_TRUE(plan_two_->IsActive());
}

// two plans, usage consumes first, consumes second exactly
TEST_F(DataPlanTest, TwoPlansSecondConsumed) {
  DataPlanList data_plans;
  data_plans.push_back(plan_one_);
  data_plans.push_back(plan_two_);

  // set up mock expectations and method return values
  {
    testing::InSequence expect_calls_in_order;
    EXPECT_CALL(device_mock_, ByteCounterRunning())
        .WillRepeatedly(testing::Return(true));
    EXPECT_CALL(service_manager_mock_,
                OnDataPlanInactive(testing::Ref(service_mock_),
                                   testing::Ref(*plan_one_))).Times(1);
    EXPECT_CALL(device_mock_, ResetByteCounter(100 * kBytesPerMegabyte,
                                               0)).Times(1);
    EXPECT_CALL(service_manager_mock_,
                OnDataPlanInactive(testing::Ref(service_mock_),
                                   testing::Ref(*plan_two_))).Times(1);
    EXPECT_CALL(device_mock_, ResetByteCounter(0, 0)).Times(1);
    EXPECT_CALL(device_mock_, StopByteCounter()).Times(1);
  }

  // simulate a byte counter update
  uint64 rx_bytes = 100 * kBytesPerMegabyte;
  uint64 tx_bytes = 100 * kBytesPerMegabyte;
  bool result = DataPlan::OnByteCounterUpdate(&data_plans, &service_mock_,
                                              &service_manager_mock_,
                                              &device_mock_, rx_bytes,
                                              tx_bytes);

  // check if test passed
  EXPECT_TRUE(result);
  EXPECT_EQ(plan_one_->GetDataBytesMax(), 100 * kBytesPerMegabyte);
  EXPECT_EQ(plan_one_->GetDataBytesUsed(), 0);
  EXPECT_EQ(plan_one_->GetLocalBytesUsed(), 100 * kBytesPerMegabyte);
  EXPECT_EQ(plan_one_->GetTotalBytesUsed(), 100 * kBytesPerMegabyte);
  EXPECT_FALSE(plan_one_->IsActive());
  EXPECT_EQ(plan_two_->GetDataBytesMax(), 100 * kBytesPerMegabyte);
  EXPECT_EQ(plan_two_->GetDataBytesUsed(), 0);
  EXPECT_EQ(plan_two_->GetLocalBytesUsed(), 100 * kBytesPerMegabyte);
  EXPECT_EQ(plan_two_->GetTotalBytesUsed(), 100 * kBytesPerMegabyte);
  EXPECT_FALSE(plan_two_->IsActive());
}

// two plans, usage consumes first, consumes second, with overage
TEST_F(DataPlanTest, TwoPlansSecondConsumedWithOverage) {
  DataPlanList data_plans;
  data_plans.push_back(plan_one_);
  data_plans.push_back(plan_two_);

  // set up mock expectations and method return values
  {
    testing::InSequence expect_calls_in_order;
    EXPECT_CALL(device_mock_, ByteCounterRunning())
        .WillRepeatedly(testing::Return(true));
    EXPECT_CALL(service_manager_mock_,
                OnDataPlanInactive(testing::Ref(service_mock_),
                                   testing::Ref(*plan_one_))).Times(1);
    EXPECT_CALL(device_mock_, ResetByteCounter(110 * kBytesPerMegabyte,
                                               0)).Times(1);
    EXPECT_CALL(service_manager_mock_,
                OnDataPlanInactive(testing::Ref(service_mock_),
                                   testing::Ref(*plan_two_))).Times(1);
    EXPECT_CALL(device_mock_, ResetByteCounter(10 * kBytesPerMegabyte,
                                               0)).Times(1);
    EXPECT_CALL(device_mock_, StopByteCounter()).Times(1);
  }

  // simulate a byte counter update
  uint64 rx_bytes = 105 * kBytesPerMegabyte;
  uint64 tx_bytes = 105 * kBytesPerMegabyte;
  bool result = DataPlan::OnByteCounterUpdate(&data_plans, &service_mock_,
                                              &service_manager_mock_,
                                              &device_mock_, rx_bytes,
                                              tx_bytes);

  // check if test passed
  EXPECT_TRUE(result);
  EXPECT_EQ(plan_one_->GetDataBytesMax(), 100 * kBytesPerMegabyte);
  EXPECT_EQ(plan_one_->GetDataBytesUsed(), 0);
  EXPECT_EQ(plan_one_->GetLocalBytesUsed(), 100 * kBytesPerMegabyte);
  EXPECT_EQ(plan_one_->GetTotalBytesUsed(), 100 * kBytesPerMegabyte);
  EXPECT_FALSE(plan_one_->IsActive());
  EXPECT_EQ(plan_two_->GetDataBytesMax(), 100 * kBytesPerMegabyte);
  EXPECT_EQ(plan_two_->GetDataBytesUsed(), 0);
  EXPECT_EQ(plan_two_->GetLocalBytesUsed(), 100 * kBytesPerMegabyte);
  EXPECT_EQ(plan_two_->GetTotalBytesUsed(), 100 * kBytesPerMegabyte);
  EXPECT_FALSE(plan_two_->IsActive());
}

// three plans, usage doesn't consume first
TEST_F(DataPlanTest, ThreePlansFirstNotConsumed) {
  DataPlanList data_plans;
  data_plans.push_back(plan_one_);
  data_plans.push_back(plan_two_);
  data_plans.push_back(plan_three_);

  // set up mock expectations and method return values
  EXPECT_CALL(device_mock_, ByteCounterRunning())
      .WillRepeatedly(testing::Return(true));

  // simulate a byte counter update
  uint64 rx_bytes = 25 * kBytesPerMegabyte;
  uint64 tx_bytes = 25 * kBytesPerMegabyte;
  bool result = DataPlan::OnByteCounterUpdate(&data_plans, &service_mock_,
                                              &service_manager_mock_,
                                              &device_mock_, rx_bytes,
                                              tx_bytes);

  // check if test passed
  EXPECT_TRUE(result);
  EXPECT_EQ(plan_one_->GetDataBytesMax(), 100 * kBytesPerMegabyte);
  EXPECT_EQ(plan_one_->GetDataBytesUsed(), 0);
  EXPECT_EQ(plan_one_->GetLocalBytesUsed(), 50 * kBytesPerMegabyte);
  EXPECT_EQ(plan_one_->GetTotalBytesUsed(), 50 * kBytesPerMegabyte);
  EXPECT_TRUE(plan_one_->IsActive());
  EXPECT_EQ(plan_two_->GetDataBytesMax(), 100 * kBytesPerMegabyte);
  EXPECT_EQ(plan_two_->GetDataBytesUsed(), 0);
  EXPECT_EQ(plan_two_->GetLocalBytesUsed(), 0);
  EXPECT_EQ(plan_two_->GetTotalBytesUsed(), 0);
  EXPECT_TRUE(plan_two_->IsActive());
  EXPECT_EQ(plan_three_->GetDataBytesMax(), 100 * kBytesPerMegabyte);
  EXPECT_EQ(plan_three_->GetDataBytesUsed(), 0);
  EXPECT_EQ(plan_three_->GetLocalBytesUsed(), 0);
  EXPECT_EQ(plan_three_->GetTotalBytesUsed(), 0);
  EXPECT_TRUE(plan_three_->IsActive());
}

// three plans, usage consumes first, doesn't consume second
TEST_F(DataPlanTest, ThreePlansSecondNotConsumed) {
  DataPlanList data_plans;
  data_plans.push_back(plan_one_);
  data_plans.push_back(plan_two_);
  data_plans.push_back(plan_three_);

  // set up mock expectations and method return values
  {
    testing::InSequence expect_calls_in_order;
    EXPECT_CALL(device_mock_, ByteCounterRunning())
        .WillRepeatedly(testing::Return(true));
    EXPECT_CALL(service_manager_mock_,
                OnDataPlanInactive(testing::Ref(service_mock_),
                                   testing::Ref(*plan_one_))).Times(1);
    EXPECT_CALL(device_mock_, ResetByteCounter(50 * kBytesPerMegabyte,
                                               0)).Times(1);
  }

  // simulate a byte counter update
  uint64 rx_bytes = 75 * kBytesPerMegabyte;
  uint64 tx_bytes = 75 * kBytesPerMegabyte;
  bool result = DataPlan::OnByteCounterUpdate(&data_plans, &service_mock_,
                                              &service_manager_mock_,
                                              &device_mock_, rx_bytes,
                                              tx_bytes);

  // check if test passed
  EXPECT_TRUE(result);
  EXPECT_EQ(plan_one_->GetDataBytesMax(), 100 * kBytesPerMegabyte);
  EXPECT_EQ(plan_one_->GetDataBytesUsed(), 0);
  EXPECT_EQ(plan_one_->GetLocalBytesUsed(), 100 * kBytesPerMegabyte);
  EXPECT_EQ(plan_one_->GetTotalBytesUsed(), 100 * kBytesPerMegabyte);
  EXPECT_FALSE(plan_one_->IsActive());
  EXPECT_EQ(plan_two_->GetDataBytesMax(), 100 * kBytesPerMegabyte);
  EXPECT_EQ(plan_two_->GetDataBytesUsed(), 0);
  EXPECT_EQ(plan_two_->GetLocalBytesUsed(), 50 * kBytesPerMegabyte);
  EXPECT_EQ(plan_two_->GetTotalBytesUsed(), 50 * kBytesPerMegabyte);
  EXPECT_TRUE(plan_two_->IsActive());
  EXPECT_EQ(plan_three_->GetDataBytesMax(), 100 * kBytesPerMegabyte);
  EXPECT_EQ(plan_three_->GetDataBytesUsed(), 0);
  EXPECT_EQ(plan_three_->GetLocalBytesUsed(), 0);
  EXPECT_EQ(plan_three_->GetTotalBytesUsed(), 0);
  EXPECT_TRUE(plan_three_->IsActive());
}

// three plans, usage consumes first, consumes second, doesn't consume third
TEST_F(DataPlanTest, ThreePlansThirdNotConsumed) {
  DataPlanList data_plans;
  data_plans.push_back(plan_one_);
  data_plans.push_back(plan_two_);
  data_plans.push_back(plan_three_);

  // set up mock expectations and method return values
  {
    testing::InSequence expect_calls_in_order;
    EXPECT_CALL(device_mock_, ByteCounterRunning())
        .WillRepeatedly(testing::Return(true));
    EXPECT_CALL(service_manager_mock_,
                OnDataPlanInactive(testing::Ref(service_mock_),
                                   testing::Ref(*plan_one_))).Times(1);
    EXPECT_CALL(device_mock_, ResetByteCounter(150 * kBytesPerMegabyte,
                                               0)).Times(1);
    EXPECT_CALL(service_manager_mock_,
                OnDataPlanInactive(testing::Ref(service_mock_),
                                   testing::Ref(*plan_two_))).Times(1);
    EXPECT_CALL(device_mock_, ResetByteCounter(50 * kBytesPerMegabyte,
                                               0)).Times(1);
  }

  // simulate a byte counter update
  uint64 rx_bytes = 125 * kBytesPerMegabyte;
  uint64 tx_bytes = 125 * kBytesPerMegabyte;
  bool result = DataPlan::OnByteCounterUpdate(&data_plans, &service_mock_,
                                              &service_manager_mock_,
                                              &device_mock_, rx_bytes,
                                              tx_bytes);

  // check if test passed
  EXPECT_TRUE(result);
  EXPECT_EQ(plan_one_->GetDataBytesMax(), 100 * kBytesPerMegabyte);
  EXPECT_EQ(plan_one_->GetDataBytesUsed(), 0);
  EXPECT_EQ(plan_one_->GetLocalBytesUsed(), 100 * kBytesPerMegabyte);
  EXPECT_EQ(plan_one_->GetTotalBytesUsed(), 100 * kBytesPerMegabyte);
  EXPECT_FALSE(plan_one_->IsActive());
  EXPECT_EQ(plan_two_->GetDataBytesMax(), 100 * kBytesPerMegabyte);
  EXPECT_EQ(plan_two_->GetDataBytesUsed(), 0);
  EXPECT_EQ(plan_two_->GetLocalBytesUsed(), 100 * kBytesPerMegabyte);
  EXPECT_EQ(plan_two_->GetTotalBytesUsed(), 100 * kBytesPerMegabyte);
  EXPECT_FALSE(plan_two_->IsActive());
  EXPECT_EQ(plan_three_->GetDataBytesMax(), 100 * kBytesPerMegabyte);
  EXPECT_EQ(plan_three_->GetDataBytesUsed(), 0);
  EXPECT_EQ(plan_three_->GetLocalBytesUsed(), 50 * kBytesPerMegabyte);
  EXPECT_EQ(plan_three_->GetTotalBytesUsed(), 50 * kBytesPerMegabyte);
  EXPECT_TRUE(plan_three_->IsActive());
}

// three plans, usage consumes first, consumes second, exactly consumes third
TEST_F(DataPlanTest, ThreePlansThirdConsumed) {
  DataPlanList data_plans;
  data_plans.push_back(plan_one_);
  data_plans.push_back(plan_two_);
  data_plans.push_back(plan_three_);

  // set up mock expectations and method return values
  {
    testing::InSequence expect_calls_in_order;
    EXPECT_CALL(device_mock_, ByteCounterRunning())
        .WillRepeatedly(testing::Return(true));
    EXPECT_CALL(service_manager_mock_,
                OnDataPlanInactive(testing::Ref(service_mock_),
                                   testing::Ref(*plan_one_))).Times(1);
    EXPECT_CALL(device_mock_, ResetByteCounter(200 * kBytesPerMegabyte,
                                               0)).Times(1);
    EXPECT_CALL(service_manager_mock_,
                OnDataPlanInactive(testing::Ref(service_mock_),
                                   testing::Ref(*plan_two_))).Times(1);
    EXPECT_CALL(device_mock_, ResetByteCounter(100 * kBytesPerMegabyte,
                                               0)).Times(1);
    EXPECT_CALL(service_manager_mock_,
                OnDataPlanInactive(testing::Ref(service_mock_),
                                   testing::Ref(*plan_three_))).Times(1);
    EXPECT_CALL(device_mock_, ResetByteCounter(0, 0)).Times(1);
    EXPECT_CALL(device_mock_, StopByteCounter()).Times(1);
  }

  // simulate a byte counter update
  uint64 rx_bytes = 150 * kBytesPerMegabyte;
  uint64 tx_bytes = 150 * kBytesPerMegabyte;
  bool result = DataPlan::OnByteCounterUpdate(&data_plans, &service_mock_,
                                              &service_manager_mock_,
                                              &device_mock_, rx_bytes,
                                              tx_bytes);

  // check if test passed
  EXPECT_TRUE(result);
  EXPECT_EQ(plan_one_->GetDataBytesMax(), 100 * kBytesPerMegabyte);
  EXPECT_EQ(plan_one_->GetDataBytesUsed(), 0);
  EXPECT_EQ(plan_one_->GetLocalBytesUsed(), 100 * kBytesPerMegabyte);
  EXPECT_EQ(plan_one_->GetTotalBytesUsed(), 100 * kBytesPerMegabyte);
  EXPECT_FALSE(plan_one_->IsActive());
  EXPECT_EQ(plan_two_->GetDataBytesMax(), 100 * kBytesPerMegabyte);
  EXPECT_EQ(plan_two_->GetDataBytesUsed(), 0);
  EXPECT_EQ(plan_two_->GetLocalBytesUsed(), 100 * kBytesPerMegabyte);
  EXPECT_EQ(plan_two_->GetTotalBytesUsed(), 100 * kBytesPerMegabyte);
  EXPECT_FALSE(plan_two_->IsActive());
  EXPECT_EQ(plan_three_->GetDataBytesMax(), 100 * kBytesPerMegabyte);
  EXPECT_EQ(plan_three_->GetDataBytesUsed(), 0);
  EXPECT_EQ(plan_three_->GetLocalBytesUsed(), 100 * kBytesPerMegabyte);
  EXPECT_EQ(plan_three_->GetTotalBytesUsed(), 100 * kBytesPerMegabyte);
  EXPECT_FALSE(plan_three_->IsActive());
}

// three plans, usage consumes first, consumes second, consumes third w/ overage
TEST_F(DataPlanTest, ThreePlansThirdConsumedWithOverage) {
  DataPlanList data_plans;
  data_plans.push_back(plan_one_);
  data_plans.push_back(plan_two_);
  data_plans.push_back(plan_three_);

  // set up mock expectations and method return values
  {
    testing::InSequence expect_calls_in_order;
    EXPECT_CALL(device_mock_, ByteCounterRunning())
        .WillRepeatedly(testing::Return(true));
    EXPECT_CALL(service_manager_mock_,
                OnDataPlanInactive(testing::Ref(service_mock_),
                                   testing::Ref(*plan_one_))).Times(1);
    EXPECT_CALL(device_mock_, ResetByteCounter(210 * kBytesPerMegabyte,
                                               0)).Times(1);
    EXPECT_CALL(service_manager_mock_,
                OnDataPlanInactive(testing::Ref(service_mock_),
                                   testing::Ref(*plan_two_))).Times(1);
    EXPECT_CALL(device_mock_, ResetByteCounter(110 * kBytesPerMegabyte,
                                               0)).Times(1);
    EXPECT_CALL(service_manager_mock_,
                OnDataPlanInactive(testing::Ref(service_mock_),
                                   testing::Ref(*plan_three_))).Times(1);
    EXPECT_CALL(device_mock_, ResetByteCounter(10 * kBytesPerMegabyte,
                                               0)).Times(1);
    EXPECT_CALL(device_mock_, StopByteCounter()).Times(1);
  }

  // simulate a byte counter update
  uint64 rx_bytes = 155 * kBytesPerMegabyte;
  uint64 tx_bytes = 155 * kBytesPerMegabyte;
  bool result = DataPlan::OnByteCounterUpdate(&data_plans, &service_mock_,
                                              &service_manager_mock_,
                                              &device_mock_, rx_bytes,
                                              tx_bytes);

  // check if test passed
  EXPECT_TRUE(result);
  EXPECT_EQ(plan_one_->GetDataBytesMax(), 100 * kBytesPerMegabyte);
  EXPECT_EQ(plan_one_->GetDataBytesUsed(), 0);
  EXPECT_EQ(plan_one_->GetLocalBytesUsed(), 100 * kBytesPerMegabyte);
  EXPECT_EQ(plan_one_->GetTotalBytesUsed(), 100 * kBytesPerMegabyte);
  EXPECT_FALSE(plan_one_->IsActive());
  EXPECT_EQ(plan_two_->GetDataBytesMax(), 100 * kBytesPerMegabyte);
  EXPECT_EQ(plan_two_->GetDataBytesUsed(), 0);
  EXPECT_EQ(plan_two_->GetLocalBytesUsed(), 100 * kBytesPerMegabyte);
  EXPECT_EQ(plan_two_->GetTotalBytesUsed(), 100 * kBytesPerMegabyte);
  EXPECT_FALSE(plan_two_->IsActive());
  EXPECT_EQ(plan_three_->GetDataBytesMax(), 100 * kBytesPerMegabyte);
  EXPECT_EQ(plan_three_->GetDataBytesUsed(), 0);
  EXPECT_EQ(plan_three_->GetLocalBytesUsed(), 100 * kBytesPerMegabyte);
  EXPECT_EQ(plan_three_->GetTotalBytesUsed(), 100 * kBytesPerMegabyte);
  EXPECT_FALSE(plan_three_->IsActive());
}

}  // namespace cashew
