// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/date.h"

#include <string>

#include <base/time.h>  // NOLINT

#include <gtest/gtest.h>  // NOLINT

namespace cashew {

class DateTest: public ::testing::Test {
  protected:
    DateTest() : base_year(1991), base_month(10), base_day(8),
                 date_(base_year, base_month, base_day),
                 date_Y_(base_year + 1, base_month, base_day),
                 date_y_(base_year - 1, base_month, base_day),
                 date_M_(base_year, base_month + 1, base_day),
                 date_m_(base_year, base_month - 1, base_day),
                 date_D_(base_year, base_month, base_day + 1),
                 date_d_(base_year, base_month, base_day - 1),
                 date_EQ_(base_year, base_month, base_day) {}

    virtual ~DateTest() {}

    virtual void SetUp() {}
    virtual void TearDown() {}

    int base_year;   // 1991
    int base_month;  // 10
    int base_day;    // 08
    Date date_;
    Date date_Y_;
    Date date_y_;
    Date date_M_;
    Date date_m_;
    Date date_D_;
    Date date_d_;
    Date date_EQ_;
};

// Tests

// test for |operator==|
TEST_F(DateTest, OperatorEQ) {
  ASSERT_FALSE(date_ == date_Y_);
  ASSERT_FALSE(date_ == date_y_);
  ASSERT_FALSE(date_ == date_M_);
  ASSERT_FALSE(date_ == date_m_);
  ASSERT_FALSE(date_ == date_D_);
  ASSERT_FALSE(date_ == date_d_);
  ASSERT_TRUE(date_ == date_EQ_);
}

// test for |operator!=|
TEST_F(DateTest, OperatorNE) {
  ASSERT_TRUE(date_ != date_Y_);
  ASSERT_TRUE(date_ != date_y_);
  ASSERT_TRUE(date_ != date_M_);
  ASSERT_TRUE(date_ != date_m_);
  ASSERT_TRUE(date_ != date_D_);
  ASSERT_TRUE(date_ != date_d_);
  ASSERT_FALSE(date_ != date_EQ_);
}

// test for |operator<|
TEST_F(DateTest, OperatorLT) {
  ASSERT_TRUE(date_ < date_Y_);
  ASSERT_FALSE(date_ < date_y_);
  ASSERT_TRUE(date_ < date_M_);
  ASSERT_FALSE(date_ < date_m_);
  ASSERT_TRUE(date_ < date_D_);
  ASSERT_FALSE(date_ < date_d_);
  ASSERT_FALSE(date_ < date_EQ_);
}

// test for |operator<=|
TEST_F(DateTest, OperatorLE) {
  ASSERT_TRUE(date_ <= date_Y_);
  ASSERT_FALSE(date_ <= date_y_);
  ASSERT_TRUE(date_ <= date_M_);
  ASSERT_FALSE(date_ <= date_m_);
  ASSERT_TRUE(date_ <= date_D_);
  ASSERT_FALSE(date_ <= date_d_);
  ASSERT_TRUE(date_ <= date_EQ_);
}

// test for |operator>|
TEST_F(DateTest, OperatorGT) {
  ASSERT_FALSE(date_ > date_Y_);
  ASSERT_TRUE(date_ > date_y_);
  ASSERT_FALSE(date_ > date_M_);
  ASSERT_TRUE(date_ > date_m_);
  ASSERT_FALSE(date_ > date_D_);
  ASSERT_TRUE(date_ > date_d_);
  ASSERT_FALSE(date_ > date_EQ_);
}

// test for |operator>=|
TEST_F(DateTest, OperatorGE) {
  ASSERT_FALSE(date_ >= date_Y_);
  ASSERT_TRUE(date_ >= date_y_);
  ASSERT_FALSE(date_ >= date_M_);
  ASSERT_TRUE(date_ >= date_m_);
  ASSERT_FALSE(date_ >= date_D_);
  ASSERT_TRUE(date_ >= date_d_);
  ASSERT_TRUE(date_ >= date_EQ_);
}

// test for |ToString| and |FromString|
TEST_F(DateTest, DateStringConversion) {
  std::string date_str = "1991-10-08";

  ASSERT_EQ(date_str, date_.ToString());

  Date date_from_str;
  EXPECT_TRUE(date_.FromString(date_str, &date_from_str));
  ASSERT_TRUE(date_ == date_from_str);
}

// test for |To{Local,UTC}Time| and |From{Local,UTC}Time|
TEST_F(DateTest, DateTimeConversion) {
  base::Time::Exploded exploded_time = {date_.GetYear(), date_.GetMonth(), 0,
                                        date_.GetDay(), 0, 0, 0, 0};

  // local time
  base::Time time = base::Time::FromLocalExploded(exploded_time);
  ASSERT_TRUE(date_ == Date::FromLocalTime(time));
  ASSERT_TRUE(time == date_.ToLocalTime());

  // UTC time
  time = base::Time::FromUTCExploded(exploded_time);
  ASSERT_TRUE(date_ == Date::FromUTCTime(time));
  ASSERT_TRUE(time == date_.ToUTCTime());
}

}  // namespace cashew
