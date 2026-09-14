// Copyright (c) 2022 Zhendong Peng (pzd17@tsinghua.org.cn)
// Modifications Copyright (c) 2026 GengXin Zheng (xiaoxinzheng35@foxmail.com)
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0

#include "stream/sliding_feature_window.h"

#include <utility>

namespace wekws {

SlidingFeatureWindow::SlidingFeatureWindow(int window_frames, int feature_dim)
    : window_frames_(window_frames),
      feature_dim_(feature_dim),
      ring_(window_frames, std::vector<float>(feature_dim, 0.0f)),
      window_(window_frames, std::vector<float>(feature_dim, 0.0f)) {}

bool SlidingFeatureWindow::Push(
    std::vector<std::vector<float>>* new_features) {
  if (new_features == nullptr || new_features->empty() ||
      new_features->size() > static_cast<size_t>(window_frames_)) {
    return false;
  }

  for (auto& feature : *new_features) {
    if (feature.size() != static_cast<size_t>(feature_dim_)) return false;
    ring_[write_index_] = std::move(feature);
    write_index_ = (write_index_ + 1) % ring_.size();
  }
  newest_frames_ = static_cast<int>(new_features->size());

  for (int frame = 0; frame < window_frames_; ++frame) {
    const size_t index = (write_index_ + frame) % ring_.size();
    window_[frame] = ring_[index];
  }
  return true;
}

}  // namespace wekws
