// Copyright (c) 2022 Zhendong Peng (pzd17@tsinghua.org.cn)
// Modifications Copyright (c) 2026 GengXin Zheng (xiaoxinzheng35@foxmail.com)
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0

#ifndef STREAM_AUDIO_RECORDER_H_
#define STREAM_AUDIO_RECORDER_H_

#include <signal.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace wekws {

class AudioRecorder {
 public:
  using AudioCallback =
      std::function<void(const std::vector<int16_t>& samples)>;
  using FinishedCallback = std::function<void()>;

  AudioRecorder(std::string device, int sample_rate, int chunk_samples);
  ~AudioRecorder();

  bool Open();
  void Start(volatile sig_atomic_t* exiting, AudioCallback audio_callback,
             FinishedCallback finished_callback);
  void Join();
  int Close();

  const std::string& device_name() const;
  unsigned long long captured_samples() const;
  unsigned int capture_overruns() const;
  bool failed() const;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

bool IsSafeAudioDevice(const std::string& device);
const char* DefaultAudioDevice();

}  // namespace wekws

#endif  // STREAM_AUDIO_RECORDER_H_
