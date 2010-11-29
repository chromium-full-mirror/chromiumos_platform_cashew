// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/property_changed_handler.h"

#include <glog/logging.h>

namespace cashew {

PropertyChangedHandler::PropertyChangedHandler()
    : delegate_(NULL), source_id_(0) {}

PropertyChangedHandler::~PropertyChangedHandler() {
  DeletePendingSignals();
  if (source_id_ != 0 && !g_source_remove(source_id_)) {
    LOG(WARNING) << "dtor: g_source_remove failed";
  }
}

void PropertyChangedHandler::delegate(PropertyChangedDelegate *delegate) {
  delegate_ = delegate;
}

void PropertyChangedHandler::EnqueueSignal(
    const PropertyChangedSignal& signal) {
  signal_queue_.push(signal);
  // schedule glib callback to do deferred signal processing if one is not
  // already scheduled
  if (source_id_ != 0) {
    return;
  }
  source_id_ = g_idle_add(StaticOnPropertyChangedCallback, this);
  if (source_id_ == 0) {
    LOG(ERROR) << "EnqueueSignal: g_idle_add failed";
    return;
  }
}

// Private methods

// static
gboolean PropertyChangedHandler::StaticOnPropertyChangedCallback(
    gpointer data) {
  PropertyChangedHandler *handler =
      reinterpret_cast<PropertyChangedHandler*>(data);
  DCHECK(handler != NULL);
  DCHECK_NE(handler->source_id_, 0);
  // TODO(vlaviano): we could impose a limit on the number of signals that we're
  // willing to process per invocation and return TRUE to reschedule this
  // callback if there are signals remaining in the queue
  while (!handler->signal_queue_.empty()) {
    PropertyChangedSignal signal = handler->signal_queue_.front();
    const std::string& property_name = signal.first;
    const DBus::Variant& new_value = signal.second;
    if (handler->delegate_ != NULL) {
      handler->delegate_->OnPropertyChanged(handler, property_name, new_value);
    }
    handler->signal_queue_.pop();
  }
  // we don't want to be run again automatically
  // EnqueueSignal will schedule us as needed
  handler->source_id_ = 0;
  return FALSE;
}

void PropertyChangedHandler::DeletePendingSignals() {
  while (!signal_queue_.empty()) {
    signal_queue_.pop();
  }
}

}  // namespace cashew
