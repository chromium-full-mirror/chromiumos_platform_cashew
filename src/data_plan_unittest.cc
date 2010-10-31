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
    DataPlanTest() : policy_(NULL) {}

    virtual ~DataPlanTest() {
      EXPECT_TRUE(policy_ == NULL);
    }

    virtual void SetUp() {
      EXPECT_TRUE(policy_ == NULL);
      policy_ = Policy::GetPolicy(kTestCarrierName);
      ASSERT_TRUE(policy_ != NULL);
    }

    virtual void TearDown() {
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

    Policy *policy_;
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

}  // namespace cashew
