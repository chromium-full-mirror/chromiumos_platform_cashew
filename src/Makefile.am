# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

#================================================================
# libcashew
#
# libcashew contains the bulk of the cashew source code
# we link it with main.cc to produce cashewd, and we link it with
# our *_unittest.cc sources to produce our unit test executable
#================================================================

noinst_LTLIBRARIES = libcashew.la

libcashew_la_SOURCES = \
  cashew_server.h \
  cashew_server.cc \
  data_plan.h \
  data_plan.cc \
  data_plan_provider.h \
  data_plan_provider.cc \
  default_policy.h \
  default_policy.cc \
  device.h \
  device.cc \
  http_fetcher.h \
  libcurl_http_fetcher.h \
  libcurl_http_fetcher.cc \
  policy.h \
  policy.cc \
  service.h \
  service.cc \
  service_manager.h \
  service_manager.cc

# D-Bus server stub generation
DBUSXX_XML2CPP = dbusxx-xml2cpp
cashew_server_glue.h: $(top_srcdir)/introspection/cashew.xml
	$(DBUSXX_XML2CPP) $^ --adaptor=$@

# D-Bus client stub generation
flimflam_device_client_glue.h: $(top_srcdir)/extrospection/flimflam-device.xml
	$(DBUSXX_XML2CPP) $^ --proxy=$@
flimflam_manager_client_glue.h: $(top_srcdir)/extrospection/flimflam-manager.xml
	$(DBUSXX_XML2CPP) $^ --proxy=$@
flimflam_service_client_glue.h: $(top_srcdir)/extrospection/flimflam-service.xml
	$(DBUSXX_XML2CPP) $^ --proxy=$@

BUILT_SOURCES = \
  cashew_server_glue.h \
  flimflam_device_client_glue.h \
  flimflam_manager_client_glue.h \
  flimflam_service_client_glue.h

CLEANFILES = $(BUILT_SOURCES)

# most CFLAGS and LIBS info comes from PKG_CHECK_MODULES checks in configure.ac
# we manually define info below for libs with no pkg-config file

LIBBASE_CFLAGS =
LIBBASE_LIBS = -lbase

GFLAGS_CFLAGS =
GFLAGS_LIBS = -lgflags

# these COMMON defs are shared among libcashew, cashewd, and cashew_unittest

COMMON_CFLAGS = \
  -Wall -Werror \
  -I$(top_srcdir)/src \
  $(LIBBASE_CFLAGS) \
  @CURL_CFLAGS@ \
  @DBUS_CPP_CFLAGS@ \
  $(GFLAGS_CFLAGS) \
  @GLIB_CFLAGS@ \
  @GLOG_CFLAGS@

COMMON_LIBS = \
  $(LIBBASE_LIBS) \
  @CURL_LIBS@ \
  @DBUS_CPP_LIBS@ \
  $(GFLAGS_LIBS) \
  @GLIB_LIBS@ \
  @GLOG_LIBS@

libcashew_la_CPPFLAGS = \
  $(COMMON_CFLAGS)

libcashew_la_LIBADD = \
  $(COMMON_LIBS)

LIBCASHEW = ./libcashew.la

#==============
# cashew daemon
#==============

sbin_PROGRAMS = cashewd

cashewd_SOURCES = \
  main.cc

cashewd_CPPFLAGS = \
  $(COMMON_CFLAGS)

cashewd_LDADD = \
  $(LIBCASHEW) \
  $(COMMON_LIBS)

#===========
# unit tests
#===========

check_PROGRAMS = cashew_unittest

cashew_unittest_SOURCES = \
  cashew_unittest.cc \
  data_plan_unittest.cc \
  default_policy_unittest.cc

GTEST_CFLAGS =
GTEST_LIBS = -lgtest

cashew_unittest_CPPFLAGS = \
  $(COMMON_CFLAGS) \
  $(GTEST_CFLAGS)

cashew_unittest_LDADD = \
  $(LIBCASHEW) \
  $(COMMON_LIBS) \
  $(GTEST_LIBS)
