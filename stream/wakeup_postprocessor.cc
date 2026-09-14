// Copyright (c) 2022 Zhendong Peng (pzd17@tsinghua.org.cn)
// Modifications Copyright (c) 2026 GengXin Zheng (xiaoxinzheng35@foxmail.com)
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0

#include "stream/wakeup_postprocessor.h"

#include <algorithm>

namespace wekws {
namespace {

constexpr std::array<WakeWordInfo, kNumWakeWords> kWakeWords = {{
    {0, "hi_xiaowen"},
    {1, "nihao_wenwen"},
}};

}  // namespace

const std::array<WakeWordInfo, kNumWakeWords>& WakeWords() {
  return kWakeWords;
}

WakeupPostprocessor::WakeupPostprocessor(float high_threshold,
                                         float medium_threshold,
                                         int medium_hits_required,
                                         float release_threshold,
                                         int release_low_frames_required)
    : high_threshold_(high_threshold),
      medium_threshold_(medium_threshold),
      medium_hits_required_(medium_hits_required),
      release_threshold_(release_threshold),
      release_low_frames_required_(release_low_frames_required) {}

WakeupResult WakeupPostprocessor::Process(
    const std::vector<std::vector<float>>& probabilities, int begin_frame,
    int end_frame) {
  WakeupResult result;
  begin_frame = std::max(0, begin_frame);
  end_frame = std::min(end_frame, static_cast<int>(probabilities.size()));

  for (int frame = begin_frame; frame < end_frame; ++frame) {
    for (size_t keyword = 0; keyword < kWakeWords.size(); ++keyword) {
      const int class_index = kWakeWords[keyword].class_index;
      if (probabilities[frame].size() > static_cast<size_t>(class_index) &&
          probabilities[frame][class_index] > result.scores[keyword]) {
        result.scores[keyword] = probabilities[frame][class_index];
        result.frames[keyword] = frame;
      }
    }
  }

  for (size_t keyword = 1; keyword < kWakeWords.size(); ++keyword) {
    if (result.scores[keyword] > result.scores[result.best_keyword]) {
      result.best_keyword = static_cast<int>(keyword);
    }
  }

  if (!armed_) {
    for (int frame = begin_frame; frame < end_frame; ++frame) {
      bool all_keywords_below_release = true;
      for (const auto& keyword : kWakeWords) {
        if (probabilities[frame].size() >
                static_cast<size_t>(keyword.class_index) &&
            probabilities[frame][keyword.class_index] >= release_threshold_) {
          all_keywords_below_release = false;
          break;
        }
      }
      consecutive_release_low_frames_ =
          all_keywords_below_release ? consecutive_release_low_frames_ + 1 : 0;
    }

    if (consecutive_release_low_frames_ >= release_low_frames_required_) {
      armed_ = true;
      medium_hit_streak_.fill(0);
      consecutive_release_low_frames_ = 0;
      result.rearmed = true;
    }
    return result;
  }

  float trigger_score = 0.0f;
  for (size_t keyword = 0; keyword < kWakeWords.size(); ++keyword) {
    if (result.scores[keyword] >= high_threshold_ &&
        result.scores[keyword] > trigger_score) {
      result.triggered_keyword = static_cast<int>(keyword);
      result.trigger_reason = "high";
      trigger_score = result.scores[keyword];
    }
  }

  if (!result.triggered()) {
    for (size_t keyword = 0; keyword < kWakeWords.size(); ++keyword) {
      medium_hit_streak_[keyword] =
          result.scores[keyword] >= medium_threshold_
              ? medium_hit_streak_[keyword] + 1
              : 0;
      if (medium_hit_streak_[keyword] >= medium_hits_required_ &&
          result.scores[keyword] > trigger_score) {
        result.triggered_keyword = static_cast<int>(keyword);
        result.trigger_reason = "medium_x2";
        trigger_score = result.scores[keyword];
      }
    }
  }

  if (result.triggered()) {
    armed_ = false;
    medium_hit_streak_.fill(0);
    consecutive_release_low_frames_ = 0;
  }
  return result;
}

}  // namespace wekws
