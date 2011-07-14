// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/user_aggregator_impl.h"

#include <unistd.h>

#include <base/stringprintf.h>

#include <gtest/gtest.h>

namespace cashew {

class UserAggregatorTest: public ::testing::Test {
  protected:
    UserAggregatorTest() : aggregator_(NULL) {}

    virtual ~UserAggregatorTest() {
      EXPECT_TRUE(aggregator_ == NULL);
    }

    virtual void SetUp() {
      EXPECT_TRUE(aggregator_ == NULL);
      aggregator_ = new(std::nothrow) UserAggregatorImpl();
      ASSERT_TRUE(aggregator_ != NULL);
    }

    virtual void TearDown() {
      delete aggregator_;
      aggregator_ = NULL;
    }

    UserAggregatorImpl *aggregator_;
    static const char delimiter_;
};

const char UserAggregatorTest::delimiter_ = '\t';

// Tests

// tests for |StringFromMap| and |MapFromString|:
// empty table
TEST_F(UserAggregatorTest, EmptyFileMapStringConversion) {
  std::string empty_str;
  BytesPerUser empty_map;

  std::string table_str_from_map;
  aggregator_->StringFromMap(empty_map,
                             &table_str_from_map, delimiter_);
  ASSERT_TRUE(table_str_from_map.empty());

  BytesPerUser table_map_from_str;
  EXPECT_TRUE(aggregator_->MapFromString(empty_str,
                            &table_map_from_str, delimiter_));
  ASSERT_TRUE(table_map_from_str.empty());
}
// well formed table
TEST_F(UserAggregatorTest, WellFormedMapStringConversion) {
  std::string table_str;
  base::StringAppendF(&table_str, "%s%c%s",
                      "Anne", delimiter_, "10\n");
  base::StringAppendF(&table_str, "%s%c%s",
                      "David Rochberg", delimiter_, "200\n");
  base::StringAppendF(&table_str, "%s%c%s",
                      "Elly\tJones", delimiter_, "3000\n");
  base::StringAppendF(&table_str, "%s%c%s",
                      "mike22\t", delimiter_, "40000\n");
  base::StringAppendF(&table_str, "%s%c%s",
                      "vlaviano@google.com", delimiter_, "500000\n");

  BytesPerUser table_map;
  table_map["Anne"] = 10;
  table_map["David Rochberg"] = 200;
  table_map["Elly\tJones"] = 3000;
  table_map["mike22\t"] = 40000;
  table_map["vlaviano@google.com"] = 500000;

  std::string table_str_from_map;
  aggregator_->StringFromMap(table_map,
                             &table_str_from_map, delimiter_);
  ASSERT_TRUE(table_str == table_str_from_map);

  BytesPerUser table_map_from_str;
  EXPECT_TRUE(aggregator_->MapFromString(table_str,
                            &table_map_from_str, delimiter_));
  ASSERT_TRUE(table_map == table_map_from_str);
}
// TODO(mikemeko): multiple empty lines (should be ok)
TEST_F(UserAggregatorTest, EmptyLinesMapStringConversion) {

}
// TODO(mikemeko): no |delimiter_| in a line
TEST_F(UserAggregatorTest, NoDelimiterMapStringConversion) {

}
// TODO(mikemeko): invalid bytes entry for a user
TEST_F(UserAggregatorTest, InvalidBytesEntryMapStringConversion) {

}
// TODO(mikemeko): multiple occurrences of same user
TEST_F(UserAggregatorTest, MultipleOccurenceMapStringConversion) {

}

// test for |Exists|, |ReadFile|, and |WriteToFile|:
TEST_F(UserAggregatorTest, ReadWrite) {
  const char *dir_name = "UserAggregatorTestFolder";
  const char *file_name = "UserAggregatorTestFolder/usage.txt";
  const char *temp_file_name = "UserAggregatorTestFolder/usage_temp.txt";

  std::string content = "content";

  // check that directory is not present
  struct stat st;
  EXPECT_NE(0, stat(dir_name, &st));
  EXPECT_TRUE(errno == ENOENT);
  ASSERT_FALSE(aggregator_->Exists(dir_name));
  // make the directory
  EXPECT_EQ(0, mkdir(dir_name, S_IRWXU | S_IRWXG | S_IRWXO));
  // check that the directory is present
  EXPECT_EQ(0, stat(dir_name, &st));
  ASSERT_TRUE(aggregator_->Exists(dir_name));

  // write |content| to the file, read from the file,
  //   and check that we get back what we expect
  EXPECT_FALSE(aggregator_->Exists(file_name));
  EXPECT_FALSE(aggregator_->Exists(temp_file_name));
  EXPECT_TRUE(aggregator_->WriteToFile(content, file_name, temp_file_name));
  EXPECT_FALSE(aggregator_->Exists(temp_file_name));
  ASSERT_TRUE(aggregator_->Exists(file_name));
  std::string file_content;
  EXPECT_TRUE(aggregator_->ReadFile(file_name, &file_content));
  ASSERT_TRUE(content == file_content);

  // remove the file and directory
  EXPECT_EQ(0, remove(file_name));
  EXPECT_EQ(0, rmdir(dir_name));
  EXPECT_FALSE(aggregator_->Exists(file_name));
  EXPECT_FALSE(aggregator_->Exists(dir_name));
}

}  // namespace cashew
