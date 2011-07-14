# Copyright (c) 2010, 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

AC_INIT([cashew], [0.0.1], [chromium-os-dev@chromium.org])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
LT_INIT([disable-shared])
AC_PROG_CXX
AC_CONFIG_MACRO_DIR([m4])
# TODO(vlaviano): check for libbase
PKG_CHECK_MODULES([CURL], [libcurl])
PKG_CHECK_MODULES([DBUS_CPP], [dbus-c++-1])
# TODO(vlaviano): check for gflags
PKG_CHECK_MODULES([GLIB], [glib-2.0])
PKG_CHECK_MODULES([GLOG], [libglog])
# TODO(vlaviano): check for gmock
# TODO(vlaviano): check for gtest
# TODO(vlaviano): check for libmetrics
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
    Makefile
    extrospection/Makefile
    introspection/Makefile
    src/Makefile
])
AC_OUTPUT
