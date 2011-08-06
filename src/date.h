// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_DATE_H_
#define SRC_DATE_H_

#include <string>

#include <base/time.h>  //NOLINT

namespace cashew {

class Date {
  public:
    Date();
    Date(int year, int month, int day);
    virtual ~Date();

    // accessor methods
    int GetYear() const {return year_;}
    int GetMonth() const {return month_;}
    int GetDay() const {return day_;}

    // comparison operators
    bool operator==(const Date& other) const;
    bool operator!=(const Date& other) const;
    bool operator<(const Date& other) const;
    bool operator<=(const Date& other) const;
    bool operator>(const Date& other) const;
    bool operator>=(const Date& other) const;

    // TODO(mikemeko): we should have a method that checks if this Date is valid

    // returns the string representation of this Date
    // follows ISO 8601 format
    std::string ToString() const;

    // sets |date| to be the Date representation of |date_str|
    // |date_str| should follow ISO 8601 format
    // returns true on success and false on failure
    static bool FromString(const std::string& date_str, Date* date);

    // returns the base::Time representation of this Date
    base::Time ToLocalTime() const;
    base::Time ToUTCTime() const;

    // returns the Date with year, month, and day fields matching the respective
    //     fields in |time|
    static Date FromLocalTime(const base::Time& time);
    static Date FromUTCTime(const base::Time& time);

    // returns the current Date
    static Date TodayLocal();
    static Date TodayUTC();

  private:
    int year_;
    int month_;
    int day_;

    // field delimiter when printing a Date
    // follows ISO 8601 format
    static const char kDelimiter;

    // returns -1 if |this| <  |other|
    //          0 if |this| == |other|
    //          1 if |this| >  |other|
    int Compare(const Date& other) const;

    // returns the base::Time::Exploded representation of this Date
    // sets all fields other than |year|, |month|, and |day_of_month| to 0
    base::Time::Exploded ToExplodedTime() const;
};  // Date

}  // namespace cashew

#endif  // SRC_DATE_H_
