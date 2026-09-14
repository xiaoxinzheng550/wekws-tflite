// Copyright (c) 2022 Zhendong Peng (pzd17@tsinghua.org.cn)
// Modifications Copyright (c) 2026 GengXin Zheng (xiaoxinzheng35@foxmail.com)
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0

#include "stream/wakeup_notifier.h"

#include <spawn.h>
#include <sys/wait.h>

#include <atomic>
#include <cerrno>
#include <fstream>
#include <thread>
#include <utility>

#include "utils/log.h"

extern char** environ;

namespace wekws {
namespace {

constexpr const char* kLedTriggerPath = "/sys/class/leds/sys-led/trigger";
constexpr const char* kLedBrightnessPath =
    "/sys/devices/platform/leds/leds/sys-led/brightness";

bool WriteLedFile(const char* path, const std::string& value) {
  std::ofstream file(path);
  if (!file.is_open()) return false;
  file << value;
  file.flush();
  return file.good();
}

void ToggleLed() {
#if !defined(__APPLE__)
  std::ifstream brightness_file(kLedBrightnessPath);
  int current_brightness = 0;
  if (!(brightness_file >> current_brightness)) {
    LOG(ERROR) << "Failed to read LED brightness from " << kLedBrightnessPath;
    return;
  }
  const bool led_on = current_brightness == 0;
  if (!WriteLedFile(kLedBrightnessPath, led_on ? "1" : "0")) {
    LOG(ERROR) << "Failed to update LED brightness";
    return;
  }
  LOG(INFO) << "LED toggled " << (led_on ? "on" : "off");
#endif
}

bool PlayAudio(const std::string& audio_path) {
#if defined(__APPLE__)
  const char* player = "afplay";
  char* const player_args[] = {
      const_cast<char*>(player), const_cast<char*>(audio_path.c_str()), nullptr};
#else
  const char* player = "aplay";
  char* const player_args[] = {const_cast<char*>(player),
                               const_cast<char*>("-q"),
                               const_cast<char*>(audio_path.c_str()), nullptr};
#endif

  pid_t player_pid = -1;
  const int spawn_status =
      posix_spawnp(&player_pid, player, nullptr, nullptr, player_args, environ);
  if (spawn_status != 0) {
    LOG(ERROR) << "Failed to start " << player << " for " << audio_path
               << ": error " << spawn_status;
    return false;
  }

  int player_status = 0;
  pid_t wait_result;
  do {
    wait_result = waitpid(player_pid, &player_status, 0);
  } while (wait_result == -1 && errno == EINTR);
  if (wait_result == -1 || !WIFEXITED(player_status) ||
      WEXITSTATUS(player_status) != 0) {
    LOG(ERROR) << player << " failed while playing " << audio_path;
    return false;
  }
  return true;
}

}  // namespace

class WakeupNotifier::Impl {
 public:
  explicit Impl(std::string audio_path) : audio_path_(std::move(audio_path)) {}

  bool Initialize() {
    std::ifstream audio_file(audio_path_, std::ios::binary);
    if (!audio_file.good()) {
      LOG(ERROR) << "Wakeup audio file not found: " << audio_path_;
      return false;
    }
#if !defined(__APPLE__)
    led_available_ = WriteLedFile(kLedTriggerPath, "none");
    if (!led_available_) {
      LOG(WARNING) << "LED control is unavailable; wake-word detection will "
                      "continue";
    }
#endif
    return true;
  }

  void Notify() {
    if (!playback_active_.exchange(true)) {
      if (playback_thread_.joinable()) playback_thread_.join();
      playback_thread_ = std::thread([this]() {
        PlayAudio(audio_path_);
        playback_active_.store(false);
      });
    } else {
      LOG(WARNING) << "Wakeup audio is already playing; skipping playback";
    }
#if !defined(__APPLE__)
    if (led_available_) ToggleLed();
#endif
  }

  void Wait() {
    if (playback_thread_.joinable()) playback_thread_.join();
  }

  std::string audio_path_;
  std::atomic<bool> playback_active_{false};
  std::thread playback_thread_;
  bool led_available_ = false;
};

WakeupNotifier::WakeupNotifier(std::string audio_path)
    : impl_(new Impl(std::move(audio_path))) {}

WakeupNotifier::~WakeupNotifier() { impl_->Wait(); }

bool WakeupNotifier::Initialize() { return impl_->Initialize(); }

void WakeupNotifier::Notify() { impl_->Notify(); }

void WakeupNotifier::Wait() { impl_->Wait(); }

}  // namespace wekws
