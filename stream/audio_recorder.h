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

  /**
   * 函数名：AudioRecorder
   * 输入：device 录音设备名，sample_rate 采样率，chunk_samples 单次采集点数
   * 输出：构造完成的录音器对象
   * 函数功能：保存录音配置并创建平台相关的录音实现
   */
  AudioRecorder(std::string device, int sample_rate, int chunk_samples);

  /**
   * 函数名：~AudioRecorder
   * 输入：无
   * 输出：无
   * 函数功能：关闭录音设备并释放平台相关资源
   */
  ~AudioRecorder();

  /**
   * 函数名：Open
   * 输入：无
   * 输出：成功返回 true，失败返回 false
   * 函数功能：打开并启动当前平台的录音设备
   */
  bool Open();

  /**
   * 函数名：Start
   * 输入：exiting 退出标志，audio_callback 音频回调，finished_callback 结束回调
   * 输出：无
   * 函数功能：启动录音线程并持续向上层提交 PCM 数据
   */
  void Start(volatile sig_atomic_t* exiting, AudioCallback audio_callback,
             FinishedCallback finished_callback);

  /**
   * 函数名：Join
   * 输入：无
   * 输出：无
   * 函数功能：等待录音线程结束
   */
  void Join();

  /**
   * 函数名：Close
   * 输入：无
   * 输出：平台录音资源的关闭状态码
   * 函数功能：停止录音并释放设备资源
   */
  int Close();

  /**
   * 函数名：device_name
   * 输入：无
   * 输出：实际使用的录音设备名称
   * 函数功能：查询当前录音设备名称
   */
  const std::string& device_name() const;

  /**
   * 函数名：captured_samples
   * 输入：无
   * 输出：已采集的 PCM 采样点总数
   * 函数功能：查询累计采集量
   */
  unsigned long long captured_samples() const;

  /**
   * 函数名：capture_overruns
   * 输入：无
   * 输出：录音输入溢出次数
   * 函数功能：查询录音过程中发生的溢出次数
   */
  unsigned int capture_overruns() const;

  /**
   * 函数名：failed
   * 输入：无
   * 输出：录音线程发生错误时返回 true
   * 函数功能：查询录音任务是否异常终止
   */
  bool failed() const;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

/**
 * 函数名：IsSafeAudioDevice
 * 输入：device 用户传入的录音设备名称
 * 输出：名称仅包含允许字符时返回 true
 * 函数功能：校验设备名，避免拼接 arecord 命令时产生命令注入
 */
bool IsSafeAudioDevice(const std::string& device);

/**
 * 函数名：DefaultAudioDevice
 * 输入：无
 * 输出：当前平台的默认录音设备名称
 * 函数功能：为未显式指定设备的流式程序提供默认值
 */
const char* DefaultAudioDevice();

}  // namespace wekws

#endif  // STREAM_AUDIO_RECORDER_H_
