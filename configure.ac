# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

AC_INIT([cashew], [0.0.1], [chromium-os-dev@chromium.org])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_PROG_CXX
PKG_CHECK_MODULES([DBUS_CPP], [dbus-c++-1])
# TODO(vlaviano): check for gflags
PKG_CHECK_MODULES([GLOG], [libglog])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
    Makefile
    introspection/Makefile
    src/Makefile
])
AC_OUTPUT
