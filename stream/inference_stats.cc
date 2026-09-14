// Copyright (c) 2022 Zhendong Peng (pzd17@tsinghua.org.cn)
// Modifications Copyright (c) 2026 GengXin Zheng (xiaoxinzheng35@foxmail.com)
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0

#include "stream/inference_stats.h"

#include <algorithm>
#include <iostream>

namespace wekws {

InferenceStats::InferenceStats(int sample_rate, int frame_length,
                               int frame_shift)
    : sample_rate_(sample_rate),
      frame_length_(frame_length),
      frame_shift_(frame_shift),
      stream_start_(std::chrono::steady_clock::now()) {}

InferenceMetrics InferenceStats::Record(
    size_t new_frames, unsigned long long captured_samples, int queued_frames,
    const std::chrono::steady_clock::time_point& inference_start,
    const std::chrono::steady_clock::time_point& inference_end) {
  ++inference_count_;
  processed_frames_ += new_frames;

  InferenceMetrics metrics;
  metrics.inference_count = inference_count_;
  metrics.processed_frames = processed_frames_;
  metrics.first_new_absolute_frame = processed_frames_ - new_frames;
  metrics.processed_audio_seconds =
      static_cast<double>(frame_length_ +
                          (processed_frames_ - 1) * frame_shift_) /
      sample_rate_;
  metrics.captured_audio_seconds =
      static_cast<double>(captured_samples) / sample_rate_;
  const double wall_seconds =
      std::chrono::duration<double>(inference_end - stream_start_).count();
  metrics.capture_lag_ms =
      std::max(0.0, (wall_seconds - metrics.captured_audio_seconds) * 1000.0);
  metrics.backlog_ms =
      std::max(0.0, (metrics.captured_audio_seconds -
                     metrics.processed_audio_seconds) *
                        1000.0);
  metrics.inference_ms =
      std::chrono::duration<double, std::milli>(inference_end -
                                                inference_start)
          .count();
  metrics.queued_frames = queued_frames;
  return metrics;
}

double InferenceStats::FrameAudioSeconds(const InferenceMetrics& metrics,
                                         int window_frame,
                                         int newest_start) const {
  const int new_frame =
      window_frame >= newest_start ? window_frame - newest_start : -1;
  if (new_frame < 0) return -1.0;
  return static_cast<double>(
             frame_length_ +
             (metrics.first_new_absolute_frame + new_frame) * frame_shift_) /
         sample_rate_;
}

void PrintInferenceResult(const InferenceMetrics& metrics,
                          const WakeupResult& result, int newest_start,
                          const InferenceStats& stats) {
  const size_t best = static_cast<size_t>(result.best_keyword);
  const int best_new_frame = result.frames[best] >= newest_start
                                 ? result.frames[best] - newest_start
                                 : -1;
  std::cout << "inference=" << metrics.inference_count
            << " audio_time=" << metrics.processed_audio_seconds << "s"
            << " captured_audio=" << metrics.captured_audio_seconds << "s"
            << " capture_lag=" << metrics.capture_lag_ms << "ms"
            << " backlog=" << metrics.backlog_ms << "ms"
            << " queued_frames=" << metrics.queued_frames
            << " hi_xiaowen_score=" << result.scores[0]
            << " nihao_wenwen_score=" << result.scores[1]
            << " best_keyword=" << WakeWords()[best].name
            << " keyword_score=" << result.scores[best]
            << " new_frame=" << best_new_frame
            << " keyword_time="
            << stats.FrameAudioSeconds(metrics, result.frames[best],
                                       newest_start)
            << "s inference_time=" << metrics.inference_ms << "ms"
            << std::endl;
}

}  // namespace wekws
