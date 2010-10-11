# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

sbin_PROGRAMS = cashewd

cashewd_SOURCES = \
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
  service_manager.cc \
  main.cc

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

# CFLAGS and LIBS info comes from PKG_CHECK_MODULES checks in configure.ac
# We manually define info below for libs with no pkg-config file

LIBBASE_CFLAGS =
LIBBASE_LIBS = -lbase

GFLAGS_CFLAGS =
GFLAGS_LIBS = -lgflags

cashewd_CPPFLAGS = -Wall -Werror \
  $(LIBBASE_CFLAGS) \
  @CURL_CFLAGS@ \
  @DBUS_CPP_CFLAGS@ \
  $(GFLAGS_CFLAGS) \
  @GLIB_CFLAGS@ \
  @GLOG_CFLAGS@

cashewd_LDADD = \
  $(LIBBASE_LIBS) \
  @CURL_LIBS@ \
  @DBUS_CPP_LIBS@ \
  $(GFLAGS_LIBS) \
  @GLIB_LIBS@ \
  @GLOG_LIBS@
