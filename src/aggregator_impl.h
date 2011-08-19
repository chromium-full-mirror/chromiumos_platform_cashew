// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_AGGREGATOR_IMPL_H_
#define SRC_AGGREGATOR_IMPL_H_

#include <glib.h>

#include <map>
#include <string>

#include <dbus-c++/dbus.h>    // NOLINT

#include <gtest/gtest_prod.h>  // NOLINT

#include "src/aggregator.h"
#include "src/cashew_server.h"
#include "src/data_plan.h"  // for ByteCount

namespace cashew {

class Date;
class Service;

// a type to represent bytes used per day
typedef std::map<Date, ByteCount> BytesPerDay;

class AggregatorImpl : public Aggregator {
  public:
    AggregatorImpl();
    virtual ~AggregatorImpl();

    // should be called right after construction
    // (1) checks that the directory the file lives in exists
    // (2) reads current daily usage data from the file
    // (3) creates a timer that regularly updates the file
    // returns true on success and false on failure
    //     success: (1) and (2) completed successfully
    bool Init();

    // Aggregator methods

    // called when |service| is updated with byte counter info
    virtual void OnByteCounterUpdate(const Service *service,
        uint64 delta_rx_bytes, uint64 delta_tx_bytes);

    // returns a map of date strings to bytes used
    virtual BytesPerDayRep GetBytesPerDay();

  private:

    // have I been successfully initialized via Init()?
    bool initialized_;

    // bytes used per day
    BytesPerDay bytes_per_day_;

    // accumulation of bytes used
    // used to regularly update the file (gets reset on file update)
    ByteCount accumulated_count_;

    // periodic file update timeout
    GSource *update_timeout_source_;

    // a line in the file looks like:
    //     [ISO 8601 formatted Date]|kColumnDelimiter|[bytes used]
    static const char kColumnDelimiter;
    static const char *kLineFormat;

    // all records older than |kCleanUpDays| days are removed
    static const int kCleanUpDays;

    // returns the string representation of |table_map|
    static std::string StringFromMap(const BytesPerDay &table_map);

    // sets |table_map| to be the BytesPerDay representation of |table_str|
    // |table_map| should be empty
    // empty lines in |table_str| are skipped
    // if a date occurs multiple times in |table_str|,
    //     only last well formed occurrence will be recorded in |table_map|
    // returns true on success and false on failure
    //     failure: found a wrongly formatted non-empty line in |table_str|
    static bool MapFromString(const std::string &table_str,
                              BytesPerDay *table_map);

    // returns the std::string to int64_t mapping of |bytes_per_day|
    static BytesPerDayRep GetBytesPerDayRep(BytesPerDay bytes_per_day);

    // gets rid of records in |bytes_per_day| that are at least
    //     |kCleanUpDays| days older than |baseline|
    static void TrimOldData(BytesPerDay *bytes_per_day, Date baseline);

    // reads the file (if it exists) and records data in |bytes_per_day_|
    // trims old data in |bytes_per_day_|
    // returns true on success and false on failure
    bool GetCurrentData();

    // if |bytes_per_day_| has changed, trims it then writes it to the file
    // returns true on success and false on failure
    bool UpdateFile();

    // static wrapper for UpdateFile
    // casts to AggregatorImpl* and invokes UpdateFile() on object ptr |data|
    static gboolean StaticUpdateFile(gpointer data);

    // creates and starts a timer that fires periodically to update the file
    // returns true on success and false on failure
    bool CreateUpdateTimer();

    // stops and destroys the update timer if it exists
    void DestroyUpdateTimer();

    // for unit testing
    friend class AggregatorTest;
    FRIEND_TEST(AggregatorTest, MapStringConversion);
    FRIEND_TEST(AggregatorTest, GetBytesPerDayRep);
    FRIEND_TEST(AggregatorTest, TrimOld);
};  // AggregatorImpl

}  // namespace cashew

#endif  // SRC_AGGREGATOR_IMPL_H_
