// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/date.h"

#include <vector>

#include <base/string_number_conversions.h>  // NOLINT
#include <base/string_split.h>  // NOLINT
#include <base/stringprintf.h>  // NOLINT

#include <glog/logging.h>  // NOLINT

namespace cashew {

static const int kInvalidValue = -1;

// static
const char Date::kDelimiter = '-';

Date::Date()
    : year_(kInvalidValue), month_(kInvalidValue), day_(kInvalidValue) {}

Date::Date(int year, int month, int day)
    : year_(year), month_(month), day_(day) {}

Date::~Date() {}

// public methods

bool Date::operator==(const Date& other) const {
  return Compare(other) == 0;
}

bool Date::operator!=(const Date& other) const {
  return Compare(other) != 0;
}

bool Date::operator<(const Date& other) const {
  return Compare(other) < 0;
}

bool Date::operator<=(const Date& other) const {
  return Compare(other) <= 0;
}

bool Date::operator>(const Date& other) const {
  return Compare(other) > 0;
}

bool Date::operator>=(const Date& other) const {
  return Compare(other) >= 0;
}

std::string Date::ToString() const {
  std::string date_str;
  base::StringAppendF(&date_str, "%d%c%02d%c%02d", year_, kDelimiter,
                      month_, kDelimiter, day_);
  return date_str;
}

// static
bool Date::FromString(const std::string& date_str, Date *date) {
  CHECK(date != NULL);

  // split up |date_str| into year, month, day
  std::vector<std::string> year_month_day;
  base::SplitString(date_str, kDelimiter, &year_month_day);

  // make sure the 3 fields are given
  if (year_month_day.size() != 3) {
    LOG(WARNING) << "FromString: \"" << date_str << "\" has wrong format";
    return false;
  }

  int year;
  if (!base::StringToInt(year_month_day[0], &year)) {
    LOG(WARNING) << "FromString: failed to convert year field to int";
    return false;
  }

  int month;
  if (!base::StringToInt(year_month_day[1], &month)) {
    LOG(WARNING) << "FromString: failed to convert month field to int";
    return false;
  }

  int day;
  if (!base::StringToInt(year_month_day[2], &day)) {
    LOG(WARNING) << "FromString: failed to convert day field to int";
    return false;
  }

  date->year_ = year;
  date->month_ = month;
  date->day_ = day;

  return true;
}

base::Time Date::ToLocalTime() const {
  return base::Time::FromLocalExploded(ToExplodedTime());
}

base::Time Date::ToUTCTime() const {
  return base::Time::FromUTCExploded(ToExplodedTime());
}

// static
Date Date::FromLocalTime(const base::Time& time) {
  base::Time::Exploded exploded_time;
  time.LocalExplode(&exploded_time);
  return Date(exploded_time.year, exploded_time.month,
              exploded_time.day_of_month);
}

// static
Date Date::FromUTCTime(const base::Time& time) {
  base::Time::Exploded exploded_time;
  time.UTCExplode(&exploded_time);
  return Date(exploded_time.year, exploded_time.month,
              exploded_time.day_of_month);
}

// static
Date Date::TodayLocal() {
  return FromLocalTime(base::Time::Now());
}

// static
Date Date::TodayUTC() {
  return FromUTCTime(base::Time::Now());
}

// private methods

int Date::Compare(const Date& other) const {
  if (year_ < other.GetYear()) {
    return -1;
  }
  if (year_ > other.GetYear()) {
    return 1;
  }
  if (month_ < other.GetMonth()) {
    return -1;
  }
  if (month_ > other.GetMonth()) {
    return 1;
  }
  if (day_ < other.GetDay()) {
    return -1;
  }
  if (day_ > other.GetDay()) {
    return 1;
  }
  return 0;
}

base::Time::Exploded Date::ToExplodedTime() const {
  base::Time::Exploded date_exploded = {year_, month_, 0, day_, 0, 0, 0, 0};
  return date_exploded;
}

}  // namespace cashew
