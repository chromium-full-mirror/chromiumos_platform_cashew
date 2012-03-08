# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = extrospection introspection src

EXTRA_DIST = \
  cashew.conf \
  org.chromium.Cashew.conf

initdir = $(sysconfdir)/init
init_DATA = cashew.conf

dbusdir = $(sysconfdir)/dbus-1/system.d
dbus_DATA = org.chromium.Cashew.conf
