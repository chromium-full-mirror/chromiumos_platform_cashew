// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_PROPERTY_CHANGED_HANDLER_H_
#define SRC_PROPERTY_CHANGED_HANDLER_H_

#include <glib.h>

#include <queue>
#include <string>
#include <utility>

#include <base/basictypes.h>  // NOLINT
#include <dbus-c++/dbus.h>  // NOLINT

namespace cashew {

class PropertyChangedDelegate;

// tuple of (property name, new property value) representing a D-Bus
// PropertyChanged signal
typedef std::pair<const std::string, const DBus::Variant> PropertyChangedSignal;

// queue of PropertyChangedSignal tuples
typedef std::queue<PropertyChangedSignal> PropertyChangedQueue;

// PropertyChangedHandler allows tuples representing D-Bus PropertyChanged
// signals to be enqueued for deferred handling from the glib main loop.
class PropertyChangedHandler {
  public:
    PropertyChangedHandler();
    virtual ~PropertyChangedHandler();

    // set delegate that will receive OnPropertyChanged callbacks
    // ok to clear delegate by setting it to NULL
    virtual void delegate(PropertyChangedDelegate *delegate);

    // add a PropertyChangedSignal to |signal_queue_|
    // schedules a glib callback for deferred signal processing if one is not
    // already scheduled
    virtual void EnqueueSignal(const PropertyChangedSignal& signal);

  private:
    // delegate for OnPropertyChanged callbacks
    PropertyChangedDelegate *delegate_;

    // glib source id for deferred processing callback
    guint source_id_;

    // queue of PropertyChangedSignal tuples awaiting deferred processing
    PropertyChangedQueue signal_queue_;

    // glib integration: static wrapper
    // takes PropertyChangedHandler as data and, if delegate is set, invokes
    // handler->delegate_->OnPropertyChanged for each signal in |signal_queue_|
    static gboolean StaticOnPropertyChangedCallback(gpointer data);

    // clear |signal_queue_|
    void DeletePendingSignals();

    DISALLOW_COPY_AND_ASSIGN(PropertyChangedHandler);
};

// callback interface for deferred PropertyChanged signal processing
class PropertyChangedDelegate {
  public:
    PropertyChangedDelegate() {}
    virtual ~PropertyChangedDelegate() {}

    // callback method for deferred PropertyChanged signal processing
    virtual void OnPropertyChanged(const PropertyChangedHandler *handler,
                                   const std::string& property_name,
                                   const DBus::Variant& new_value) = 0;

  private:
    DISALLOW_COPY_AND_ASSIGN(PropertyChangedDelegate);
};

}  // namespace cashew

#endif  // SRC_PROPERTY_CHANGED_HANDLER_H_
