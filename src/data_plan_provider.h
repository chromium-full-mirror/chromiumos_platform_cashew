// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_DATA_PLAN_PROVIDER_H_
#define SRC_DATA_PLAN_PROVIDER_H_

#include <string>
#include <vector>

#include <base/basictypes.h>  // NOLINT
#include <base/scoped_ptr.h>  // NOLINT
#include <base/values.h>  // NOLINT

#include "src/http_fetcher.h"

namespace cashew {

class DataPlanProviderDelegate;

// carrier usage API proxy
class DataPlanProvider: public HttpFetcherDelegate {
  public:
    explicit DataPlanProvider(const std::string& carrier);
    virtual ~DataPlanProvider();

    // get cellular carrier for which this proxy is configured
    virtual const std::string& GetCarrier() const;

    // get usage API URL for which this proxy is configured
    virtual const std::string& GetUsageUrl() const;

    // set usage API URL for which this proxy is configured
    virtual void SetUsageUrl(const std::string& usage_url);

    // set delegate interface that will receive callbacks from this proxy
    // it's ok to clear this by setting it to NULL
    virtual void SetDelegate(DataPlanProviderDelegate *delegate);

    // request that proxy obtain updated info from carrier API
    // NOTES:
    // - the request is asynchronous
    // - the result will be returned via a callback to the delegate
    //   that has been set via SetDelegate
    // - returns true on success and false on failure
    // - success indicates only that a request to the API was initiated
    //   successfully, but it does not guarantee that the request will
    //   ultimately succeed
    // - this call will fail if no valid url has been set via SetUsageUrl
    // - this call will fail if no delegate has been set via SetDelegate
    // - this call will return success and do nothing if an earlier
    //   request is already in progress
    virtual bool RequestUsageUpdate();

    // HttpFetcherDelegate methods

    // transfer is progressing, we've received more bytes
    virtual void ReceivedBytes(HttpFetcher *fetcher, const char *bytes,
                               int length);

    // transfer has ended (either complete or aborted)
    virtual void TransferComplete(HttpFetcher *fetcher, bool successful);

    // cancel any pending requests that are in progress
    void CancelPendingRequests();

  private:
    // cellular carrier
    const std::string carrier_;

    // usage API URL
    std::string usage_url_;

    // delegate
    DataPlanProviderDelegate *delegate_;

    // http fetcher
    scoped_ptr<HttpFetcher> fetcher_;

    // do we have an http request in progress?
    bool request_in_progress_;

    // stores response that we receive from usage API
    std::vector<char> response_buffer_;

    DISALLOW_COPY_AND_ASSIGN(DataPlanProvider);
};

// interface for delegates
class DataPlanProviderDelegate {
  public:
    virtual ~DataPlanProviderDelegate() {}

    // Called when request has either completed successfully or failed.
    // |parsed_usage_update| will be NULL if |successful| is false.
    // Caller owns |parsed_usage_update| and will delete it after this
    // callback returns, so delegate should copy anything it needs.
    virtual void OnRequestComplete(const DataPlanProvider *provider,
                                   bool successful,
                                   const Value *parsed_usage_update) = 0;
};

}  // namespace cashew

#endif  // SRC_DATA_PLAN_PROVIDER_H_
