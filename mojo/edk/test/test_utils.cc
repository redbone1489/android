// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/test/test_utils.h"

#include <fcntl.h>
#include <stddef.h>
#include <unistd.h>

#include "base/posix/eintr_wrapper.h"

namespace mojo {
namespace edk {
namespace test {

PlatformHandle PlatformHandleFromFILE(base::ScopedFILE fp) {
  CHECK(fp);
  int rv = HANDLE_EINTR(dup(fileno(fp.get())));
  PCHECK(rv != -1) << "dup";
  return PlatformHandle(base::ScopedFD(rv));
}

base::ScopedFILE FILEFromPlatformHandle(PlatformHandle h, const char* mode) {
  CHECK(h.is_valid());
  base::ScopedFILE rv(fdopen(h.ReleaseFD(), mode));
  PCHECK(rv) << "fdopen";
  return rv;
}

}  // namespace test
}  // namespace edk
}  // namespace mojo
