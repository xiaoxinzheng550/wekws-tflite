// Copyright (c) 2022 Zhendong Peng (pzd17@tsinghua.org.cn)
// Modifications Copyright (c) 2026 GengXin Zheng (xiaoxinzheng35@foxmail.com)
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0

#include "stream/audio_recorder.h"

#include <atomic>
#include <cerrno>
#include <cstdio>
#include <thread>
#include <utility>

#include "utils/log.h"

#if defined(__APPLE__)
#include "portaudio.h"  // NOLINT
#endif

namespace wekws {

class AudioRecorder::Impl {
 public:
  Impl(std::string device, int sample_rate, int chunk_samples)
      : device_(std::move(device)),
        device_name_(device_),
        sample_rate_(sample_rate),
        chunk_samples_(chunk_samples) {}

  bool Open() {
#if defined(__APPLE__)
    PaError error = Pa_Initialize();
    if (error != paNoError) {
      LOG(ERROR) << "Pa_Initialize failed: " << Pa_GetErrorText(error);
      return false;
    }
    portaudio_initialized_ = true;

    PaStreamParameters input_parameters{};
    input_parameters.device = Pa_GetDefaultInputDevice();
    if (input_parameters.device == paNoDevice) {
      LOG(ERROR) << "No macOS default input device";
      return false;
    }
    input_parameters.channelCount = 1;
    input_parameters.sampleFormat = paInt16;
    input_parameters.suggestedLatency =
        Pa_GetDeviceInfo(input_parameters.device)->defaultLowInputLatency;
    error = Pa_OpenStream(&stream_, &input_parameters, nullptr, sample_rate_,
                          chunk_samples_, paClipOff, nullptr, nullptr);
    if (error == paNoError) error = Pa_StartStream(stream_);
    if (error != paNoError) {
      LOG(ERROR) << "Failed to start macOS microphone: "
                 << Pa_GetErrorText(error);
      return false;
    }
    device_name_ = Pa_GetDeviceInfo(input_parameters.device)->name;
#else
    const std::string command = "exec arecord -D " + device_ +
                                " -t raw -f S16_LE -r " +
                                std::to_string(sample_rate_) + " -c 1";
    recorder_ = popen(command.c_str(), "r");
    if (recorder_ == nullptr) {
      LOG(ERROR) << "Failed to start arecord for " << device_;
      return false;
    }
#endif
    opened_ = true;
    return true;
  }

  void Start(volatile sig_atomic_t* exiting, AudioCallback audio_callback,
             FinishedCallback finished_callback) {
    exiting_ = exiting;
    capture_thread_ = std::thread(
        [this, audio_callback, finished_callback]() {
          std::vector<int16_t> pcm(chunk_samples_);
          while (!*exiting_) {
            size_t samples_read = 0;
#if defined(__APPLE__)
            const PaError error =
                Pa_ReadStream(stream_, pcm.data(), chunk_samples_);
            if (error == paInputOverflowed) {
              ++capture_overruns_;
            } else if (error != paNoError) {
              LOG(ERROR) << "Microphone read failed: "
                         << Pa_GetErrorText(error);
              failed_ = true;
              *exiting_ = 1;
              break;
            }
            samples_read = chunk_samples_;
#else
            while (samples_read < pcm.size() && !*exiting_) {
              const size_t count =
                  fread(pcm.data() + samples_read, sizeof(int16_t),
                        pcm.size() - samples_read, recorder_);
              if (count > 0) {
                samples_read += count;
                continue;
              }
              if (ferror(recorder_) && errno == EINTR) {
                clearerr(recorder_);
                continue;
              }
              if (!*exiting_) {
                LOG(ERROR) << "arecord stopped while capturing audio";
                failed_ = true;
                *exiting_ = 1;
              }
              break;
            }
#endif
            if (samples_read == 0) break;
            if (samples_read == pcm.size()) {
              audio_callback(pcm);
            } else {
              audio_callback(std::vector<int16_t>(
                  pcm.begin(), pcm.begin() + samples_read));
            }
            captured_samples_.fetch_add(samples_read,
                                        std::memory_order_relaxed);
          }
          finished_callback();
        });
  }

  void Join() {
    if (capture_thread_.joinable()) capture_thread_.join();
  }

  int Close() {
    if (!opened_ && !portaudio_initialized_) return 0;
#if defined(__APPLE__)
    if (stream_ != nullptr) {
      Pa_StopStream(stream_);
      Pa_CloseStream(stream_);
      stream_ = nullptr;
    }
    if (portaudio_initialized_) {
      Pa_Terminate();
      portaudio_initialized_ = false;
    }
    opened_ = false;
    return 0;
#else
    const int status = recorder_ == nullptr ? 0 : pclose(recorder_);
    recorder_ = nullptr;
    opened_ = false;
    return status;
#endif
  }

  std::string device_;
  std::string device_name_;
  int sample_rate_;
  int chunk_samples_;
  bool opened_ = false;
  volatile sig_atomic_t* exiting_ = nullptr;
  std::thread capture_thread_;
  std::atomic<unsigned long long> captured_samples_{0};
  std::atomic<unsigned int> capture_overruns_{0};
  std::atomic<bool> failed_{false};
#if defined(__APPLE__)
  PaStream* stream_ = nullptr;
  bool portaudio_initialized_ = false;
#else
  FILE* recorder_ = nullptr;
  const bool portaudio_initialized_ = false;
#endif
};

AudioRecorder::AudioRecorder(std::string device, int sample_rate,
                             int chunk_samples)
    : impl_(new Impl(std::move(device), sample_rate, chunk_samples)) {}

AudioRecorder::~AudioRecorder() { impl_->Close(); }

bool AudioRecorder::Open() { return impl_->Open(); }

void AudioRecorder::Start(volatile sig_atomic_t* exiting,
                          AudioCallback audio_callback,
                          FinishedCallback finished_callback) {
  impl_->Start(exiting, std::move(audio_callback),
               std::move(finished_callback));
}

void AudioRecorder::Join() { impl_->Join(); }

int AudioRecorder::Close() { return impl_->Close(); }

const std::string& AudioRecorder::device_name() const {
  return impl_->device_name_;
}

unsigned long long AudioRecorder::captured_samples() const {
  return impl_->captured_samples_.load(std::memory_order_relaxed);
}

unsigned int AudioRecorder::capture_overruns() const {
  return impl_->capture_overruns_.load(std::memory_order_relaxed);
}

bool AudioRecorder::failed() const {
  return impl_->failed_.load(std::memory_order_relaxed);
}

bool IsSafeAudioDevice(const std::string& device) {
  if (device.empty()) return false;
  for (char character : device) {
    const bool valid =
        (character >= 'a' && character <= 'z') ||
        (character >= 'A' && character <= 'Z') ||
        (character >= '0' && character <= '9') || character == '_' ||
        character == '-' || character == ':' || character == ',' ||
        character == '.';
    if (!valid) return false;
  }
  return true;
}

const char* DefaultAudioDevice() {
#if defined(__APPLE__)
  return "default";
#else
  return "plughw:C170,0";
#endif
}

}  // namespace wekws
