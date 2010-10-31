// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/data_plan_provider.h"

#include <base/scoped_ptr.h>
#include <glog/logging.h>

#include "src/json_reader.h"
#include "src/libcurl_http_fetcher.h"
#include "src/values.h"

namespace cashew {

DataPlanProvider::DataPlanProvider(const std::string& carrier)
    : carrier_(carrier), delegate_(NULL),
      fetcher_(CHECK_NOTNULL(new(std::nothrow) LibcurlHttpFetcher())),
      request_in_progress_(false) {
  fetcher_->set_delegate(this);
}

DataPlanProvider::~DataPlanProvider() {
  CancelPendingRequests();
}

const std::string& DataPlanProvider::GetCarrier() const {
  return carrier_;
}

const std::string& DataPlanProvider::GetUsageUrl() const {
  return usage_url_;
}

void DataPlanProvider::SetUsageUrl(const std::string& usage_url) {
  DLOG(INFO) << carrier_ << ": SetUsageUrl: url = " << usage_url;
  if (usage_url_ != usage_url) {
    usage_url_ = usage_url;
    CancelPendingRequests();
  }
}

void DataPlanProvider::SetDelegate(DataPlanProviderDelegate *delegate) {
  DLOG(INFO) << carrier_ << ": SetDelegate";
  delegate_ = delegate;
  if (delegate_ == NULL) {
    CancelPendingRequests();
  }
}

bool DataPlanProvider::RequestUsageUpdate() {
  if (usage_url_.empty()) {
    DLOG(WARNING) << carrier_ << ": RequestUsageUpdate: no usage url";
    return false;
  }
  if (delegate_ == NULL) {
    DLOG(WARNING) << carrier_ << ": RequestUsageUpdate: no delegate";
    return false;
  }
  if (request_in_progress_) {
    DLOG(INFO) << carrier_
        << ": RequestUsageUpdate: request already in progress";
    // we'll satisfy this request with the result from the in-flight one
    return true;
  }
  DCHECK(response_buffer_.empty());
  DLOG(INFO) << carrier_ << ": RequestUsageUpdate: initiating request";
  request_in_progress_ = true;
  fetcher_->BeginTransfer(usage_url_);
  return true;
}

// HttpFetcherDelegate methods

void DataPlanProvider::ReceivedBytes(HttpFetcher *fetcher, const char *bytes,
                                     int length) {
  DCHECK(fetcher != NULL);
  DCHECK(bytes != NULL);
  DCHECK_GE(length, 0);
  if (fetcher != fetcher_) {
    DLOG(WARNING) << carrier_ << ": ReceivedBytes: wrong fetcher";
    return;
  }
  DCHECK(request_in_progress_);
  DLOG(INFO) << carrier_ << ": ReceivedBytes: length = " << length;
  response_buffer_.reserve(response_buffer_.size() + length);
  response_buffer_.insert(response_buffer_.end(), bytes, bytes + length);
}

void DataPlanProvider::TransferComplete(HttpFetcher *fetcher,
                                        bool successful) {
  DCHECK(fetcher != NULL);
  if (fetcher != fetcher_) {
    DLOG(WARNING) << carrier_ << ": TransferComplete: wrong fetcher";
    return;
  }
  DCHECK(request_in_progress_);
  DCHECK(delegate_ != NULL);
  DLOG(INFO) << carrier_ << ": TransferComplete: result = " << successful;
  if (!successful) {
    LOG(WARNING) << carrier_ << ": TransferComplete: failed: response code = "
        << fetcher_->http_response_code();
    response_buffer_.clear();
    request_in_progress_ = false;
    delegate_->OnRequestComplete(this, false, NULL);
    return;
  }
  LOG(INFO) << carrier_ << ": TransferComplete: success: response code = "
      << fetcher_->http_response_code();

  // carrier usage API has (we hope) given us a JSON-encoded result
  // log it and then parse it, converting it into a tree of Value nodes
  const std::string json_encoded_usage_update(response_buffer_.begin(),
                                              response_buffer_.end());
  response_buffer_.clear();
  request_in_progress_ = false;

  LOG(INFO) << carrier_ << ": TransferComplete: response:\n"
      << json_encoded_usage_update;

  int error_code;
  std::string error_msg;
  scoped_ptr<Value> parsed_usage_update(
      JSONReader::ReadAndReturnError(json_encoded_usage_update, true,
                                     &error_code, &error_msg));
  if (parsed_usage_update == NULL) {
    LOG(WARNING) << carrier_ << ": TransferComplete: could not parse json: "
        << "error code " << error_code << ": " << error_msg;
    delegate_->OnRequestComplete(this, false, NULL);
    return;
  }
  DLOG(INFO) << carrier_ << ": TransferComplete: successfully parsed json";
  delegate_->OnRequestComplete(this, true, parsed_usage_update.get());
}

void DataPlanProvider::CancelPendingRequests() {
  if (request_in_progress_) {
    fetcher_->TerminateTransfer();
    response_buffer_.clear();
    request_in_progress_ = false;
  }
}

// private methods

}  // namespace cashew
