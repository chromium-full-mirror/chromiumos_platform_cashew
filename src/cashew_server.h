// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CASHEW_SERVER_H_
#define SRC_CASHEW_SERVER_H_

#include <base/basictypes.h>
#include <dbus-c++/dbus.h>

#include "src/cashew_server_glue.h"

namespace cashew {

// Implements Cashew D-Bus API
class CashewServer : public org::chromium::Cashew_adaptor,
                     public DBus::IntrospectableAdaptor,
                     public DBus::ObjectAdaptor {
  public:
    explicit CashewServer(DBus::Connection& connection);  // NOLINT
    virtual ~CashewServer() {}

    // Cashew D-Bus API methods
    // TODO(vlaviano): add real API methods

    virtual bool IsAlive(DBus::Error& error);  // NOLINT

    static const char* kServiceName;
    static const char* kServicePath;

  private:
    DISALLOW_COPY_AND_ASSIGN(CashewServer);
};

}  // namespace cashew

#endif  // SRC_CASHEW_SERVER_H_
