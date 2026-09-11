// Copyright (c) 2022 Zhendong Peng (pzd17@tsinghua.org.cn)
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
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "frontend/feature_pipeline.h"
#include "kws/keyword_spotting.h"
#include "model/model_data.h"
#include "utils/log.h"

#if defined(__APPLE__)
#include "portaudio.h"  // NOLINT
#endif

namespace {

constexpr int kSampleRate = 16000;
constexpr int kCaptureChunkMs = 20;
constexpr int kSamplesPerChunk = kSampleRate * kCaptureChunkMs / 1000;
constexpr int kKeywordClass = 1;
constexpr int kDefaultStrideFrames = 50;
constexpr float kMediumWakeupThreshold = 0.60f;
constexpr float kWakeupReleaseThreshold = 0.20f;
constexpr int kMediumHitsRequired = 2;
// Release the wakeup latch only when the newest output ends with this many
// genuinely consecutive low-score frames (10 frames = 100 ms).
constexpr int kReleaseLowFramesRequired = 10;
constexpr const char* kLedTriggerPath =
    "/sys/class/leds/sys-led/trigger";
constexpr const char* kLedBrightnessPath =
    "/sys/devices/platform/leds/leds/sys-led/brightness";
#if defined(__APPLE__)
constexpr const char* kDefaultAudioDevice = "default";
#else
constexpr const char* kDefaultAudioDevice = "plughw:C170,0";
#endif

volatile sig_atomic_t g_exiting = 0;

void SigRoutine(int signal_number) {
  if (signal_number == SIGINT || signal_number == SIGTERM) g_exiting = 1;
}

bool IsSafeAlsaDevice(const std::string& device) {
  if (device.empty()) return false;
  for (char c : device) {
    const bool valid = (c >= 'a' && c <= 'z') ||
                       (c >= 'A' && c <= 'Z') ||
                       (c >= '0' && c <= '9') || c == '_' || c == '-' ||
                       c == ':' || c == ',' || c == '.';
    if (!valid) return false;
  }
  return true;
}

bool WriteLedFile(const char* path, const std::string& value) {
  std::ofstream file(path);
  if (!file.is_open()) {
    LOG(ERROR) << "Failed to open LED file " << path;
    return false;
  }
  file << value;
  file.flush();
  if (!file.good()) {
    LOG(ERROR) << "Failed to write '" << value << "' to " << path;
    return false;
  }
  return true;
}

bool ToggleLed(bool* led_on) {
  std::ifstream brightness_file(kLedBrightnessPath);
  int current_brightness = 0;
  if (!(brightness_file >> current_brightness)) {
    LOG(ERROR) << "Failed to read LED brightness from " << kLedBrightnessPath;
    return false;
  }

  const bool new_led_on = current_brightness == 0;
  if (!WriteLedFile(kLedBrightnessPath, new_led_on ? "1" : "0")) {
    return false;
  }
  *led_on = new_led_on;
  return true;
}

void PrintUsage(const char* program) {
  std::cerr << "Usage: " << program
            << " [alsa_device] [mfcc|fbank] [feature_dim] [threshold]"
               " [stride_frames]\n"
            << "Defaults: audio_device=" << kDefaultAudioDevice
            << " feat_type=mfcc "
               "feature_dim=80 threshold=0.80 stride_frames=50\n";
}

}  // namespace

int main(int argc, char* argv[]) {
  if (argc > 6) {
    PrintUsage(argv[0]);
    return 2;
  }

  const std::string audio_device = argc > 1 ? argv[1] : kDefaultAudioDevice;
  const std::string feat_type = argc > 2 ? argv[2] : "mfcc";
  const int feature_dim = argc > 3 ? std::stoi(argv[3]) : 80;
  const float threshold = argc > 4 ? std::stof(argv[4]) : 0.80f;
  const int stride_frames =
      argc > 5 ? std::stoi(argv[5]) : kDefaultStrideFrames;

  if (!IsSafeAlsaDevice(audio_device) ||
      (feat_type != "mfcc" && feat_type != "fbank") ||
      feature_dim <= 0 || threshold < 0.0f || threshold > 1.0f ||
      stride_frames <= 0) {
    PrintUsage(argv[0]);
    return 2;
  }
  if (threshold <= kWakeupReleaseThreshold) {
    LOG(ERROR) << "Wakeup threshold " << threshold
               << " must be greater than release threshold "
               << kWakeupReleaseThreshold
               << "; otherwise the detector can repeatedly re-arm and "
                  "trigger at the same score";
    return 2;
  }

  wekws::KeywordSpotting spotter;
  if (!spotter.Init(g_model_data, g_model_data_len)) {
    LOG(ERROR) << "Failed to initialize KWS model";
    return 1;
  }

  LOG(INFO) << "Model loaded successfully";
  LOG(INFO) << "  Fixed frames per inference: " << spotter.fixed_frames();
  LOG(INFO) << "  Feature dim: " << spotter.feature_dim();
  if (feature_dim != spotter.feature_dim()) {
    LOG(ERROR) << "Feature dim " << feature_dim
               << " does not match model feature dim "
               << spotter.feature_dim();
    return 2;
  }
  if (stride_frames > spotter.fixed_frames()) {
    LOG(ERROR) << "Stride frames " << stride_frames
               << " exceeds model fixed frames " << spotter.fixed_frames();
    return 2;
  }

  wenet::FeaturePipelineConfig feature_config(feature_dim, kSampleRate);
  feature_config.feat_type = feat_type;
  feature_config.num_ceps = feature_dim;
  wenet::FeaturePipeline feature_pipeline(feature_config);

#if defined(__APPLE__)
  PaError pa_error = Pa_Initialize();
  if (pa_error != paNoError) {
    LOG(ERROR) << "Pa_Initialize failed: " << Pa_GetErrorText(pa_error);
    return 1;
  }
  PaStreamParameters input_parameters{};
  input_parameters.device = Pa_GetDefaultInputDevice();
  if (input_parameters.device == paNoDevice) {
    LOG(ERROR) << "No macOS default input device";
    Pa_Terminate();
    return 1;
  }
  input_parameters.channelCount = 1;
  input_parameters.sampleFormat = paInt16;
  input_parameters.suggestedLatency =
      Pa_GetDeviceInfo(input_parameters.device)->defaultLowInputLatency;
  PaStream* recorder = nullptr;
  pa_error = Pa_OpenStream(&recorder, &input_parameters, nullptr, kSampleRate,
                           kSamplesPerChunk, paClipOff, nullptr, nullptr);
  if (pa_error == paNoError) pa_error = Pa_StartStream(recorder);
  if (pa_error != paNoError) {
    LOG(ERROR) << "Failed to start macOS microphone: "
               << Pa_GetErrorText(pa_error);
    if (recorder != nullptr) Pa_CloseStream(recorder);
    Pa_Terminate();
    return 1;
  }
  const char* device_name = Pa_GetDeviceInfo(input_parameters.device)->name;
#else
  // arecord 已在目标板验证可用。-t raw 使 stdout 只包含 S16_LE PCM，
  // plughw 负责在摄像头原生采样率和模型需要的 16 kHz 之间转换。
  const std::string command =
      "exec arecord -D " + audio_device +
      " -t raw -f S16_LE -r 16000 -c 1";
  FILE* recorder = popen(command.c_str(), "r");
  if (recorder == nullptr) {
    LOG(ERROR) << "Failed to start arecord for " << audio_device;
    return 1;
  }
#endif

  signal(SIGINT, SigRoutine);
  signal(SIGTERM, SigRoutine);
#if !defined(__APPLE__)
  // Disable the factory heartbeat trigger so brightness can be controlled
  // directly, as in AppDemo::on_mid_pushButton_6_clicked().
  if (!WriteLedFile(kLedTriggerPath, "none")) {
    LOG(WARNING) << "LED control is unavailable; wake-word detection will "
                    "continue";
  }
#endif
#if defined(__APPLE__)
  LOG(INFO) << "Recording from macOS input " << device_name
            << ": 16 kHz, mono, S16_LE";
#else
  LOG(INFO) << "Recording from " << audio_device << ": 16 kHz, mono, S16_LE";
#endif
  LOG(INFO) << "Capture chunk: " << kCaptureChunkMs << " ms ("
            << kSamplesPerChunk << " samples)";
  LOG(INFO) << "Audio preprocessing: raw PCM (no built-in AEC/NS/AGC)";
  LOG(INFO) << "Feature: " << feat_type << " " << feature_dim
            << " dim; keyword threshold: " << threshold;
  LOG(INFO) << "Sliding inference: window=" << spotter.fixed_frames()
            << " frames ("
            << spotter.fixed_frames() * feature_config.frame_shift * 1000 /
                   kSampleRate
            << " ms), stride=" << stride_frames << " frames ("
            << stride_frames * feature_config.frame_shift * 1000 / kSampleRate
            << " ms)";
  LOG(INFO) << "Wakeup policy: high=" << threshold
            << ", medium=" << kMediumWakeupThreshold << " x "
            << kMediumHitsRequired << ", re-arm below="
            << kWakeupReleaseThreshold << " for at least "
            << kReleaseLowFramesRequired * feature_config.frame_shift * 1000 /
                   kSampleRate
            << " ms";
  LOG(INFO) << "Press Ctrl+C to stop.";

  std::atomic<unsigned long long> captured_samples{0};
  std::atomic<unsigned int> capture_overruns{0};
  std::atomic<bool> capture_failed{false};
  int inference_count = 0;
  bool wakeup_armed = true;
  int medium_hit_streak = 0;
  int consecutive_release_low_frames = 0;
  unsigned long long processed_frames = 0;
  const auto stream_start = std::chrono::steady_clock::now();

  // Keep audio capture independent from inference. On the target board one
  // 256-frame inference takes about one second; reading arecord in this
  // dedicated thread prevents its pipe/ALSA buffer from filling during that
  // time. FeaturePipeline's BlockingQueue provides the producer-consumer
  // boundary between this capture thread and the inference thread below.
  std::thread capture_thread([&]() {
    std::vector<int16_t> pcm(kSamplesPerChunk);
    while (!g_exiting) {
      size_t samples_read = 0;
#if defined(__APPLE__)
      const PaError read_error =
          Pa_ReadStream(recorder, pcm.data(), kSamplesPerChunk);
      if (read_error == paInputOverflowed) {
        ++capture_overruns;
      } else if (read_error != paNoError) {
        LOG(ERROR) << "Microphone read failed: "
                   << Pa_GetErrorText(read_error);
        capture_failed = true;
        g_exiting = 1;
        break;
      }
      samples_read = kSamplesPerChunk;
#else
      while (samples_read < pcm.size() && !g_exiting) {
        const size_t n = fread(pcm.data() + samples_read, sizeof(int16_t),
                               pcm.size() - samples_read, recorder);
        if (n > 0) {
          samples_read += n;
          continue;
        }
        if (ferror(recorder) && errno == EINTR) {
          clearerr(recorder);
          continue;
        }
        if (!g_exiting) {
          LOG(ERROR) << "arecord stopped while capturing audio";
          capture_failed = true;
          g_exiting = 1;
        }
        break;
      }
#endif
      if (samples_read == 0) break;

      if (samples_read == pcm.size()) {
        feature_pipeline.AcceptWaveform(pcm);
      } else {
        std::vector<int16_t> partial_pcm(pcm.begin(),
                                         pcm.begin() + samples_read);
        feature_pipeline.AcceptWaveform(partial_pcm);
      }
      captured_samples.fetch_add(samples_read, std::memory_order_relaxed);
    }
    feature_pipeline.set_input_finished();
  });

  const int window_frames = spotter.fixed_frames();
  // A fixed-capacity circular buffer retains the most recent model window.
  // New frames overwrite the same number of oldest frames; inference never
  // drains or clears this history.
  std::vector<std::vector<float>> feature_ring(
      window_frames, std::vector<float>(feature_dim, 0.0f));
  // The ring starts as 256 frames of silence. This permits the first
  // inference after one stride instead of imposing a 2.56-second startup
  // delay. ring_write_index always identifies the oldest frame to overwrite.
  size_t ring_write_index = 0;
  while (!g_exiting) {
    std::vector<std::vector<float>> new_feats;
    if (!feature_pipeline.Read(stride_frames, &new_feats)) break;
    processed_frames += new_feats.size();

    for (auto& feat : new_feats) {
      feature_ring[ring_write_index] = std::move(feat);
      ring_write_index = (ring_write_index + 1) % window_frames;
    }

    // Materialize oldest -> newest order for the fixed-shape model input.
    std::vector<std::vector<float>> feature_window;
    feature_window.reserve(window_frames);
    for (int i = 0; i < window_frames; ++i) {
      const size_t index = (ring_write_index + i) % window_frames;
      feature_window.push_back(feature_ring[index]);
    }
    const int output_start =
        window_frames - static_cast<int>(new_feats.size());
    const int output_end_expected = window_frames;

    std::vector<std::vector<float>> probabilities;
    const auto inference_start = std::chrono::steady_clock::now();
    // The overlapping window already contains all required history. Keeping
    // cache across overlapping windows would feed that history twice.
    spotter.Reset();
    spotter.Forward(feature_window, &probabilities);
    const auto inference_end = std::chrono::steady_clock::now();
    ++inference_count;

    float best_keyword_score = 0.0f;
    int best_frame = -1;
    const int output_end = std::min(
        output_end_expected, static_cast<int>(probabilities.size()));
    for (int frame = output_start; frame < output_end; ++frame) {
      if (probabilities[frame].size() > kKeywordClass &&
          probabilities[frame][kKeywordClass] > best_keyword_score) {
        best_keyword_score = probabilities[frame][kKeywordClass];
        best_frame = static_cast<int>(frame);
      }
    }

    const double processed_audio_seconds =
        static_cast<double>(feature_config.frame_length +
                            (processed_frames - 1) * feature_config.frame_shift) /
        kSampleRate;
    const double captured_audio_seconds =
        static_cast<double>(
            captured_samples.load(std::memory_order_relaxed)) /
        kSampleRate;
    const double wall_seconds =
        std::chrono::duration<double>(inference_end - stream_start).count();
    const double capture_lag_ms =
        std::max(0.0, (wall_seconds - captured_audio_seconds) * 1000.0);
    const double backlog_ms =
        std::max(0.0,
                 (captured_audio_seconds - processed_audio_seconds) * 1000.0);
    const double inference_ms =
        std::chrono::duration<double, std::milli>(inference_end -
                                                  inference_start)
            .count();
    const int best_new_frame =
        best_frame >= output_start ? best_frame - output_start : -1;
    const unsigned long long first_new_absolute_frame =
        processed_frames - new_feats.size();
    const double keyword_audio_seconds =
        best_new_frame >= 0
            ? static_cast<double>(feature_config.frame_length +
                                  (first_new_absolute_frame + best_new_frame) *
                                      feature_config.frame_shift) /
                  kSampleRate
            : -1.0;
    std::cout << "inference=" << inference_count
              << " audio_time=" << processed_audio_seconds << "s"
              << " captured_audio=" << captured_audio_seconds << "s"
              << " capture_lag=" << capture_lag_ms << "ms"
              << " backlog=" << backlog_ms << "ms"
              << " queued_frames=" << feature_pipeline.NumQueuedFrames()
              << " keyword_score=" << best_keyword_score
              << " new_frame=" << best_new_frame
              << " keyword_time=" << keyword_audio_seconds << "s"
              << " inference_time=" << inference_ms << "ms" << std::endl;

    bool should_trigger = false;
    const char* trigger_reason = nullptr;
    if (!wakeup_armed) {
      // Check every newly produced model output in chronological order. Do
      // not approximate 60 outputs using their maximum: that used to turn one
      // low-scoring inference directly into 60 "low frames".
      for (int frame = output_start; frame < output_end; ++frame) {
        float frame_score = 0.0f;
        if (probabilities[frame].size() > kKeywordClass) {
          frame_score = probabilities[frame][kKeywordClass];
        }
        if (frame_score < kWakeupReleaseThreshold) {
          ++consecutive_release_low_frames;
        } else {
          consecutive_release_low_frames = 0;
        }
      }
      if (consecutive_release_low_frames >= kReleaseLowFramesRequired) {
        // Re-arm only for the next inference. Even if this 60-frame batch
        // contains an earlier low valley and another peak, it cannot trigger
        // twice inside one inference.
        wakeup_armed = true;
        medium_hit_streak = 0;
        consecutive_release_low_frames = 0;
        LOG(INFO) << "Wakeup detector re-armed: newest "
                  << kReleaseLowFramesRequired << " frames are below "
                  << kWakeupReleaseThreshold;
      }
    } else if (best_keyword_score >= threshold) {
      should_trigger = true;
      trigger_reason = "high";
    } else if (best_keyword_score >= kMediumWakeupThreshold) {
      ++medium_hit_streak;
      if (medium_hit_streak >= kMediumHitsRequired) {
        should_trigger = true;
        trigger_reason = "medium_x2";
      }
    } else {
      medium_hit_streak = 0;
    }

    if (should_trigger) {
      wakeup_armed = false;
      medium_hit_streak = 0;
      consecutive_release_low_frames = 0;
      std::cout << "*** WAKEUP DETECTED *** score=" << best_keyword_score
                << " keyword_time=" << keyword_audio_seconds << "s"
                << " processed_audio=" << processed_audio_seconds << "s"
                << " trigger=" << trigger_reason
                << std::endl;
#if !defined(__APPLE__)
      bool led_on = false;
      if (ToggleLed(&led_on)) {
        LOG(INFO) << "LED toggled " << (led_on ? "on" : "off");
      }
#endif
    }
  }

  g_exiting = 1;
  capture_thread.join();

#if defined(__APPLE__)
  Pa_StopStream(recorder);
  Pa_CloseStream(recorder);
  Pa_Terminate();
  const int recorder_status = 0;
#else
  const int recorder_status = pclose(recorder);
#endif
  LOG(INFO) << "Stopped. Captured "
            << static_cast<double>(
                   captured_samples.load(std::memory_order_relaxed)) /
                   kSampleRate
            << " seconds; capture overruns=" << capture_overruns.load()
            << "; arecord status=" << recorder_status;
  return recorder_status == -1 || capture_failed ? 1 : 0;
}
