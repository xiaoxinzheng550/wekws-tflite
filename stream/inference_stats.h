// Copyright (c) 2022 Zhendong Peng (pzd17@tsinghua.org.cn)
// Modifications Copyright (c) 2026 GengXin Zheng (xiaoxinzheng35@foxmail.com)
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0

#ifndef STREAM_INFERENCE_STATS_H_
#define STREAM_INFERENCE_STATS_H_

#include <chrono>
#include <cstddef>

#include "stream/wakeup_postprocessor.h"

namespace wekws {

struct InferenceMetrics {
  unsigned long long inference_count = 0;
  unsigned long long processed_frames = 0;
  unsigned long long first_new_absolute_frame = 0;
  double processed_audio_seconds = 0.0;
  double captured_audio_seconds = 0.0;
  double capture_lag_ms = 0.0;
  double backlog_ms = 0.0;
  double inference_ms = 0.0;
  int queued_frames = 0;
};

class InferenceStats {
 public:
  InferenceStats(int sample_rate, int frame_length, int frame_shift);

  InferenceMetrics Record(
      size_t new_frames, unsigned long long captured_samples,
      int queued_frames,
      const std::chrono::steady_clock::time_point& inference_start,
      const std::chrono::steady_clock::time_point& inference_end);

  double FrameAudioSeconds(const InferenceMetrics& metrics, int window_frame,
                           int newest_start) const;

 private:
  int sample_rate_;
  int frame_length_;
  int frame_shift_;
  unsigned long long inference_count_ = 0;
  unsigned long long processed_frames_ = 0;
  std::chrono::steady_clock::time_point stream_start_;
};

void PrintInferenceResult(const InferenceMetrics& metrics,
                          const WakeupResult& result, int newest_start,
                          const InferenceStats& stats);

}  // namespace wekws

#endif  // STREAM_INFERENCE_STATS_H_
