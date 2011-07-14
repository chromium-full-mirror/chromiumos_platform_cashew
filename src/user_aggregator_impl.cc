// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "src/user_aggregator_impl.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <base/string_number_conversions.h>
#include <base/string_split.h>
#include <base/stringprintf.h>

#include <glog/logging.h>

#include <vector>

namespace cashew {

// path of the directory in which we write the file
static const char *kDirName = "/var/db/cashew";
// path of the file we write to
static const char *kFileName = "/var/db/cashew/usage.txt";
// path of the temporary file (for atomic writing)
static const char *kTempFileName = "/var/db/cashew/usage_temp.txt";

// column separater
static const char kColumnDelimiter = '\t';

// maximum size for a username or bytes entry in the file
static const int kMaxEntrySize = 30;  // maximum username length

// unique username for a guest on the machine
static const Username kGuestUsername = "$guest$";

// update the file every |kFileUpdateIntervalBytes| bytes
static const ByteCount kFileUpdateIntervalBytes = 1*1024*1024;  // 1 MB
// update the file every |kFileUpdateIntervalSeconds| seconds
static const guint kFileUpdateIntervalSeconds = 600;  // 10 mins
// TODO(mikemeko): set reasonable values for these constants

UserAggregatorImpl::UserAggregatorImpl()
    : initialized_(false), bytes_used_since_last_write_(0),
      update_timeout_source_(NULL) {}

UserAggregatorImpl::~UserAggregatorImpl() {
  // make sure to record most recent usage
  if (initialized_) {
    UpdateFile();
  }
  // destroy the update timer
  DestroyUpdateTimer();
}

// public methods

// this should be called right after construction
bool UserAggregatorImpl::Init() {
  // identify the current user and record username in |current_user_|
  if (!IdentifyCurrentUser(&current_user_)) {
    LOG(WARNING) << "Init: failed to identify current user";
  }

  // check to see that the directory in which the file lives exists
  if (!Exists(kDirName)) {
    LOG(WARNING) << "Init: directory " << kDirName << " doesn't exist";
    return false;
  }

  // read the file, and record the current usage data in |bytes_per_user_|
  if (!GetCurrentUsageData()) {
    LOG(WARNING) << "Init: failed to record data from file";
    return false;
  }

  // make sure we recognize |current_user_| as a user on this machine
  RecordCurrentUser();

  // create the update timer
  if (!CreateUpdateTimer()) {
    LOG(WARNING) << "Init: failed to create update timer";
  }

  initialized_ = true;
  return true;
}

// UserAggregator methods

void UserAggregatorImpl::OnNewDataPlan(DataPlan *data_plan) {
  CHECK(initialized_);
  CHECK(data_plan != NULL);
  data_plan->SetDelegate(this);
}

// DataPlanDelegate methods

// TODO(mikemeko): |data_plan| is not used right now, but might be useful later
void UserAggregatorImpl::OnByteCounterUpdate(DataPlan *data_plan,
                                             ByteCount delta_bytes) {
  CHECK(initialized_);
  CHECK(data_plan != NULL);
  CHECK_GE(delta_bytes, 0);

  if (delta_bytes > 0) {
    bytes_used_since_last_write_ += delta_bytes;

    if (bytes_used_since_last_write_ < 0) {
      LOG(WARNING) << "OnByteCounterUpdate: overflow detected";
      bytes_used_since_last_write_ = kint64max;
    }

    // regularly update the file
    if (bytes_used_since_last_write_ > kFileUpdateIntervalBytes) {
      if (!UpdateFile()) {
        LOG(WARNING) << "OnByteCounterUpdate: failed to update file";
      }
    }
  }
}

// private methods

void UserAggregatorImpl::StringFromMap(const BytesPerUser &table_map,
                          std::string *table_str, const char column_delimiter) {
  CHECK(table_str != NULL);

  BytesPerUser::const_iterator it;
  for (it = table_map.begin(); it != table_map.end(); ++it) {
    const std::string& user = it->first;
    const std::string& bytes = base::Int64ToString(it->second);
    base::StringAppendF(table_str, "%s%c%s\n",
                        user.c_str(), column_delimiter, bytes.c_str());
  }
}

bool UserAggregatorImpl::MapFromString(const std::string &table_str,
                         BytesPerUser* table_map, const char column_delimiter) {
  CHECK(table_map != NULL);
  CHECK(table_map->empty());

  // split up |table_str| into lines
  std::vector<std::string> *lines =
                            new(std::nothrow) std::vector<std::string>();
  if (lines == NULL) {
    LOG(WARNING) << "MapFromString: failed to initialize list";
    return false;
  }
  base::SplitStringDontTrim(table_str, '\n', lines);

  // number of successfuly parsed non-empty lines (added to |table_map|)
  uint parsed_lines = 0;
  // number of unsuccessfully parsed non-empty lines (not added to |table_map|)
  uint unparsed_lines = 0;

  // construct |table_map| from |table_str| line by line
  std::vector<std::string>::const_iterator it;
  for (it = lines->begin(); it != lines->end(); ++it) {
    // skip empty lines (empty lines are considered neither parsed nor unparsed)
    if (it->empty()) {
      continue;
    }

    uint line_number = it - lines->begin() + 1;

    // USERNAME\tBYTES\0
    // ^       ^      ^
    const char *line = it->c_str();
    const char *last_delimiter = strrchr(line, column_delimiter);
    if (last_delimiter == NULL) {
      LOG(WARNING) << "MapFromString: delimiter not found in line: "
        << line_number;
      ++unparsed_lines;
      continue;  // failure, go to next line
    }
    const char *end = strchr(line, '\0');
    if (end == NULL) {
      LOG(WARNING) << "MapFromString: null terminator not found in line: "
        << line_number;
      ++unparsed_lines;
      continue;  // failure, go to next line
    }

    size_t username_size = last_delimiter - line;
    char username[kMaxEntrySize];
    if (strncpy(username, line, username_size) == NULL) {
      LOG(WARNING) << "MapFromString: failed to retrieve username in line: "
        << line_number;
      ++unparsed_lines;
      continue;  // failure, go to next line
    }
    username[username_size] = '\0';

    size_t bytes_size = end - last_delimiter - 1;
    char bytes[kMaxEntrySize];
    if (strncpy(bytes, last_delimiter + 1, bytes_size) == NULL) {
      LOG(WARNING) << "MapFromString: failed to retrieve bytes for user: \""
        << username << "\" in line: " << line_number;
      ++unparsed_lines;
      continue;  // failure, go to next line
    }
    bytes[bytes_size] = '\0';

    // check that |username| has not already been recorded in |table_map|
    BytesPerUser::const_iterator find_user = table_map->find(username);
    if (find_user != table_map->end()) {
      // |username| has already been recorded in |table_map|
      LOG(WARNING) << "MapFromString: user \"" << username
        << "\" recorded more than once";
      ++unparsed_lines;
      continue;  // failure, go to next line
    }

    // record |username|-|bytes| pair in |table_map|
    if (!base::StringToInt64(bytes, &((*table_map)[username]))) {
      LOG(WARNING) << "MapFromString: failed to store bytes for user: \""
        << username << "\" in line: " << line_number;
      ++unparsed_lines;
      continue;  // failure, go to next line
    }

    // successfully parsed this line
    ++parsed_lines;
  }
  delete lines;
  LOG(INFO) << "MapFromString: successfully parsed " << parsed_lines
    << (parsed_lines == 1 ? " line" : " lines");
  if (unparsed_lines > 0) {
    LOG(WARNING) << "MapFromString: failed to parse " << unparsed_lines
    << (unparsed_lines == 1 ? " line" : " lines");
  }
  return unparsed_lines == 0;
}

bool UserAggregatorImpl::Exists(const char *path) {
  CHECK(path != NULL);
  struct stat st;
  if (stat(path, &st) != 0) {
    if (errno != ENOENT) {
      // stat failed
      PLOG(ERROR) << "stat failed for " << path;
      return false;
    }
    // file/directory doesn't exist
    return false;
  }
  // file/directory exists
  return true;
}

bool UserAggregatorImpl::ReadFile(const char *file_name, std::string *content) {
  CHECK(file_name != NULL);
  CHECK(content != NULL);

  FILE *read_file;
  read_file = fopen(file_name, "r");

  if (read_file == NULL) {
    // file does not exist
    LOG(INFO) << "ReadFile: failed to open file: " << file_name;
    return false;
  }

  char *line = NULL;
  size_t len = 0;
  ssize_t read;
  while ((read = getline(&line, &len, read_file)) != -1) {
    base::StringAppendF(content, "%s", line);
  }
  if (line == NULL) {
    free(line);
  }

  if (fclose(read_file) != 0) {
    PLOG(WARNING) << "ReadFile: failed to close file: " << file_name;
    return false;
  }

  return true;
}

bool UserAggregatorImpl::WriteToFile(const std::string& content,
                            const char *file_name, const char *temp_file_name) {
  CHECK(file_name != NULL);

  size_t nmemb = content.size();
  const char* content_c = content.c_str();
  FILE *write_file;

  // first, write the data to a temporary file
  write_file = fopen(temp_file_name, "w");
  if (write_file == NULL) {
    PLOG(WARNING) << "WriteToFile: failed to create temporary file: "
      << temp_file_name;
    return false;
  }
  char c;  // for sizeof
  size_t written_to_temp = fwrite(content_c, sizeof(c), nmemb, write_file);
  if (written_to_temp != nmemb) {
    PLOG(WARNING) << "WriteToFile: failed to write content to temporary file: "
      << temp_file_name;
    return false;
  }
  if (fclose(write_file) != 0) {
    PLOG(WARNING) << "WriteToFile: failed to close temporary file: "
      << temp_file_name;
    return false;
  }

  // then, rename the temporary file
  if (rename(temp_file_name, file_name) != 0) {
    PLOG(WARNING) << "WriteToFile: failed to rename temporary file ["
      << temp_file_name << "] to main file [" << file_name << "]";
    return false;
  }

  return true;
}

// identifies the username of the current user by making a request over DBus
// if no username returned by session manager, default to guest
bool UserAggregatorImpl::IdentifyCurrentUser(Username *username) {
  CHECK(username != NULL);

  // TODO(annemkenyon, mikemeko):
  //   get username by making a request over DBus to the session manager
  *username = kGuestUsername;  // default to guest
  return true;
}

// user just logged out
// TODO(annemkenyon, mikemeko):
//   we need to listen for a DBus signal that tells us when user logs out
void UserAggregatorImpl::OnUserLogout() {
  CHECK(initialized_);

  if (current_user_.empty()) {
    LOG(WARNING) << "OnUserLogout: no user logged in";
    return;
  }
  if (!UpdateFile()) {
    LOG(WARNING) << "OnUserLogout: failed to update file";
  }
  // reset (default to guest)
  current_user_ = kGuestUsername;
}

// user just logged in
// TODO(annemkenyon, mikemeko):
//   we need to listen for a DBus signal that tells us when user logs in
void UserAggregatorImpl::OnUserLogin(const Username& username) {
  CHECK(initialized_);

  if (!current_user_.empty()) {
    LOG(WARNING) << "OnUserLogin: a user is already logged in";
  }

  // update the current user anyways
  current_user_ = username;

  // make sure we recognize |username| as a user on this machine
  RecordCurrentUser();
}

// TODO(mikemeko): if a user deletes his/her account on a ChromeOS machine,
//   user aggregator never finds out about this and that user lives in the
//   usage file forever. Listen for such a signal?
void UserAggregatorImpl::RecordCurrentUser() {
  if (!current_user_.empty()) {
    BytesPerUser::const_iterator it;
    it = bytes_per_user_.find(current_user_);
    if (it == bytes_per_user_.end()) {
      // |current_user_| has never been recorded
      bytes_per_user_[current_user_] = 0;
    }
  }
}

// reads the file and records the data in |bytes_per_user_|
bool UserAggregatorImpl::GetCurrentUsageData() {
  if (!Exists(kFileName)) {
    // file does not exist, so create it
    FILE *new_file = fopen(kFileName, "w");
    if (new_file == NULL) {
      LOG(WARNING) << "GetCurrentUsageData: failed to create file: "
        << kFileName;
      return false;
    }
    if (fclose(new_file) != 0) {
      PLOG(WARNING) << "GetCurrentUsageData: failed to close file: "
        << kFileName;
      return false;
    }
    // leave |bytes_per_user_| empty
    return true;
  }
  std::string file_content;
  if (!ReadFile(kFileName, &file_content)) {
    LOG(WARNING) << "GetCurrentUsageData: failed to read file: " << kFileName;
    return false;
  }
  if (!MapFromString(file_content, &bytes_per_user_, kColumnDelimiter)) {
    LOG(WARNING) << "GetCurrentUsageData: failed to parse content";
    return false;
  }
  return true;
}

// updates the files so that |current_user_| is reported
//   to have used |bytes_used_since_last_write_| more bytes
bool UserAggregatorImpl::UpdateFile() {
  CHECK(initialized_);
  CHECK(!current_user_.empty());
  CHECK_GE(bytes_used_since_last_write_, 0);

  if (bytes_used_since_last_write_ > 0) {
    // record new data
    bytes_per_user_[current_user_] += bytes_used_since_last_write_;

    // reset
    bytes_used_since_last_write_ = 0;

    // get the content to write to file
    std::string content_to_write;
    StringFromMap(bytes_per_user_, &content_to_write, kColumnDelimiter);

    // write to file
    if (!WriteToFile(content_to_write, kFileName, kTempFileName)) {
      LOG(WARNING) << "UpdateFile: failed to write to file";
      return false;
    }
  }
  return true;
}

// static
gboolean UserAggregatorImpl::StaticUpdateFile(gpointer data) {
  UserAggregatorImpl *aggregator = reinterpret_cast<UserAggregatorImpl*>(data);
  CHECK_NOTNULL(aggregator);

  if (!aggregator->UpdateFile()) {
    LOG(WARNING) << "StaticUpdateFile: failed to update file";
  }

  return TRUE;  // call back
}

bool UserAggregatorImpl::CreateUpdateTimer() {
  if (update_timeout_source_ != NULL) {
    LOG(WARNING) << "CreateUpdateTimer: timer already running";
    return false;
  }
  update_timeout_source_ = g_timeout_source_new_seconds(
                                                    kFileUpdateIntervalSeconds);
  if (update_timeout_source_ == NULL) {
    LOG(WARNING) << "CreateUpdateTimer: failed to create timeout source";
    return false;
  }
  g_source_set_callback(update_timeout_source_, StaticUpdateFile, this, NULL);
  if (g_source_attach(update_timeout_source_, NULL) == 0) {
    LOG(WARNING) << "CreateUpdateTimer: could not attach timeout source";
    return false;
  }
  return true;
}

void UserAggregatorImpl::DestroyUpdateTimer() {
  if (update_timeout_source_ != NULL) {
    g_source_destroy(update_timeout_source_);
    update_timeout_source_ = NULL;
  }
}

}  // namespace cashew
