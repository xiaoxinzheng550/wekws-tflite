// Copyright (c) 2022 Binbin Zhang (binbzha@qq.com)
// Modifications Copyright (c) 2026 GengXin Zheng (xiaoxinzheng35@foxmail.com)
// Modified by GengXin Zheng in 2026 for the TFLite Micro runtime.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef KWS_KEYWORD_SPOTTING_H_
#define KWS_KEYWORD_SPOTTING_H_

#include <vector>

#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"

namespace wekws {

// 模型输入固定帧数 (TFLite Micro 不支持动态形状)
// 如果需要支持不同帧数，需要重新导出模型或多次推理
constexpr int kMaxFrames = 100;  // 最大帧数

class KeywordSpotting {
 public:
  KeywordSpotting();
  ~KeywordSpotting() = default;

  // 初始化模型
  bool Init(const unsigned char* model_data, size_t model_size);

  // 重置缓存状态
  void Reset();

  // 前向推理
  // 注意: TFLite Micro 不支持动态输入形状
  // 输入帧数必须与模型导出时固定的一致
  void Forward(const std::vector<std::vector<float>>& feats,
               std::vector<std::vector<float>>* prob);

  // 获取模型信息
  int cache_dim() const { return cache_dim_; }
  int cache_len() const { return cache_len_; }
  int fixed_frames() const { return fixed_frames_; }
  int feature_dim() const { return feature_dim_; }

 private:
  // 内存分配
  static constexpr int kTensorArenaSize = 1.7 * 1024 * 1024;  // 2MB
  alignas(16) uint8_t tensor_arena_[kTensorArenaSize];

  // TFLite Micro 组件
  const tflite::Model* model_;
  tflite::MicroInterpreter* interpreter_;

  // 算子解析器 (根据模型需要注册算子)
  // 注册常用算子: CONV_2D, DEPTHWISE_CONV_2D, FULLY_CONNECTED, etc.
  using MicroOpResolver = tflite::MicroMutableOpResolver<20>;
  MicroOpResolver* op_resolver_;

  // 模型元信息
  int cache_dim_ = 0;
  int cache_len_ = 0;
  int fixed_frames_ = 1;  // 固定输入帧数
  int feature_dim_ = 80;  // 特征维度

  // 缓存数据 (静态分配)
  std::vector<int8_t> cache_;

  // 量化参数
  float input_scale_ = 1.0f;
  int32_t input_zero_point_ = 0;
  float output_scale_ = 1.0f;
  int32_t output_zero_point_ = 0;
  int32_t cache_zero_point_ = 0;
};

}  // namespace wekws

#endif  // KWS_KEYWORD_SPOTTING_H_
