// Copyright (c) 2022 Zhendong Peng (pzd17@tsinghua.org.cn)
// Modifications Copyright (c) 2026 GengXin Zheng (xiaoxinzheng35@foxmail.com)
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0

#ifndef STREAM_WAKEUP_NOTIFIER_H_
#define STREAM_WAKEUP_NOTIFIER_H_

#include <memory>
#include <string>

namespace wekws {

class WakeupNotifier {
 public:
  /**
   * 函数名：WakeupNotifier
   * 输入：audio_path 唤醒后播放的音频文件路径
   * 输出：构造完成的通知器对象
   * 函数功能：保存提示音配置并创建平台通知实现
   */
  explicit WakeupNotifier(std::string audio_path);

  /**
   * 函数名：~WakeupNotifier
   * 输入：无
   * 输出：无
   * 函数功能：等待提示音播放结束并释放线程资源
   */
  ~WakeupNotifier();

  /**
   * 函数名：Initialize
   * 输入：无
   * 输出：初始化成功返回 true，失败返回 false
   * 函数功能：检查提示音文件并尝试初始化 Linux LED 控制
   */
  bool Initialize();

  /**
   * 函数名：Notify
   * 输入：无
   * 输出：无
   * 函数功能：异步播放提示音，并在支持时切换 LED 状态
   */
  void Notify();

  /**
   * 函数名：Wait
   * 输入：无
   * 输出：无
   * 函数功能：等待正在播放的提示音结束
   */
  void Wait();

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace wekws

#endif  // STREAM_WAKEUP_NOTIFIER_H_
