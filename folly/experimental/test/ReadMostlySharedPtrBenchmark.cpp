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
/* -*- Mode: C++; tab-width: 2; c-basic-offset: 2; indent-tabs-mode: nil -*- */

#include <thread>
#include <iostream>
#include <folly/Benchmark.h>
#include <folly/Memory.h>
#include <gflags/gflags.h>

#include <folly/experimental/ReadMostlySharedPtr.h>

template <template<typename> class MainPtr,
          template<typename> class WeakPtr,
          size_t threadCount>
void benchmark(size_t n) {
  MainPtr<int> mainPtr(folly::make_unique<int>(42));

  std::vector<std::thread> ts;

  for (size_t t = 0; t < threadCount; ++t) {
    ts.emplace_back([&]() {
        WeakPtr<int> weakPtr(mainPtr);

        for (size_t i = 0; i < n; ++i) {
          weakPtr.lock();
        }
      });
  }

  for (auto& t: ts) {
    t.join();
  }
}

template <typename T>
using RCUMainPtr = folly::ReadMostlyMainPtr<T, folly::RCURefCount>;
template <typename T>
using RCUWeakPtr = folly::ReadMostlyWeakPtr<T, folly::RCURefCount>;
template <typename T>
using TLMainPtr = folly::ReadMostlyMainPtr<T, folly::TLRefCount>;
template <typename T>
using TLWeakPtr = folly::ReadMostlyWeakPtr<T, folly::TLRefCount>;


BENCHMARK(WeakPtrOneThread, n) {
  benchmark<std::shared_ptr, std::weak_ptr, 1>(n);
}

BENCHMARK(WeakPtrFourThreads, n) {
  benchmark<std::shared_ptr, std::weak_ptr, 4>(n);
}

BENCHMARK(RCUReadMostlyWeakPtrOneThread, n) {
  benchmark<RCUMainPtr, RCUWeakPtr, 1>(n);
}

BENCHMARK(RCUReadMostlyWeakPtrFourThreads, n) {
  benchmark<RCUMainPtr, RCUWeakPtr, 4>(n);
}

BENCHMARK(TLReadMostlyWeakPtrOneThread, n) {
  benchmark<TLMainPtr, TLWeakPtr, 1>(n);
}

BENCHMARK(TLReadMostlyWeakPtrFourThreads, n) {
  benchmark<TLMainPtr, TLWeakPtr, 4>(n);
}

int main(int argc, char** argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  gflags::SetCommandLineOptionWithMode(
    "bm_min_usec", "100000", gflags::SET_FLAG_IF_DEFAULT
  );

  folly::runBenchmarks();

  return 0;
}
