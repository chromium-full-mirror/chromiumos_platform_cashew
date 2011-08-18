// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/aggregator_impl.h"

#include <vector>

#include <base/file_path.h>  // NOLINT
#include <base/file_util.h>  // NOLINT
#include <base/logging.h>  // NOLINT
#include <base/string_number_conversions.h>  // NOLINT
#include <base/string_split.h>  // NOLINT
#include <base/stringprintf.h>  // NOLINT

#include "src/date.h"
#include "src/service.h"

// TODO(mikemeko): #including <base/file_util.h> makes us #include
//     <base/logging.h> as well and we would rather use <glog/logging.h>

namespace cashew {

// a type to store split up pieces of a string
typedef std::vector<std::string> StringParts;

// file storage location info
static const FilePath kDirPath(FILE_PATH_LITERAL("/var/db/cashew"));
static const FilePath kFilePath = kDirPath.Append("daily_usage.txt");

// static
const char AggregatorImpl::kColumnDelimiter = '\t';
const char *AggregatorImpl::kLineFormat = "%s%c%s\n";

// static
const int AggregatorImpl::kCleanUpDays = 367;  // number of days in a year + 1

// file update intervals
static const ByteCount kBytesPerMegaByte = 1024 * 1024;
static const ByteCount kFileUpdateIntervalBytes = 1 * kBytesPerMegaByte;
static const guint kSecondsPerMinute = 60;
static const guint kFileUpdateIntervalSeconds = 10 * kSecondsPerMinute;

AggregatorImpl::AggregatorImpl()
    : initialized_(false), accumulated_count_(0),
      update_timeout_source_(NULL) {}

AggregatorImpl::~AggregatorImpl() {
  if (initialized_) {
    // make sure to record most recent usage
    if (!UpdateFile()) {
      LOG(WARNING) << "dtor: failed to update file";
    }
    // destroy the update timer
    DestroyUpdateTimer();
  }
}

// public methods

bool AggregatorImpl::Init() {
  // (1) check that the directory the file lives in exists
  if (!file_util::DirectoryExists(kDirPath)) {
    LOG(WARNING) << "Init: storage directory \"" << kDirPath.value()
        << "\" doesn't exist";
    return false;
  }

  // (2) read the current daily usage data
  if (!GetCurrentData()) {
    LOG(WARNING) << "Init: failed to read current data";
    return false;
  }

  // (3) create the update timer
  if (!CreateUpdateTimer()) {
    LOG(WARNING) << "Init: failed to create update timer";
    // rely on byte-accumulation update method
  }

  initialized_ = true;
  return true;
}

// Aggregator methods

// TODO(mikemeko): change this so that internal rep is in UTC (stored at finer
//     granularity) and convert to local time when displaying
void AggregatorImpl::OnByteCounterUpdate(const Service *service,
    uint64 delta_rx_bytes, uint64 delta_tx_bytes) {
  CHECK(initialized_);
  CHECK(service != NULL);

  LOG(INFO) << "OnByteCounterUpdate: received byte counter info from service: "
      << service->GetPath() << ": delta_rx_bytes = " << delta_rx_bytes
      << ", delta_tx_bytes = " << delta_tx_bytes;

  ByteCount delta_bytes_used = delta_rx_bytes + delta_tx_bytes;
  // try to detect overflow
  if (delta_bytes_used < 0 ||
      static_cast<uint64>(delta_bytes_used) < delta_rx_bytes ||
      static_cast<uint64>(delta_bytes_used) < delta_tx_bytes) {
    LOG(WARNING) << "OnByteCounterUpdate: overflow detected:"
        " |delta_bytes_used|";
    delta_bytes_used = kint64max;
  }

  // record new usage info
  Date today = Date::TodayLocal();
  ByteCount bytes_today;
  if (bytes_per_day_.find(today) == bytes_per_day_.end()) {
    // today is a new day
    bytes_today = delta_bytes_used;
  } else {
    bytes_today = bytes_per_day_[today] + delta_bytes_used;
    // check for overflow
    if (bytes_today < 0) {
      LOG(WARNING) << "OnByteCounterUpdate: overflow detected: |bytes_today|";
      bytes_today = kint64max;
    }
  }
  bytes_per_day_[today] = bytes_today;

  // update |accumulated_count_| and maybe update the file
  accumulated_count_ += delta_bytes_used;
  // update file if |accumulated_count_| exceeds |kFileUpdateIntervalBytes|
  if (accumulated_count_ > kFileUpdateIntervalBytes || accumulated_count_ < 0) {
    if (!UpdateFile()) {
      LOG(WARNING) << "OnByteCounterUpdate: failed to update file";
    }
  }
}

std::map<std::string, int64_t> AggregatorImpl::GetBytesPerDay() {
  std::map<std::string, int64_t> bytes_per_day;
  // TODO(mikemeko): fill this function to return whatever structure
  // contains our bytes per day info.
  return bytes_per_day;
}

// private

// static
std::string AggregatorImpl::StringFromMap(const BytesPerDay &table_map) {
  std::string table_str;
  BytesPerDay::const_iterator it;
  for (it = table_map.begin(); it != table_map.end(); ++it) {
    std::string date_str = (it->first).ToString();
    std::string bytes = base::Int64ToString(it->second);
    base::StringAppendF(&table_str, kLineFormat,
        date_str.c_str(), kColumnDelimiter, bytes.c_str());
  }
  return table_str;
}

// static
bool AggregatorImpl::MapFromString(const std::string &table_str,
                                   BytesPerDay* table_map) {
  CHECK(table_map != NULL);
  CHECK(table_map->empty());

  // split up |table_str| into lines
  StringParts lines;
  base::SplitString(table_str, '\n', &lines);

  // number of non-empty lines we successfully parse (added to |table_map|)
  int parsed_lines = 0;
  // number of non-empty lines we fail to parse (not added to |table_map|)
  int unparsed_lines = 0;

  // construct |table_map| from |table_str| line by line
  StringParts::const_iterator it;
  for (it = lines.begin(); it != lines.end(); ++it) {
    // skip empty lines (empty lines are considered neither parsed nor unparsed)
    if (it->empty()) {
      continue;
    }

    int line_number = it - lines.begin() + 1;

    // split up line into date and bytes
    StringParts date_bytes;
    base::SplitString(*it, kColumnDelimiter, &date_bytes);
    if (date_bytes.size() != 2) {
      LOG(WARNING) << "MapFromString: wrong format in line: " << line_number;
      ++unparsed_lines;
      continue;
    }

    const std::string& date_str = date_bytes[0];
    const std::string& bytes_str = date_bytes[1];

    Date date;
    if (!Date::FromString(date_str, &date)) {
      LOG(WARNING) << "MapFromString: failed to parse date in line: "
          << line_number;
      ++unparsed_lines;
      continue;
    }

    // check if |date| has already been recorded in |table_map|
    if (table_map->find(date) != table_map->end()) {
      // |date| has already been recorded in |table_map|
      LOG(WARNING) << "MapFromString: day \"" << date_str
          << "\" recorded more than once";
      // fall through and try to record this occurrence
    }

    ByteCount bytes;
    if (!base::StringToInt64(bytes_str, &bytes)) {
      LOG(WARNING) << "MapFromString: failed to parse byte count in line: "
          << line_number;
      ++unparsed_lines;
      continue;
    }

    // successfully parsed this line
    (*table_map)[date] = bytes;
    ++parsed_lines;
  }

  LOG(INFO) << "MapFromString: successfully parsed " << parsed_lines
      << " lines";
  if (unparsed_lines > 0) {
    LOG(WARNING) << "MapFromString: failed to parse " << unparsed_lines
        << " lines";
    return false;
  }
  return true;
}

// static
void AggregatorImpl::TrimOldData(BytesPerDay *bytes_per_day, Date baseline) {
  CHECK(bytes_per_day != NULL);

  BytesPerDay::iterator it;
  for (it = bytes_per_day->begin(); it != bytes_per_day->end(); ) {
    BytesPerDay::iterator current = it++;
    Date current_date = current->first;

    // get age of |current_date| with respect to |baseline|
    base::TimeDelta current_date_age = baseline.ToLocalTime() -
        current_date.ToLocalTime();

    // if |current_date| is too old, remove it
    if (current_date_age.InDays() >= kCleanUpDays) {
      LOG(INFO) << "TrimOldData: removing record for date: "
          << current_date.ToString();
      bytes_per_day->erase(current);
    }
  }
}

bool AggregatorImpl::GetCurrentData() {
  // check if the file exists
  if (!file_util::PathExists(kFilePath)) {
    return false;
  }

  // read the file
  std::string file_content;
  if (!file_util::ReadFileToString(kFilePath, &file_content)) {
    LOG(WARNING) << "GetCurrentData: failed to read file: "
        << kFilePath.value();
    return false;
  }

  // parse the content
  if (!MapFromString(file_content, &bytes_per_day_)) {
    LOG(WARNING) << "GetCurrentData: failed to parse content";
    return false;
  }

  // trim out old records
  TrimOldData(&bytes_per_day_, Date::TodayLocal());

  return true;
}

bool AggregatorImpl::UpdateFile() {
  CHECK(initialized_);
  if (accumulated_count_ == 0) {
    // nothing to do
    return true;
  }

  // trim out old records
  TrimOldData(&bytes_per_day_, Date::TodayLocal());

  // get content to write to file
  std::string to_write = StringFromMap(bytes_per_day_);

  // write to file
  if (!file_util::WriteFile(kFilePath, to_write.c_str(), to_write.size())) {
    LOG(WARNING) << "UpdateFile: failed to write to file";
    return false;
  }

  // reset |accumulated_count_|
  accumulated_count_ = 0;

  return true;
}

// static
gboolean AggregatorImpl::StaticUpdateFile(gpointer data) {
  AggregatorImpl *aggregator = reinterpret_cast<AggregatorImpl*>(data);
  CHECK(aggregator != NULL);
  if (!aggregator->UpdateFile()) {
    LOG(WARNING) << "StaticUpdateFile: failed to update file";
  }
  return TRUE;  // call back
}

bool AggregatorImpl::CreateUpdateTimer() {
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
    LOG(WARNING) << "CreateUpdateTimer: failed to attach timeout source";
    return false;
  }

  return true;
}

void AggregatorImpl::DestroyUpdateTimer() {
  if (update_timeout_source_ != NULL) {
    g_source_destroy(update_timeout_source_);
    update_timeout_source_ = NULL;
  }
}

}  // namespace cashew
