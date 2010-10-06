// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <config.h>

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dbus-c++/dbus.h>
#include <gflags/gflags.h>
#include <glog/logging.h>

#include "src/cashew_server.h"
#include "src/service_manager.h"

namespace cashew {

typedef int Signal;
typedef void (*SignalHandler)(Signal signal);

int InstallSignalHandler(const Signal signal,
                         const SignalHandler signal_handler,
                         const sigset_t& mask) {
  DLOG(INFO) << "installing handler for signal " << signal;
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  action.sa_handler = signal_handler;
  action.sa_mask = mask;
  return sigaction(signal, &action, NULL);
}

static DBus::BusDispatcher *dispatcher = NULL;

static CashewServer *server = NULL;

static ServiceManager *service_manager = NULL;

static void OnSignal(int signal) {
  DLOG(INFO) << "received signal " << signal;
  if (dispatcher) {
    dispatcher->leave();
  }
}

int InstallSignalHandlers() {
  sigset_t mask;
  if (sigemptyset(&mask) < 0) {
    PLOG(ERROR) << "sigemptyset failed";
    return -1;
  }
  if (sigaddset(&mask, SIGINT) < 0) {
    PLOG(ERROR) << "couldn't add SIGINT to mask";
    return -1;
  }
  if (sigaddset(&mask, SIGTERM) < 0) {
    PLOG(ERROR) << "couldn't add SIGTERM to mask";
    return -1;
  }
  if (InstallSignalHandler(SIGINT, OnSignal, mask) < 0) {
    PLOG(ERROR) << "couldn't install SIGINT handler";
    return -1;
  }
  if (InstallSignalHandler(SIGTERM, OnSignal, mask) < 0) {
    PLOG(ERROR) << "couldn't install SIGTERM handler";
    return -1;
  }
  return 0;
}

// should not be called while event loop is running
static void CleanUpForExit() {
  DLOG(INFO) << "cleaning up";
  delete server;
  server = NULL;
  delete service_manager;
  service_manager = NULL;
  DBus::default_dispatcher = NULL;
  DBus::BusDispatcher *dispatcher_to_delete = dispatcher;
  dispatcher = NULL;
  delete dispatcher_to_delete;
  // TODO(vlaviano): uncomment when we get a newer version of glog
  // google::ShutdownGoogleLogging();
}

}  // namespace cashew

int main(int argc, char *argv[]) {
  google::SetUsageMessage(argv[0]);
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  google::InstallFailureSignalHandler();
  LOG(INFO) << PACKAGE_STRING;

  DLOG(INFO) << "creating dbus dispatcher";
  cashew::dispatcher = new(std::nothrow) DBus::BusDispatcher();
  if (cashew::dispatcher == NULL) {
    LOG(ERROR) << "couldn't create dbus dispatcher";
    cashew::CleanUpForExit();
    exit(EXIT_FAILURE);
  }
  DBus::default_dispatcher = cashew::dispatcher;

  // TODO(vlaviano): policies

  // TODO(vlaviano): back-end carrier apis

  DLOG(INFO) << "creating service manager";
  DBus::Connection client_conn = DBus::Connection::SystemBus();
  cashew::service_manager = new(std::nothrow) cashew::ServiceManager(
      client_conn);
  if (cashew::service_manager == NULL) {
    LOG(ERROR) << "couldn't create service manager";
    cashew::CleanUpForExit();
    exit(EXIT_FAILURE);
  }

  DLOG(INFO) << "creating cashew server";
  // TODO(vlaviano): can we share a single conn for client and server roles?
  DBus::Connection server_conn = DBus::Connection::SystemBus();
  server_conn.request_name(cashew::CashewServer::kServiceName);
  cashew::server = new(std::nothrow) cashew::CashewServer(server_conn,
      *cashew::service_manager);
  if (cashew::server == NULL) {
    LOG(ERROR) << "couldn't create cashew server";
    cashew::CleanUpForExit();
    exit(EXIT_FAILURE);
  }

  if (cashew::InstallSignalHandlers() < 0) {
    LOG(ERROR) << "couldn't install signal handlers";
    cashew::CleanUpForExit();
    exit(EXIT_FAILURE);
  }

  DLOG(INFO) << "entering event loop";
  cashew::dispatcher->enter();
  DLOG(INFO) << "exited event loop";
  cashew::CleanUpForExit();
  exit(EXIT_SUCCESS);
}
