/*
 * Copyright 2015 Facebook, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <folly/Singleton.h>
#include <folly/experimental/symbolizer/Symbolizer.h>

namespace folly {

namespace {

std::string stackTraceGetter() {
  // Get and symbolize stack trace
  constexpr size_t kMaxStackTraceDepth = 100;
  symbolizer::FrameArray<kMaxStackTraceDepth> addresses;

  if (!getStackTraceSafe(addresses)) {
    return "";
  } else {
    constexpr size_t kDefaultCapacity = 500;
    symbolizer::ElfCache elfCache(kDefaultCapacity);

    symbolizer::Symbolizer symbolizer(&elfCache);
    symbolizer.symbolize(addresses);

    symbolizer::StringSymbolizePrinter printer;
    printer.println(addresses);
    return printer.str();
  }
}

struct SetStackTraceGetter {
  SetStackTraceGetter() {
    SingletonVault::stackTraceGetter().store(stackTraceGetter);
  }
};

#ifdef __APPLE__
// OS X doesn't support constructor priorities.
SetStackTraceGetter setStackTraceGetter;
#else
SetStackTraceGetter __attribute__((__init_priority__(101))) setStackTraceGetter;
#endif
}

}
