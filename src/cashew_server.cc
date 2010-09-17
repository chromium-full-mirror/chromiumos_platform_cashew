// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/cashew_server.h"

#include <glog/logging.h>

namespace cashew {

const char* CashewServer::kServiceName = "org.chromium.Cashew";
const char* CashewServer::kServicePath = "/org/chromium/Cashew";

CashewServer::CashewServer(DBus::Connection& connection)  // NOLINT
    : DBus::ObjectAdaptor(connection, kServicePath) {
}

// Cashew D-Bus API Methods

bool CashewServer::IsAlive(DBus::Error& error) {  // NOLINT
  DLOG(INFO) << "IsAlive";
  return true;
}

}  // namespace cashew
