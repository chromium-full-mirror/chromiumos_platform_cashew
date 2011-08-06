// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_SERVICE_MANAGER_H_
#define SRC_SERVICE_MANAGER_H_

#include <glib.h>

#include <map>
#include <string>
#include <vector>

#include <base/basictypes.h>  // NOLINT
#include <dbus-c++/dbus.h>  // NOLINT

#include "src/service.h"

namespace cashew {

class Aggregator;
class CashewServer;
class MetricsManager;

// map of service path names to Service objects
typedef std::map<std::string, Service*> ServiceMap;

// vector of DBus::Path objects (strings) representing service path names
typedef std::vector<DBus::Path> ServicePathList;

// ServiceManager interface
// monitors Flimflam and maintains a collection of cellular service objects
class ServiceManager {
  public:
    ServiceManager() {}
    virtual ~ServiceManager() {}

    // look up a service by its path name
    // returns NULL if service is not found
    virtual const Service* GetService(const std::string& service_path) const
        = 0;

    // set Cashew server
    // we'll talk to it when we want to emit updates to the world on behalf of
    // our child Services.
    // it's ok to clear this by setting it to NULL, in which case our updates
    // will go into a black hole.
    virtual void SetCashewServer(CashewServer *cashew_server) = 0;

    // return our latest info on the current default technology
    virtual Service::Type GetDefaultTechnology() const = 0;

    // Service methods

    // a child Service wants us to emit an update about its data plans
    virtual void EmitDataPlansUpdate(const Service& service) = 0;

    // |plan|, associated with |service|, has become inactive
    virtual void OnDataPlanInactive(const Service& service,
                                    const DataPlan& plan) = 0;

    // factory
    static ServiceManager* NewServiceManager(
                                        DBus::Connection& connection,  // NOLINT
                                        GMainLoop * const main_loop,
                                        MetricsManager * const metrics_manager,
                                        Aggregator * const aggregator);

  private:
    DISALLOW_COPY_AND_ASSIGN(ServiceManager);
};

}  // namespace cashew

#endif  // SRC_SERVICE_MANAGER_H_
