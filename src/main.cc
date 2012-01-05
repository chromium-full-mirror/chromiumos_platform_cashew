// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <config.h>

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <glib-unix.h>

#include <dbus-c++/dbus.h>
#include <dbus-c++/glib-integration.h>
#include <gflags/gflags.h>
#include <glog/logging.h>

#include "src/aggregator.h"
#include "src/cashew_server.h"
#include "src/metrics_manager.h"
#include "src/service_manager.h"

namespace cashew {

static const int kMillisecondsPerSecond = 1000;
static const int kDBusConnectionTimeoutMilliseconds =
    1 * kMillisecondsPerSecond;

typedef int Signal;
typedef void (*SignalHandler)(Signal signal);

int InstallSignalHandler(const Signal signal,
                         GSourceFunc signal_handler) {
  LOG(INFO) << "installing handler for  signal " << signal;

  g_unix_signal_add(signal, signal_handler, reinterpret_cast<void*>(signal));
  return 0;
}

static GMainLoop *main_loop = NULL;
static DBus::Glib::BusDispatcher *dispatcher = NULL;
static MetricsManager *metrics_manager = NULL;
static Aggregator *aggregator = NULL;
static ServiceManager *service_manager = NULL;
static CashewServer *server = NULL;

static gboolean OnSignal(gpointer user_data) {
  long signal = reinterpret_cast<long>(user_data);
  LOG(INFO) << "received signal " << signal;
  if (main_loop != NULL) {
    g_main_loop_quit(main_loop);
  }
  return TRUE;
}

int InstallSignalHandlers() {
  if (InstallSignalHandler(SIGINT, OnSignal) < 0) {
    PLOG(ERROR) << "couldn't install SIGINT handler";
    return -1;
  }
  if (InstallSignalHandler(SIGTERM, OnSignal) < 0) {
    PLOG(ERROR) << "couldn't install SIGTERM handler";
    return -1;
  }
  return 0;
}

// should not be called while event loop is running
static void CleanUpForExit() {
  DCHECK(main_loop == NULL || !g_main_loop_is_running(main_loop));
  LOG(INFO) << "cleaning up";
  delete server;
  server = NULL;
  delete service_manager;
  service_manager = NULL;
  delete aggregator;
  aggregator = NULL;
  delete metrics_manager;
  metrics_manager = NULL;
  DBus::default_dispatcher = NULL;
  delete dispatcher;
  dispatcher = NULL;
  if (main_loop != NULL) {
    g_main_loop_unref(main_loop);
    main_loop = NULL;
  }
  google::ShutdownGoogleLogging();
}

}  // namespace cashew

int main(int argc, char *argv[]) {
  google::SetUsageMessage(argv[0]);
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  google::InstallFailureSignalHandler();
  LOG(INFO) << PACKAGE_STRING;

  LOG(INFO) << "creating main loop";
  cashew::main_loop = g_main_loop_new(NULL, false);
  if (cashew::main_loop == NULL) {
    LOG(ERROR) << "couldn't create main loop";
    cashew::CleanUpForExit();
    exit(EXIT_FAILURE);
  }

  LOG(INFO) << "creating dbus dispatcher";
  cashew::dispatcher = new(std::nothrow) DBus::Glib::BusDispatcher();
  if (cashew::dispatcher == NULL) {
    LOG(ERROR) << "couldn't create dbus dispatcher";
    cashew::CleanUpForExit();
    exit(EXIT_FAILURE);
  }
  DBus::default_dispatcher = cashew::dispatcher;
  cashew::dispatcher->attach(NULL);

  LOG(INFO) << "creating metrics manager";
  cashew::metrics_manager = cashew::MetricsManager::NewMetricsManager();
  if (cashew::metrics_manager == NULL) {
    LOG(ERROR) << "couldn't create metrics manager";
    cashew::CleanUpForExit();
    exit(EXIT_FAILURE);
  }

  LOG(INFO) << "creating aggregator";
  cashew::aggregator = cashew::Aggregator::NewAggregator();
  if (cashew::aggregator == NULL) {
    LOG(ERROR) << "couldn't create aggregator";
    cashew::CleanUpForExit();
    exit(EXIT_FAILURE);
  }

  LOG(INFO) << "creating service manager";
  DBus::Connection client_conn = DBus::Connection::SystemBus();
  client_conn.set_timeout(cashew::kDBusConnectionTimeoutMilliseconds);
  cashew::service_manager = cashew::ServiceManager::NewServiceManager(
      client_conn, cashew::main_loop, cashew::metrics_manager,
      cashew::aggregator);
  if (cashew::service_manager == NULL) {
    LOG(ERROR) << "couldn't create service manager";
    cashew::CleanUpForExit();
    exit(EXIT_FAILURE);
  }

  LOG(INFO) << "creating cashew server";
  // TODO(vlaviano): can we share a single conn for client and server roles?
  DBus::Connection server_conn = DBus::Connection::SystemBus();
  server_conn.request_name(cashew::CashewServer::kServiceName);
  server_conn.set_timeout(cashew::kDBusConnectionTimeoutMilliseconds);
  cashew::server = new(std::nothrow) cashew::CashewServer(server_conn,
      cashew::service_manager, cashew::main_loop, cashew::aggregator);
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

  LOG(INFO) << "entering event loop";
  g_main_loop_run(cashew::main_loop);
  LOG(INFO) << "exited event loop";
  cashew::CleanUpForExit();
  exit(EXIT_SUCCESS);
}
