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
  /**
   * 函数名：InferenceStats
   * 输入：sample_rate 采样率，frame_length 帧长，frame_shift 帧移
   * 输出：构造完成的性能统计对象
   * 函数功能：初始化流式处理的计数器和时间基准
   */
  InferenceStats(int sample_rate, int frame_length, int frame_shift);

  /**
   * 函数名：Record
   * 输入：new_frames 新处理帧数，captured_samples 已采样点数，queued_frames 排队帧数，inference_start/end 推理起止时刻
   * 输出：本轮完整的推理和音频进度指标
   * 函数功能：累计处理进度并计算推理耗时、采集延迟和待处理积压
   */
  InferenceMetrics Record(
      size_t new_frames, unsigned long long captured_samples,
      int queued_frames,
      const std::chrono::steady_clock::time_point& inference_start,
      const std::chrono::steady_clock::time_point& inference_end);

  /**
   * 函数名：FrameAudioSeconds
   * 输入：metrics 本轮指标，window_frame 窗口内帧下标，newest_start 新增区间起点
   * 输出：该帧在整段音频中的时间；不属于新增区间时返回 -1
   * 函数功能：将模型窗口内的唤醒位置换算为绝对音频时间
   */
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

/**
 * 函数名：PrintInferenceResult
 * 输入：metrics 性能指标，result 后处理结果，newest_start 新增区间起点，stats 时间换算对象
 * 输出：无
 * 函数功能：按统一格式输出本轮得分、音频进度和推理性能
 */
void PrintInferenceResult(const InferenceMetrics& metrics,
                          const WakeupResult& result, int newest_start,
                          const InferenceStats& stats);

}  // namespace wekws

#endif  // STREAM_INFERENCE_STATS_H_
