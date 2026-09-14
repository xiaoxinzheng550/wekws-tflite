// Copyright (c) 2022 Zhendong Peng (pzd17@tsinghua.org.cn)
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

#include <signal.h>
#include <algorithm>
#include <chrono>
#include <exception>
#include <iostream>
#include <string>
#include <vector>

#include "frontend/feature_pipeline.h"
#include "kws/keyword_spotting.h"
#include "model/model_data.h"
#include "stream/audio_recorder.h"
#include "stream/inference_stats.h"
#include "stream/sliding_feature_window.h"
#include "stream/wakeup_notifier.h"
#include "stream/wakeup_postprocessor.h"
#include "utils/log.h"

namespace {

constexpr int kSampleRate = 16000;
constexpr int kCaptureChunkMs = 20;
constexpr int kSamplesPerChunk = kSampleRate * kCaptureChunkMs / 1000;
constexpr int kDefaultStrideFrames = 50;
constexpr float kMediumWakeupThreshold = 0.60f;
constexpr float kWakeupReleaseThreshold = 0.20f;
constexpr int kMediumHitsRequired = 2;
constexpr int kReleaseLowFramesRequired = 10;
constexpr const char* kDefaultWakeupAudio =
    "examples/test_audio/wozai.wav";

volatile sig_atomic_t g_exiting = 0;

struct StreamOptions {
  std::string audio_device = wekws::DefaultAudioDevice();
  std::string feature_type = "fbank";
  int feature_dim = 40;
  float threshold = 0.80f;
  int stride_frames = kDefaultStrideFrames;
  std::string wakeup_audio = kDefaultWakeupAudio;
};

/**
 * 函数名：HandleSignal
 * 输入：signal_number 操作系统信号编号
 * 输出：无
 * 函数功能：收到 SIGINT 或 SIGTERM 时设置全局退出标志
 */
void HandleSignal(int signal_number) {
  if (signal_number == SIGINT || signal_number == SIGTERM) g_exiting = 1;
}

/**
 * 函数名：PrintUsage
 * 输入：program 当前可执行程序名称
 * 输出：无
 * 函数功能：输出流式唤醒程序的命令行参数和默认值
 */
void PrintUsage(const char* program) {
  std::cerr << "Usage: " << program
            << " [alsa_device] [mfcc|fbank] [feature_dim] [threshold]"
               " [stride_frames] [wakeup_audio]\n"
            << "Defaults: audio_device=" << wekws::DefaultAudioDevice()
            << " feat_type=fbank feature_dim=40 threshold=0.80"
               " stride_frames=50 wakeup_audio="
            << kDefaultWakeupAudio << "\n";
}

/**
 * 函数名：ParseOptions
 * 输入：argc 参数数量，argv 参数数组，options 待写入的配置对象
 * 输出：全部参数合法返回 true，否则返回 false
 * 函数功能：解析并校验录音设备、特征、阈值、步长和提示音参数
 */
bool ParseOptions(int argc, char* argv[], StreamOptions* options) {
  if (options == nullptr || argc > 7) return false;
  try {
    if (argc > 1) options->audio_device = argv[1];
    if (argc > 2) options->feature_type = argv[2];
    if (argc > 3) options->feature_dim = std::stoi(argv[3]);
    if (argc > 4) options->threshold = std::stof(argv[4]);
    if (argc > 5) options->stride_frames = std::stoi(argv[5]);
    if (argc > 6) options->wakeup_audio = argv[6];
  } catch (const std::exception& error) {
    LOG(ERROR) << "Invalid command-line argument: " << error.what();
    return false;
  }

  return wekws::IsSafeAudioDevice(options->audio_device) &&
         (options->feature_type == "mfcc" ||
          options->feature_type == "fbank") &&
         options->feature_dim > 0 && options->threshold >= 0.0f &&
         options->threshold <= 1.0f && options->stride_frames > 0 &&
         !options->wakeup_audio.empty();
}

/**
 * 函数名：CreateFeatureConfig
 * 输入：options 已解析的流式运行配置
 * 输出：特征提取流水线配置
 * 函数功能：根据采样率、特征类型和维度创建前端配置
 */
wenet::FeaturePipelineConfig CreateFeatureConfig(
    const StreamOptions& options) {
  wenet::FeaturePipelineConfig config(options.feature_dim, kSampleRate);
  config.feat_type = options.feature_type;
  config.num_ceps = options.feature_dim;
  return config;
}

/**
 * 函数名：PrintConfiguration
 * 输入：options 运行配置，feature_config 特征配置，spotter 模型对象，recorder 录音器
 * 输出：无
 * 函数功能：输出录音、特征、滑窗、阈值和提示音等启动信息
 */
void PrintConfiguration(const StreamOptions& options,
                        const wenet::FeaturePipelineConfig& feature_config,
                        const wekws::KeywordSpotting& spotter,
                        const wekws::AudioRecorder& recorder) {
  LOG(INFO) << "Recording from " << recorder.device_name()
            << ": 16 kHz, mono, S16_LE";
  LOG(INFO) << "Capture chunk: " << kCaptureChunkMs << " ms ("
            << kSamplesPerChunk << " samples)";
  LOG(INFO) << "Audio preprocessing: raw PCM (no built-in AEC/NS/AGC)";
  LOG(INFO) << "Feature: " << options.feature_type << " "
            << options.feature_dim
            << " dim; keyword threshold: " << options.threshold;
  LOG(INFO) << "Sliding inference: window=" << spotter.fixed_frames()
            << " frames ("
            << spotter.fixed_frames() * feature_config.frame_shift * 1000 /
                   kSampleRate
            << " ms), stride=" << options.stride_frames << " frames ("
            << options.stride_frames * feature_config.frame_shift * 1000 /
                   kSampleRate
            << " ms)";
  LOG(INFO) << "Wakeup policy: high=" << options.threshold
            << ", medium=" << kMediumWakeupThreshold << " x "
            << kMediumHitsRequired << ", re-arm below="
            << kWakeupReleaseThreshold << " for at least "
            << kReleaseLowFramesRequired * feature_config.frame_shift * 1000 /
                   kSampleRate
            << " ms";
  LOG(INFO) << "Wakeup audio: " << options.wakeup_audio;
  LOG(INFO) << "Press Ctrl+C to stop.";
}

}  // namespace

/**
 * 函数名：main
 * 输入：argc 命令行参数数量，argv 命令行参数数组
 * 输出：0 表示正常退出，1 表示运行失败，2 表示参数或配置错误
 * 函数功能：组织实时录音、特征提取、滑窗推理、唤醒后处理和通知流程
 */
int main(int argc, char* argv[]) {
  // 解析并校验命令行配置。
  StreamOptions options;
  if (!ParseOptions(argc, argv, &options)) {
    PrintUsage(argv[0]);
    return 2;
  }
  if (options.threshold <= kWakeupReleaseThreshold) {
    LOG(ERROR) << "Wakeup threshold must exceed release threshold "
               << kWakeupReleaseThreshold;
    return 2;
  }

  // 初始化提示器、模型、特征流水线和录音设备。
  wekws::WakeupNotifier notifier(options.wakeup_audio);
  if (!notifier.Initialize()) return 2;

  wekws::KeywordSpotting spotter;
  if (!spotter.Init(g_model_data, g_model_data_len)) {
    LOG(ERROR) << "Failed to initialize KWS model";
    return 1;
  }
  LOG(INFO) << "Model loaded successfully";
  LOG(INFO) << "  Fixed frames per inference: " << spotter.fixed_frames();
  LOG(INFO) << "  Feature dim: " << spotter.feature_dim();
  if (options.feature_dim != spotter.feature_dim()) {
    LOG(ERROR) << "Feature dim " << options.feature_dim
               << " does not match model feature dim "
               << spotter.feature_dim();
    return 2;
  }
  if (options.stride_frames > spotter.fixed_frames()) {
    LOG(ERROR) << "Stride frames " << options.stride_frames
               << " exceeds model fixed frames " << spotter.fixed_frames();
    return 2;
  }

  const auto feature_config = CreateFeatureConfig(options);
  wenet::FeaturePipeline feature_pipeline(feature_config);
  wekws::AudioRecorder recorder(options.audio_device, kSampleRate,
                                kSamplesPerChunk);
  if (!recorder.Open()) return 1;

  // 注册退出信号并创建滑窗、后处理和性能统计对象。
  signal(SIGINT, HandleSignal);
  signal(SIGTERM, HandleSignal);
  PrintConfiguration(options, feature_config, spotter, recorder);

  wekws::SlidingFeatureWindow feature_window(spotter.fixed_frames(),
                                             options.feature_dim);
  wekws::WakeupPostprocessor postprocessor(
      options.threshold, kMediumWakeupThreshold, kMediumHitsRequired,
      kWakeupReleaseThreshold, kReleaseLowFramesRequired);
  wekws::InferenceStats stats(kSampleRate, feature_config.frame_length,
                              feature_config.frame_shift);

  // 启动录音线程，将采集到的 PCM 连续送入特征流水线。
  recorder.Start(
      &g_exiting,
      [&](const std::vector<int16_t>& pcm) {
        feature_pipeline.AcceptWaveform(pcm);
      },
      [&]() { feature_pipeline.set_input_finished(); });

  // 按固定步长读取新特征，执行滑窗推理和双唤醒词判断。
  while (!g_exiting) {
    std::vector<std::vector<float>> new_features;
    if (!feature_pipeline.Read(options.stride_frames, &new_features)) break;
    const size_t new_frame_count = new_features.size();
    if (!feature_window.Push(&new_features)) {
      LOG(ERROR) << "Invalid feature block for sliding window";
      g_exiting = 1;
      break;
    }

    std::vector<std::vector<float>> probabilities;
    const auto inference_start = std::chrono::steady_clock::now();
    // Each overlapping window already contains its acoustic history. Reusing
    // the model cache here would feed the overlapping history twice.
    spotter.Reset();
    spotter.Forward(feature_window.features(), &probabilities);
    const auto inference_end = std::chrono::steady_clock::now();

    const int output_begin = feature_window.newest_start();
    const int output_end = std::min(
        spotter.fixed_frames(), static_cast<int>(probabilities.size()));
    const wekws::WakeupResult result =
        postprocessor.Process(probabilities, output_begin, output_end);
    const wekws::InferenceMetrics metrics = stats.Record(
        new_frame_count, recorder.captured_samples(),
        feature_pipeline.NumQueuedFrames(), inference_start, inference_end);
    wekws::PrintInferenceResult(metrics, result, output_begin, stats);

    if (result.rearmed) {
      LOG(INFO) << "Wakeup detector re-armed: newest "
                << kReleaseLowFramesRequired << " frames are below "
                << kWakeupReleaseThreshold;
    }
    if (!result.triggered()) continue;

    // 唤醒成功后输出事件信息并执行提示音和 LED 通知。
    const size_t keyword = static_cast<size_t>(result.triggered_keyword);
    const auto& wake_word = wekws::WakeWords()[keyword];
    const double keyword_time = stats.FrameAudioSeconds(
        metrics, result.frames[keyword], output_begin);
    std::cout << "*** WAKEUP DETECTED *** keyword=" << wake_word.name
              << " class=" << wake_word.class_index
              << " score=" << result.scores[keyword]
              << " keyword_time=" << keyword_time << "s"
              << " processed_audio=" << metrics.processed_audio_seconds << "s"
              << " trigger=" << result.trigger_reason << std::endl;
    notifier.Notify();
  }

  // 等待异步任务结束并释放录音、播放等平台资源。
  g_exiting = 1;
  recorder.Join();
  notifier.Wait();
  const int recorder_status = recorder.Close();
  LOG(INFO) << "Stopped. Captured "
            << static_cast<double>(recorder.captured_samples()) / kSampleRate
            << " seconds; capture overruns=" << recorder.capture_overruns()
            << "; recorder status=" << recorder_status;
  return recorder_status == -1 || recorder.failed() ? 1 : 0;
}
