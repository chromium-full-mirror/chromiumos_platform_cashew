# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

sbin_PROGRAMS = cashewd

cashewd_SOURCES = \
  cashew_server.h \
  cashew_server.cc \
  main.cc

# D-Bus server stub generation
DBUSXX_XML2CPP = dbusxx-xml2cpp
cashew_server_glue.h: $(top_srcdir)/introspection/cashew.xml
	$(DBUSXX_XML2CPP) $^ --adaptor=$@

BUILT_SOURCES = \
  cashew_server_glue.h

CLEANFILES = $(BUILT_SOURCES)

# manually defining these instead of getting them via autoconf, because gflags
# has no pkg-config file
GFLAGS_CFLAGS =
GFLAGS_LIBS = -lgflags

cashewd_CPPFLAGS = -Wall -Werror \
  @DBUS_CPP_CFLAGS@ \
  $(GFLAGS_CFLAGS) \
  @GLOG_CFLAGS@

cashewd_LDADD = \
  @DBUS_CPP_LIBS@ \
  $(GFLAGS_LIBS) \
  @GLOG_LIBS@
