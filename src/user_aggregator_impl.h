// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_USER_AGGREGATOR_IMPL_H_
#define SRC_USER_AGGREGATOR_IMPL_H_

#include <glib.h>
#include <gtest/gtest_prod.h>

#include <string>

#include "src/user_aggregator.h"

namespace cashew {

class UserAggregatorImpl : public UserAggregator,
                           public DataPlanDelegate {
  public:
    UserAggregatorImpl();
    virtual ~UserAggregatorImpl();

    // should be called right after construction
    // 1) sets |current_user_| to the username of the current user
    // 2) sets |bytes_per_user_| to current usage data (from the file)
    // returns true on success and false on failure
    bool Init();

    // UserAggregator methods

    // sets this aggregator as the DataPlanDelegate for |data_plan|
    virtual void OnNewDataPlan(DataPlan *data_plan);

    // DataPlanDelegate methods

    // called when |data_plan|, to which this is a delegate, is updated
    virtual void OnByteCounterUpdate(DataPlan *data_plan,
                                     ByteCount delta_bytes);

  private:

    // have I been successfully initialized (via Init())?
    bool initialized_;

    // username of the current user
    // |UserAggregatorImpl::kGuestUsername| represents guest
    // empty string represents unknown user
    Username current_user_;

    // bytes used by each user of the machine
    BytesPerUser bytes_per_user_;

    // number of bytes used by |current_user_| since last write to file
    ByteCount bytes_used_since_last_write_;

    // periodic file update timeout
    GSource *update_timeout_source_;

    // sets |table_str| to be the string representation of |table_map|
    // columns are separated by the last |column_delimiter|
    void StringFromMap(const BytesPerUser &table_map, std::string *table_str,
                       const char column_delimiter);

    // sets |table_map| to be the BytesPerUser representation of |table_str|
    // columns in |table_str| are separated by the last |column_delimiter|
    // empty lines are skipped
    // if a user occurs multiple times, only first occurrence will be recorded
    // |table_map| should be empty
    // returns true on success and false on failure
    //   success: recorded a user-bytes pair from every non-empty line
    bool MapFromString(const std::string &table_str, BytesPerUser *table_map,
                       const char column_delimiter);

    // returns true if the file/directory whose path is |path| exists,
    //   false otherwise
    bool Exists(const char *path);

    // reads the file with path |file_name| and records it in |content|
    // returns true on success and false on failure
    bool ReadFile(const char *file_name, std::string *content);

    // writes |content| into the file with path |file_name|
    // guarantees to leave the file in one of two states:
    //   1) on success: file contains |content|
    //   2) on failure: file is left exactly as it was before method call
    // |temp_file_name| is the name of the temporary file we write to first,
    //   which then gets renamed to |file_name|
    // returns true on success and false on failure
    bool WriteToFile(const std::string& content,
                     const char *file_name, const char *temp_file_name);

    // gets the username of the current user by making a request on DBus
    // returns true on success and false on failure
    bool IdentifyCurrentUser(std::string* username);

    // user just logged out
    void OnUserLogout();

    // user just logged in
    void OnUserLogin(const Username& username);

    // checks if we have ever seen |current_user_|,
    //   and if not adds him/her to the list of known users
    void RecordCurrentUser();

    // reads the file and records the data in |bytes_per_user_|
    // if the file does not exist (first run), creates the file
    // returns true on success and false on failure
    bool GetCurrentUsageData();

    // updates then writes |bytes_per_user_| to the usage file
    // returns true on success and false on failure
    bool UpdateFile();

    // static wrapper for UpdateFile
    // casts to UserAggregatorImpl* and invokes UpdateFile on object ptr |data|
    static gboolean StaticUpdateFile(gpointer data);

    // creates and starts a timer that fires periodically to update the file
    // returns true of success and false on failure
    bool CreateUpdateTimer();

    // stops and destroys the update timer if it exists
    void DestroyUpdateTimer();

    // for unittesting
    friend class UserAggregatorTest;
    FRIEND_TEST(UserAggregatorTest, EmptyFileMapStringConversion);
    FRIEND_TEST(UserAggregatorTest, WellFormedMapStringConversion);
    FRIEND_TEST(UserAggregatorTest, EmptyLinesMapStringConversion);
    FRIEND_TEST(UserAggregatorTest, NoDelimiterMapStringConversion);
    FRIEND_TEST(UserAggregatorTest, InvalidBytesEntryMapStringConversion);
    FRIEND_TEST(UserAggregatorTest, MultipleOccurenceMapStringConversion);
    FRIEND_TEST(UserAggregatorTest, ReadWrite);
};  // UserAggregatorImpl

}  // namespace cashew

#endif  // SRC_USER_AGGREGATOR_IMPL_H_
