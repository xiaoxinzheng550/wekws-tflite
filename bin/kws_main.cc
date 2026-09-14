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

#include <iostream>
#include <string>

#include "frontend/feature_pipeline.h"
#include "frontend/wav.h"
#include "kws/keyword_spotting.h"
#include "model/model_data.h"
#include "utils/log.h"

#include <chrono>

int main(int argc, char* argv[]) {
  if (argc != 4 && argc != 5) {
    LOG(FATAL) << "Usage: kws_main [mfcc|fbank] feature_dim(int) "
               << "batch_size(int) test_wav_path";
    LOG(INFO) << "Note: Model is embedded in the binary (model_data.cc)";
  }

  // 三参数形式兼容原命令，默认匹配当前嵌入的 DS-TCN/Fbank 模型。
  const std::string feat_type = argc == 5 ? argv[1] : "fbank";
  const int arg_offset = argc == 5 ? 1 : 0;
  const int feature_dim = std::stoi(argv[1 + arg_offset]);
  const int batch_size = std::stoi(argv[2 + arg_offset]);
  const std::string wav_path = argv[3 + arg_offset];
  if (feat_type != "mfcc" && feat_type != "fbank") {
    LOG(FATAL) << "Unsupported feature type: " << feat_type;
  }

  // 初始化 KWS 模型 (从嵌入的模型数据加载)
  wekws::KeywordSpotting spotter;
  if (!spotter.Init(g_model_data, g_model_data_len)) {
    LOG(FATAL) << "Failed to initialize KWS model";
  }

  LOG(INFO) << "Model loaded successfully";
  LOG(INFO) << "  Fixed frames per inference: " << spotter.fixed_frames();
  LOG(INFO) << "  Feature dim: " << spotter.feature_dim();
  LOG(INFO) << "  Cache dim: " << spotter.cache_dim();
  LOG(INFO) << "  Cache len: " << spotter.cache_len();
  if (feature_dim != spotter.feature_dim()) {
    LOG(ERROR) << "Feature dim " << feature_dim
               << " does not match model feature dim "
               << spotter.feature_dim();
    return 2;
  }

  // 读取 WAV 文件
  auto wav_start = std::chrono::high_resolution_clock::now();
  wenet::WavReader wav_reader(wav_path);
  int num_samples = wav_reader.num_samples();
  LOG(INFO) << "WAV file: " << wav_path;
  LOG(INFO) << "  Sample rate: " << wav_reader.sample_rate();
  LOG(INFO) << "  Num samples: " << num_samples;
  LOG(INFO) << "  Duration: " << static_cast<float>(num_samples) / wav_reader.sample_rate() << "s";

  // 特征提取配置
  wenet::FeaturePipelineConfig feature_config(feature_dim, 16000);
  feature_config.feat_type = feat_type;
  feature_config.num_ceps = feature_dim;
  wenet::FeaturePipeline feature_pipeline(feature_config);

  std::vector<float> wav(wav_reader.data(), wav_reader.data() + num_samples);
  feature_pipeline.AcceptWaveform(wav);
  feature_pipeline.set_input_finished();
  auto wav_end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double, std::milli> wav_duration = wav_end - wav_start;
  LOG(INFO) << "WAV load & Feature Extraction time: " << wav_duration.count() << " ms";

  // 流式检测，按批次处理
  int offset = 0;
  double total_inference_time = 0.0;
  int num_batches = 0;
  while (true) {
    std::vector<std::vector<float>> feats;
    bool ok = feature_pipeline.Read(batch_size, &feats);

    if (feats.size() == 0) {
      if (!ok) break;
      continue;
    }

    std::vector<std::vector<float>> prob;
    auto start = std::chrono::high_resolution_clock::now();
    spotter.Forward(feats, &prob);
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;
    total_inference_time += duration.count();
    num_batches++;

    std::cout << "batch " << num_batches << " (size: " << feats.size()
              << ") inference time: " << duration.count() << " ms" << std::endl;

    for (int i = 0; i < prob.size(); i++) {
      std::cout << "frame " << offset + i << " prob";
      for (int j = 0; j < prob[i].size(); j++) {
        std::cout << " " << prob[i][j];
      }
      std::cout << std::endl;
    }

    offset += feats.size();

    // 特征提取完成
    if (!ok) break;
  }

  LOG(INFO) << "Processing completed, total frames: " << offset;
  if (num_batches > 0) {
    LOG(INFO) << "=== Performance Summary ===";
    LOG(INFO) << "Total inference time: " << total_inference_time << " ms";
    LOG(INFO) << "Average inference time per batch: " << total_inference_time / num_batches << " ms";
    LOG(INFO) << "Average inference time per frame: " << total_inference_time / offset << " ms";
  }
  return 0;
}
