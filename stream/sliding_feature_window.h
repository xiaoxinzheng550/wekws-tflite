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
  SlidingFeatureWindow(int window_frames, int feature_dim);

  bool Push(std::vector<std::vector<float>>* new_features);
  const std::vector<std::vector<float>>& features() const { return window_; }
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
