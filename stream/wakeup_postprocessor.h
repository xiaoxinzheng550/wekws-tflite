// Copyright (c) 2022 Zhendong Peng (pzd17@tsinghua.org.cn)
// Modifications Copyright (c) 2026 GengXin Zheng (xiaoxinzheng35@foxmail.com)
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0

#ifndef STREAM_WAKEUP_POSTPROCESSOR_H_
#define STREAM_WAKEUP_POSTPROCESSOR_H_

#include <array>
#include <cstddef>
#include <vector>

namespace wekws {

constexpr size_t kNumWakeWords = 2;

struct WakeWordInfo {
  int class_index;
  const char* name;
};

const std::array<WakeWordInfo, kNumWakeWords>& WakeWords();

struct WakeupResult {
  std::array<float, kNumWakeWords> scores{};
  std::array<int, kNumWakeWords> frames{{-1, -1}};
  int best_keyword = 0;
  int triggered_keyword = -1;
  const char* trigger_reason = nullptr;
  bool rearmed = false;

  bool triggered() const { return triggered_keyword >= 0; }
};

// Converts frame-level probabilities into debounced wake-up events.
class WakeupPostprocessor {
 public:
  WakeupPostprocessor(float high_threshold, float medium_threshold,
                      int medium_hits_required, float release_threshold,
                      int release_low_frames_required);

  WakeupResult Process(
      const std::vector<std::vector<float>>& probabilities, int begin_frame,
      int end_frame);

 private:
  float high_threshold_;
  float medium_threshold_;
  int medium_hits_required_;
  float release_threshold_;
  int release_low_frames_required_;
  bool armed_ = true;
  std::array<int, kNumWakeWords> medium_hit_streak_{};
  int consecutive_release_low_frames_ = 0;
};

}  // namespace wekws

#endif  // STREAM_WAKEUP_POSTPROCESSOR_H_
