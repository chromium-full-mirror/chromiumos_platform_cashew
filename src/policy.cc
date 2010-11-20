// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/policy.h"

#include "src/default_policy.h"

namespace cashew {

// static
Policy* Policy::GetPolicy(const std::string& carrier) {
  // for now, everyone gets the default policy
  return new(std::nothrow) DefaultPolicy(carrier);
}

}  // namespace cashew
