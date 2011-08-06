// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/aggregator_impl.h"

#include <string>

#include <base/stringprintf.h>  // NOLINT
#include <base/string_number_conversions.h>  // NOLINT

#include <gtest/gtest.h>  // NOLINT

#include "src/date.h"

namespace cashew {

class AggregatorTest: public ::testing::Test {
  protected:
    AggregatorTest() : aggregator_(NULL) {}

    virtual ~AggregatorTest() {
      EXPECT_TRUE(aggregator_ == NULL);
    }

    virtual void SetUp() {
      EXPECT_TRUE(aggregator_ == NULL);
      aggregator_ = new(std::nothrow) AggregatorImpl();
      ASSERT_TRUE(aggregator_ != NULL);
    }

    virtual void TearDown() {
      delete aggregator_;
      aggregator_ = NULL;
    }

    AggregatorImpl *aggregator_;
};

// Tests

// test for |StringFromMap| and |MapFromString|
TEST_F(AggregatorTest, MapStringConversion) {
  std::string table_str;
  Date date1(1970, 1, 1);
  Date date2(1970, 2, 2);
  int bytes1 = 10;
  int bytes2 = 20;
  base::StringAppendF(&table_str, AggregatorImpl::kLineFormat,
             date1.ToString().c_str(), AggregatorImpl::kColumnDelimiter,
             base::IntToString(bytes1).c_str());
  base::StringAppendF(&table_str, AggregatorImpl::kLineFormat,
             date2.ToString().c_str(), AggregatorImpl::kColumnDelimiter,
             base::IntToString(bytes2).c_str());

  BytesPerDay table_map;
  table_map[date1] = bytes1;
  table_map[date2] = bytes2;

  ASSERT_EQ(table_str, aggregator_->StringFromMap(table_map));

  BytesPerDay table_map_from_str;
  EXPECT_TRUE(aggregator_->MapFromString(table_str, &table_map_from_str));
  ASSERT_TRUE(table_map == table_map_from_str);
}

// test for |TrimOldData|
TEST_F(AggregatorTest, TrimOld) {
  base::Time baseline_time = base::Time::Now();
  base::Time trim_1 = baseline_time -
      base::TimeDelta::FromDays(AggregatorImpl::kCleanUpDays);
  base::Time trim_2 = baseline_time -
      base::TimeDelta::FromDays(AggregatorImpl::kCleanUpDays + 1);
  base::Time dont_trim_1 = baseline_time -
      base::TimeDelta::FromDays(AggregatorImpl::kCleanUpDays - 1);
  base::Time dont_trim_2 = baseline_time -
      base::TimeDelta::FromDays(AggregatorImpl::kCleanUpDays - 2);

  Date baseline_date = Date::FromLocalTime(baseline_time);
  Date trim_date_1 = Date::FromLocalTime(trim_1);
  Date trim_date_2 = Date::FromLocalTime(trim_2);
  Date dont_trim_date_1 = Date::FromLocalTime(dont_trim_1);
  Date dont_trim_date_2 = Date::FromLocalTime(dont_trim_2);

  BytesPerDay bytes_per_day;
  bytes_per_day[trim_date_1] = 0;
  bytes_per_day[trim_date_2] = 0;
  bytes_per_day[dont_trim_date_1] = 0;
  bytes_per_day[dont_trim_date_2] = 0;

  aggregator_->TrimOldData(&bytes_per_day, baseline_date);
  ASSERT_TRUE(bytes_per_day.find(trim_date_1) == bytes_per_day.end());
  ASSERT_TRUE(bytes_per_day.find(trim_date_2) == bytes_per_day.end());
  ASSERT_TRUE(bytes_per_day.find(dont_trim_date_1) != bytes_per_day.end());
  ASSERT_TRUE(bytes_per_day.find(dont_trim_date_2) != bytes_per_day.end());
}

}  // namespace cashew
