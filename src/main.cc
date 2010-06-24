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

static DBus::BusDispatcher *g_dispatcher = NULL;

static void OnSignal(int signal) {
  DLOG(INFO) << "received signal " << signal;
  if (g_dispatcher) {
    g_dispatcher->leave();
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
  DBus::default_dispatcher = NULL;
  DBus::BusDispatcher *dispatcher_to_delete = g_dispatcher;
  g_dispatcher = NULL;
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
  cashew::g_dispatcher = new(std::nothrow) DBus::BusDispatcher();
  if (!cashew::g_dispatcher) {
    LOG(ERROR) << "couldn't create dbus dispatcher";
    cashew::CleanUpForExit();
    exit(EXIT_FAILURE);
  }
  DBus::default_dispatcher = cashew::g_dispatcher;

  // TODO(vlaviano): policies

  // TODO(vlaviano): back-end carrier apis

  // TODO(vlaviano): front-end dbus api

  if (cashew::InstallSignalHandlers() < 0) {
    LOG(ERROR) << "couldn't install signal handlers";
    cashew::CleanUpForExit();
    exit(EXIT_FAILURE);
  }

  DLOG(INFO) << "entering event loop";
  cashew::g_dispatcher->enter();
  DLOG(INFO) << "exited event loop";
  cashew::CleanUpForExit();
  exit(EXIT_SUCCESS);
}
