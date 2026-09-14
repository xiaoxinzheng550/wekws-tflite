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

/**
 * 函数名：WakeWords
 * 输入：无
 * 输出：唤醒词名称与模型输出类别下标的映射表
 * 函数功能：提供项目当前支持的双唤醒词配置
 */
const std::array<WakeWordInfo, kNumWakeWords>& WakeWords();

struct WakeupResult {
  std::array<float, kNumWakeWords> scores{};
  std::array<int, kNumWakeWords> frames{{-1, -1}};
  int best_keyword = 0;
  int triggered_keyword = -1;
  const char* trigger_reason = nullptr;
  bool rearmed = false;

  /**
   * 函数名：triggered
   * 输入：无
   * 输出：本轮产生唤醒事件时返回 true
   * 函数功能：判断后处理结果是否已触发某个唤醒词
   */
  bool triggered() const { return triggered_keyword >= 0; }
};

// Converts frame-level probabilities into debounced wake-up events.
class WakeupPostprocessor {
 public:
  /**
   * 函数名：WakeupPostprocessor
   * 输入：high_threshold 高阈值，medium_threshold 中阈值，medium_hits_required 连续命中次数，release_threshold 释放阈值，release_low_frames_required 重新激活帧数
   * 输出：构造完成的唤醒后处理器对象
   * 函数功能：配置双唤醒词的触发、去抖和重新激活规则
   */
  WakeupPostprocessor(float high_threshold, float medium_threshold,
                      int medium_hits_required, float release_threshold,
                      int release_low_frames_required);

  /**
   * 函数名：Process
   * 输入：probabilities 模型逐帧输出，begin_frame 检查起点，end_frame 检查终点
   * 输出：双唤醒词得分、位置、触发类别及重新激活状态
   * 函数功能：在指定的新输出区间完成阈值判断、连续命中和重复触发抑制
   */
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
