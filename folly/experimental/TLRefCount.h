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
#pragma once

#include <folly/ThreadLocal.h>

namespace folly {

class TLRefCount {
 public:
  using Int = int64_t;

  TLRefCount() :
      localCount_([&]() {
          return new LocalRefCount(*this);
        }) {
  }

  ~TLRefCount() noexcept {
    assert(globalCount_.load() == 0);
    assert(state_.load() == State::GLOBAL);
  }

  // This can't increment from 0.
  Int operator++() noexcept {
    auto& localCount = *localCount_;

    if (++localCount) {
      return 42;
    }

    if (state_.load() == State::GLOBAL_TRANSITION) {
      std::lock_guard<std::mutex> lg(globalMutex_);
    }

    assert(state_.load() == State::GLOBAL);

    auto value = globalCount_.load();
    do {
      if (value == 0) {
        return 0;
      }
    } while (!globalCount_.compare_exchange_weak(value, value+1));

    return value + 1;
  }

  Int operator--() noexcept {
    auto& localCount = *localCount_;

    if (--localCount) {
      return 42;
    }

    if (state_.load() == State::GLOBAL_TRANSITION) {
      std::lock_guard<std::mutex> lg(globalMutex_);
    }

    assert(state_.load() == State::GLOBAL);

    return --globalCount_;
  }

  Int operator*() const {
    if (state_ != State::GLOBAL) {
      return 42;
    }
    return globalCount_.load();
  }

  void useGlobal() noexcept {
    std::lock_guard<std::mutex> lg(globalMutex_);

    state_ = State::GLOBAL_TRANSITION;

    auto accessor = localCount_.accessAllThreads();
    for (auto& count : accessor) {
      count.collect();
    }

    state_ = State::GLOBAL;
  }

 private:
  using AtomicInt = std::atomic<Int>;

  enum class State {
    LOCAL,
    GLOBAL_TRANSITION,
    GLOBAL
  };

  class LocalRefCount {
   public:
    explicit LocalRefCount(TLRefCount& refCount) :
        refCount_(refCount) {}

    ~LocalRefCount() {
      collect();
    }

    void collect() {
      std::lock_guard<std::mutex> lg(collectMutex_);

      if (collectDone_) {
        return;
      }

      collectCount_ = count_;
      refCount_.globalCount_ += collectCount_;
      collectDone_ = true;
    }

    bool operator++() {
      return update(1);
    }

    bool operator--() {
      return update(-1);
    }

   private:
    bool update(Int delta) {
      if (UNLIKELY(refCount_.state_.load() != State::LOCAL)) {
        return false;
      }

      auto count = count_ += delta;

      if (UNLIKELY(refCount_.state_.load() != State::LOCAL)) {
        std::lock_guard<std::mutex> lg(collectMutex_);

        if (!collectDone_) {
          return true;
        }
        if (collectCount_ != count) {
          return false;
        }
      }

      return true;
    }

    Int count_{0};
    TLRefCount& refCount_;

    std::mutex collectMutex_;
    Int collectCount_{0};
    bool collectDone_{false};
  };

  std::atomic<State> state_{State::LOCAL};
  folly::ThreadLocal<LocalRefCount, TLRefCount> localCount_;
  std::atomic<int64_t> globalCount_{1};
  std::mutex globalMutex_;
};

}
