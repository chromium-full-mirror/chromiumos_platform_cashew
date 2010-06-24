# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

sbin_PROGRAMS = cashewd

cashewd_SOURCES = main.cc

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

EXTRA_DIST = cashew.conf
