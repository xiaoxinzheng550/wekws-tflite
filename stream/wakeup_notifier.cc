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

/**
 * 函数名：WriteLedFile
 * 输入：path LED sysfs 文件路径，value 待写入的控制值
 * 输出：写入成功返回 true，失败返回 false
 * 函数功能：向 Linux LED 控制文件写入触发方式或亮度
 */
bool WriteLedFile(const char* path, const std::string& value) {
  std::ofstream file(path);
  if (!file.is_open()) return false;
  file << value;
  file.flush();
  return file.good();
}

/**
 * 函数名：ToggleLed
 * 输入：无
 * 输出：无
 * 函数功能：在 Linux 平台读取当前亮度并切换板载 LED 状态
 */
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

/**
 * 函数名：PlayAudio
 * 输入：audio_path 待播放的提示音路径
 * 输出：播放器正常退出返回 true，否则返回 false
 * 函数功能：调用 macOS afplay 或 Linux aplay 同步播放提示音
 */
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
  /**
   * 函数名：Impl
   * 输入：audio_path 唤醒提示音文件路径
   * 输出：构造完成的平台通知实现
   * 函数功能：保存提示音路径并初始化通知状态
   */
  explicit Impl(std::string audio_path) : audio_path_(std::move(audio_path)) {}

  /**
   * 函数名：Initialize
   * 输入：无
   * 输出：提示音可用返回 true，否则返回 false
   * 函数功能：检查提示音并在 Linux 平台关闭 LED 默认触发器
   */
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

  /**
   * 函数名：Notify
   * 输入：无
   * 输出：无
   * 函数功能：启动异步提示音播放，并在 Linux 平台切换 LED
   */
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

  /**
   * 函数名：Wait
   * 输入：无
   * 输出：无
   * 函数功能：等待提示音播放线程退出
   */
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
