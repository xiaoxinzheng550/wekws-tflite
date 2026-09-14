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
  explicit WakeupNotifier(std::string audio_path);
  ~WakeupNotifier();

  bool Initialize();
  void Notify();
  void Wait();

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace wekws

#endif  // STREAM_WAKEUP_NOTIFIER_H_
