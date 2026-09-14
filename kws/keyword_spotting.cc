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

#include "kws/keyword_spotting.h"

#include <cstring>
#include <iostream>

namespace wekws {

KeywordSpotting::KeywordSpotting()
    : model_(nullptr),
      interpreter_(nullptr),
      op_resolver_(nullptr) {}

bool KeywordSpotting::Init(const unsigned char* model_data, size_t model_size) {
  // 1. 加载模型
  model_ = tflite::GetModel(model_data);
  if (model_->version() != TFLITE_SCHEMA_VERSION) {
    std::cerr << "Model version " << model_->version()
              << " not equal to supported version " << TFLITE_SCHEMA_VERSION
              << std::endl;
    return false;
  }

  // 2. 创建算子解析器
  op_resolver_ = new MicroOpResolver();
  op_resolver_->AddConv2D();
  op_resolver_->AddDepthwiseConv2D();
  op_resolver_->AddFullyConnected();
  op_resolver_->AddAdd();
  op_resolver_->AddSub();
  op_resolver_->AddMul();
  op_resolver_->AddRelu();
  op_resolver_->AddReshape();
  op_resolver_->AddTranspose();
  op_resolver_->AddQuantize();
  op_resolver_->AddDequantize();
  op_resolver_->AddCast();
  op_resolver_->AddSlice();
  op_resolver_->AddConcatenation();
  op_resolver_->AddLogistic();
  op_resolver_->AddRound();
  op_resolver_->AddStridedSlice();
  op_resolver_->AddGather();

  // 3. 创建解释器
  interpreter_ = new tflite::MicroInterpreter(
      model_, *op_resolver_, tensor_arena_, kTensorArenaSize);

  // 默认硬编码形状
  fixed_frames_ = 256;
  feature_dim_ = 40;
  cache_dim_ = 256;
  cache_len_ = 105;

  // 4. 分配张量内存
  TfLiteStatus allocate_status = interpreter_->AllocateTensors();
  if (allocate_status != kTfLiteOk) {
    std::cerr << "AllocateTensors() failed" << std::endl;
    return false;
  }

  // 5. 从张量中读取模型形状和量化参数
  TfLiteTensor* input_tensor = interpreter_->input(0);
  if (input_tensor != nullptr) {
    if (input_tensor->dims->size >= 3) {
      fixed_frames_ = input_tensor->dims->data[1];
      feature_dim_ = input_tensor->dims->data[2];
    }
    input_scale_ = input_tensor->params.scale;
    input_zero_point_ = input_tensor->params.zero_point;
  }

  TfLiteTensor* cache_tensor = interpreter_->input(1);
  if (cache_tensor != nullptr) {
    if (cache_tensor->dims->size >= 3) {
      cache_dim_ = cache_tensor->dims->data[1];
      cache_len_ = cache_tensor->dims->data[2];
    }
    cache_zero_point_ = cache_tensor->params.zero_point;
  }

  TfLiteTensor* out_0 = interpreter_->output(0);
  TfLiteTensor* out_1 = interpreter_->output(1);
  TfLiteTensor* output_tensor = nullptr;

  if (out_0 != nullptr && out_0->dims->size >= 3 && out_0->dims->data[2] == 2) {
    output_tensor = out_0;
  } else {
    output_tensor = out_1;
  }

  if (output_tensor != nullptr) {
    output_scale_ = output_tensor->params.scale;
    output_zero_point_ = output_tensor->params.zero_point;
  }

  size_t used_bytes = interpreter_->arena_used_bytes();
  std::cout << "Kws Model Info:" << std::endl
            << "\tfixed_frames: " << fixed_frames_ << std::endl
            << "\tfeature_dim: " << feature_dim_ << std::endl
            << "\tcache_dim: " << cache_dim_ << std::endl
            << "\tcache_len: " << cache_len_ << std::endl
            << "\tarena_used_bytes: " << used_bytes << std::endl
            << "\tinput_scale: " << input_scale_ << std::endl
            << "\tinput_zero_point: " << input_zero_point_ << std::endl
            << "\tcache_zero_point: " << cache_zero_point_ << std::endl
            << "\toutput_scale: " << output_scale_ << std::endl
            << "\toutput_zero_point: " << output_zero_point_ << std::endl;

  std::cout << "Tensor info:" << std::endl;
  std::cout << "\tinputs:" << interpreter_->inputs_size() << std::endl;
  std::cout << "\toutputs:" << interpreter_->outputs_size() << std::endl;
  if (input_tensor) std::cout << "\tinput[0] type:" << input_tensor->type << std::endl;
  if (cache_tensor) std::cout << "\tinput[1] type:" << cache_tensor->type << std::endl;
  if (out_0) std::cout << "\toutput[0] type:" << out_0->type << std::endl;
  if (out_1) std::cout << "\toutput[1] type:" << out_1->type << std::endl;

  Reset();
  return true;
}

void KeywordSpotting::Reset() {
  TfLiteTensor* cache_tensor = interpreter_->input(1);
  if (cache_tensor != nullptr && cache_tensor->type == kTfLiteFloat32) {
    cache_.assign(cache_dim_ * cache_len_ * sizeof(float), 0);
  } else {
    cache_.assign(cache_dim_ * cache_len_ * sizeof(int8_t),
                  static_cast<int8_t>(cache_zero_point_));
  }
}

void KeywordSpotting::Forward(const std::vector<std::vector<float>>& feats,
                              std::vector<std::vector<float>>* prob) {
  prob->clear();
  if (feats.size() == 0) return;

  int num_frames = feats.size();

  if (feats[0].size() != static_cast<size_t>(feature_dim_)) {
    std::cerr << "Error: Input feature dim " << feats[0].size()
              << " != model feature dim " << feature_dim_ << std::endl;
    return;
  }

  if (num_frames != fixed_frames_) {
    std::cerr << "Warning: Input frames " << num_frames
              << " != fixed frames " << fixed_frames_ << std::endl;
  }

  // 1. 准备输入特征，将 2D 特征展平，不足的帧补 0.0f
  std::vector<float> flat_feats(fixed_frames_ * feature_dim_, 0.0f);
  int copy_frames = std::min(num_frames, fixed_frames_);
  for (int i = 0; i < copy_frames; i++) {
    int copy_dim = std::min((int)feats[i].size(), feature_dim_);
    memcpy(flat_feats.data() + i * feature_dim_,
           feats[i].data(),
           copy_dim * sizeof(float));
  }

  float min_val = flat_feats[0], max_val = flat_feats[0];
  for (auto v : flat_feats) {
    min_val = std::min(min_val, v);
    max_val = std::max(max_val, v);
  }
  std::cout << "Input feature range: [" << min_val << ", " << max_val << "]" << std::endl;

  // 2. 将特征拷贝/量化到输入张量
  TfLiteTensor* input_tensor = interpreter_->input(0);
  if (input_tensor->type == kTfLiteFloat32) {
    float* input = input_tensor->data.f;
    if (input == nullptr) {
      std::cerr << "Failed to get float input tensor" << std::endl;
      return;
    }
    memcpy(input, flat_feats.data(), flat_feats.size() * sizeof(float));
  } else {
    int8_t* input = input_tensor->data.int8;
    if (input == nullptr) {
      std::cerr << "Failed to get int8 input tensor" << std::endl;
      return;
    }
    for (size_t i = 0; i < flat_feats.size(); i++) {
      int32_t qval = std::round(flat_feats[i] / input_scale_) + input_zero_point_;
      if (qval < -128) qval = -128;
      if (qval > 127) qval = 127;
      input[i] = static_cast<int8_t>(qval);
    }
  }

  // 3. 复制缓存到输入张量
  TfLiteTensor* cache_input_tensor = interpreter_->input(1);
  if (cache_input_tensor->type == kTfLiteFloat32) {
    float* cache_input = cache_input_tensor->data.f;
    if (cache_input == nullptr) {
      std::cerr << "Failed to get float cache input tensor" << std::endl;
      return;
    }
    memcpy(cache_input, cache_.data(), cache_.size());
  } else {
    int8_t* cache_input = cache_input_tensor->data.int8;
    if (cache_input == nullptr) {
      std::cerr << "Failed to get int8 cache input tensor" << std::endl;
      return;
    }
    memcpy(cache_input, cache_.data(), cache_.size() * sizeof(int8_t));
  }

  // 4. 执行推理
  TfLiteStatus invoke_status = interpreter_->Invoke();
  if (invoke_status != kTfLiteOk) {
    std::cerr << "Invoke failed" << std::endl;
    return;
  }

  // 5. 动态检测输出：哪个是概率，哪个是缓存
  TfLiteTensor* out_0 = interpreter_->output(0);
  TfLiteTensor* out_1 = interpreter_->output(1);
  TfLiteTensor* output_tensor = nullptr;
  TfLiteTensor* cache_output_tensor = nullptr;

  if (out_0 != nullptr && out_0->dims->size >= 3 && out_0->dims->data[2] == 2) {
    output_tensor = out_0;
    cache_output_tensor = out_1;
  } else {
    output_tensor = out_1;
    cache_output_tensor = out_0;
  }

  // 6. 更新缓存
  if (cache_output_tensor->type == kTfLiteFloat32) {
    float* cache_output = cache_output_tensor->data.f;
    memcpy(cache_.data(), cache_output, cache_.size());
  } else {
    int8_t* cache_output = cache_output_tensor->data.int8;
    memcpy(cache_.data(), cache_output, cache_.size() * sizeof(int8_t));
  }

  // 7. 反量化/读取输出概率
  int output_frames = output_tensor->dims->data[1];
  int output_dim = output_tensor->dims->data[2];
  int valid_output_frames = std::min(num_frames, output_frames);
  prob->resize(valid_output_frames);

  if (output_tensor->type == kTfLiteFloat32) {
    float* output_data = output_tensor->data.f;
    for (int i = 0; i < valid_output_frames; i++) {
      (*prob)[i].resize(output_dim);
      for (int j = 0; j < output_dim; j++) {
        (*prob)[i][j] = output_data[i * output_dim + j];
      }
    }
  } else {
    int8_t* output_data = output_tensor->data.int8;
    for (int i = 0; i < valid_output_frames; i++) {
      (*prob)[i].resize(output_dim);
      for (int j = 0; j < output_dim; j++) {
        int8_t qval = output_data[i * output_dim + j];
        (*prob)[i][j] = (qval - output_zero_point_) * output_scale_;
      }
    }
  }
}

}  // namespace wekws

#include <cstdio>
#include <cstdarg>

#if defined(__GNUC__) || defined(__clang__)
extern "C" __attribute__((weak)) int DebugVsnprintf(
    char* buffer, size_t buf_size, const char* format, va_list vlist) {
#else
extern "C" int DebugVsnprintf(char* buffer, size_t buf_size,
                                const char* format, va_list vlist) {
#endif
  return vsnprintf(buffer, buf_size, format, vlist);
}
