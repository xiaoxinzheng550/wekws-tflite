// Copyright (c) 2022 Zhendong Peng (pzd17@tsinghua.org.cn)
// Modifications Copyright (c) 2026 GengXin Zheng (xiaoxinzheng35@foxmail.com)
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0

#ifndef STREAM_SLIDING_FEATURE_WINDOW_H_
#define STREAM_SLIDING_FEATURE_WINDOW_H_

#include <cstddef>
#include <vector>

namespace wekws {

// Retains the newest fixed-size feature window while new frames arrive in
// smaller strides.
class SlidingFeatureWindow {
 public:
  /**
   * 函数名：SlidingFeatureWindow
   * 输入：window_frames 固定窗口帧数，feature_dim 单帧特征维度
   * 输出：构造完成并以零初始化的滑动窗口对象
   * 函数功能：创建用于模型固定长度输入的环形特征缓存
   */
  SlidingFeatureWindow(int window_frames, int feature_dim);

  /**
   * 函数名：Push
   * 输入：new_features 本轮新增的特征帧，成功后其帧数据会被移动到缓存
   * 输出：特征尺寸合法返回 true，否则返回 false
   * 函数功能：追加新特征并生成按时间顺序排列的固定长度窗口
   */
  bool Push(std::vector<std::vector<float>>* new_features);

  /**
   * 函数名：GetFeatures
   * 输入：无
   * 输出：当前固定长度特征窗口的只读引用
   * 函数功能：为模型推理提供连续的特征输入
   */
  const std::vector<std::vector<float>>& GetFeatures() const {
    return window_;
  }

  /**
   * 函数名：newest_start
   * 输入：无
   * 输出：本轮新增特征在固定窗口中的起始帧下标
   * 函数功能：限定后处理仅检查本轮新增时间范围的输出
   */
  int newest_start() const { return window_frames_ - newest_frames_; }

 private:
  int window_frames_;
  int feature_dim_;
  int newest_frames_ = 0;
  size_t write_index_ = 0;
  std::vector<std::vector<float>> ring_;
  std::vector<std::vector<float>> window_;
};

}  // namespace wekws

#endif  // STREAM_SLIDING_FEATURE_WINDOW_H_
