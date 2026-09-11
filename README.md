# wekws-tflite

基于 [WeKWS](https://github.com/wenet-e2e/wekws) 和 TensorFlow Lite Micro
的轻量级关键词唤醒推理示例，提供离线 WAV 检测、实时麦克风检测和嵌入式
模型数组生成工具。

当前示例模型为 float32 MDTC-small TFLite 模型，输入为 16 kHz
单声道 PCM 提取的 80 维 MFCC，固定输入窗口为 256 帧。推理封装
同时兼容 int8 量化张量。仓库专注 KWS 推理，不内置
SpeexDSP、AEC、降噪或 AGC；产品使用时可在送入特征提取器前接入自己的
音频前处理模块。

## 功能

- TFLite Micro C++ 推理，支持 float32 和 int8 张量。
- 80 维 MFCC 与 Log-Mel Fbank 前端。
- 固定 256 帧滑动窗口推理和重复唤醒抑制。
- WAV 文件离线测试。
- macOS PortAudio、Linux `arecord` 实时采集示例。
- `.tflite` 转 C++ 静态数组工具。
- 随仓库提供 macOS arm64、Linux x86_64 和 ARMv7 预编译 TFLM 库。

## 目录

```text
wekws-tflite/
├── bin/                 # 离线和实时示例入口
├── frontend/            # MFCC/Fbank、FFT、WAV读取
├── kws/                 # TFLite Micro KWS封装
├── model/               # 示例TFLite模型及嵌入数组
├── examples/audio/      # 16 kHz单声道WAV测试样本
├── tools/               # 模型检查和数组转换工具
├── third_party/tflm/    # TFLM头文件及分平台静态库
├── third_party/portaudio/ # 官方PortAudio v19.7.0源码
├── CMakeLists.txt
├── requirements.txt
└── LICENSE
```

## 快速开始

### 1. 编译

要求：CMake 3.16+、支持 C++14 的编译器。

```bash
git clone https://github.com/xiaoxinzheng550/wekws-tflite.git
cd wekws-tflite
./build.sh
```

不带子命令时会同时编译离线与实时程序。也可以只编译其中一个目标：

```bash
./build.sh              # 全量编译 kws_main 和 stream_kws_main
./build.sh kws          # 只编译离线 WAV 程序 kws_main
./build.sh kws_stream   # 只编译实时程序 stream_kws_main
```

默认支持 macOS arm64 和 Linux x86_64，编译结果位于 `build/bin/`。额外的
CMake 参数可以放在子命令后，例如 `./build.sh kws -DCMAKE_BUILD_TYPE=Debug`。

CMake 的构建缓存会记录源码绝对路径。如果工程连同旧 `build/` 一起移动或
复制到其他目录，`build.sh` 会识别路径变化，自动删除默认的生成目录并重新
配置；源码、模型和测试音频不会受影响。使用自定义 `BUILD_DIR` 时，为避免
误删外部目录，脚本只会提示换用新的构建目录，不会自动清理。

### 2. 清理构建文件

删除默认的 `build/` 生成目录：

```bash
./build.sh clean
```

如果编译时使用了自定义 `BUILD_DIR`，清理时需要指定同一个目录：

```bash
BUILD_DIR=/tmp/wekws-build ./build.sh clean
```

`clean` 只删除构建输出，保留源码、模型、测试音频和 Python 环境。出于安全
考虑，脚本拒绝将项目根目录或 `/` 作为清理目标。

### 3. 测试 WAV

输入必须是 16 kHz、单声道 PCM WAV：

```bash
./build/bin/kws_main mfcc 80 256 examples/audio/silence.wav
./build/bin/kws_main mfcc 80 256 examples/audio/wozai.wav
./build/bin/kws_main mfcc 80 256 /path/to/16k_mono.wav
```

参数依次为：特征类型、特征维度、固定窗口帧数、WAV 文件。
仓库附带多个正样本、口语干扰和噪声测试文件，全部为 16 kHz、
16-bit、单声道 PCM WAV，详见 `examples/audio/README.md`。

### 4. 实时麦克风检测

```bash
./build.sh kws_stream
```

macOS 构建使用仓库内置的 PortAudio v19.7.0 源码，不需要在配置阶段联网，
并使用系统默认输入设备：

```bash
./build/bin/stream_kws_main default mfcc 80 0.80 50
```

`build.sh` 不会下载 PortAudio。开启实时模式时，它会先检查
`third_party/portaudio/CMakeLists.txt`，缺失时直接报错退出。PortAudio 的构建
中间文件位于 `build/third_party/portaudio/`；`build/fc_base/` 不再使用。

Linux 使用系统的 `arecord`，需要先安装 ALSA utilities：

```bash
./build/bin/stream_kws_main plughw:CARD,DEV mfcc 80 0.80 50
```

最后一个参数是滑动步长，单位为特征帧；50 帧约为 500 ms。

如需改用另一份 PortAudio 源码，可以显式指定：

```bash
./build.sh kws_stream \
  -DWEKWS_PORTAUDIO_SOURCE_DIR=/absolute/path/to/portaudio
```

## Python 工具

创建环境并安装可选依赖：

```bash
python3 -m venv .venv
source .venv/bin/activate
python -m pip install --upgrade pip
python -m pip install -r requirements.txt
```

检查 TFLite 张量并执行一次零输入推理：

```bash
python tools/verify_tflite.py
```

把自己的模型转换为可烧录的 C++ 数组：

```bash
python tools/convert_tflite_to_cc.py \
  /path/to/model.tflite model/model_data.cc
```

转换后重新编译即可。自己的模型必须满足以下约束：

- 两个输入：声学特征和流式缓存。
- 两个输出：关键词概率和更新后的缓存。
- 使用的算子必须已在 `kws/keyword_spotting.cc` 注册。
- 特征维度和固定窗口必须与运行参数一致。

## 使用其他平台的 TFLite Micro

预编译静态库不是跨平台文件。如果你的系统不在默认支持范围，请先用目标
工具链构建 TFLite Micro，再显式指定头文件和静态库：

```bash
cmake -S . -B build-target \
  -DTFLM_INCLUDE_DIR=/absolute/path/to/tflm/include \
  -DTFLM_LIBRARY=/absolute/path/to/libtensorflow-microlite.a
cmake --build build-target --parallel
```

TFLM 官方建议为具体平台生成所需源码树并使用目标平台构建系统编译静态库。

## 嵌入式接入

产品侧通常只需要复用：

```text
PCM回调
  -> 可选AEC/NS/AGC
  -> FeaturePipeline::AcceptWaveform()
  -> 固定窗口MFCC/Fbank
  -> KeywordSpotting::Forward()
  -> 阈值、防抖和业务回调
```

移植到 MCU/RTOS 时还应：

- 将 `std::vector` 和动态分配替换为固定内存池。
- 将录音线程替换为 DMA/音频中断回调。
- 根据模型实际峰值调整 Tensor Arena。
- 只注册模型真实使用的 TFLM 算子。
- 用目标语料重新标定阈值、连续命中次数和冷却时间。

## 第三方依赖版本与修改说明

仓库中的依赖取自原工程
`runtime/tflite_micro_runtime/fc_base`，发布前进行了逐文件核对：

- `third_party/portaudio`：PortAudio 官方 `v19.7.0`，提交
  `147dd722548358763a8b649b3e4b41dfffbcfbb6`。源码未修改，仅排除了
  `.git`、构建产物、测试、示例、文档和 macOS `._*` 元数据；保留实际构建
  所需的 `CMakeLists.txt`、`cmake_support`、`include`、`src` 和原许可证。
- `third_party/tflm/include`：与原工程的 131 个 TFLite Micro 头文件逐文件一致。
- `third_party/tflm/lib/macos-arm64/libtensorflow-microlite.a`：对应原文件
  `libtensorflow-microlite.a`，SHA-256 为
  `7e78cc5d80207e88eece4fa5c4b5fb553cf65322b463c4dc4a6cf181f38e69e1`。
- Linux x86_64、ARMv7 glibc 和 ARMv7 musl 静态库也只是按目标平台重命名
  归档，二进制内容未修改；完整校验值见 `third_party/tflm/README.md`。
- 原目录中的 `libtensorflow-microlite.a.arm32` 没有被原 CMake 使用，且无法
  从文件名可靠确定 ABI，因此没有作为默认库发布。需要该特定产物时请通过
  `-DTFLM_LIBRARY=/absolute/path/to/library.a` 显式指定，并确保工具链和 ABI 匹配。

项目对第三方源码本身没有打补丁。`cmake/portaudio.cmake` 通过
`add_subdirectory()` 直接加入本地源码，关闭不需要的共享库/测试/示例，并
兼容 CMake 4 的策略要求；工程中不再包含 PortAudio 的网络下载逻辑。

## 第三方代码与模型

代码基于 WeKWS，按 Apache-2.0 发布。TFLite Micro 和 PortAudio 保留各自
许可证，具体见 [NOTICE](NOTICE)。示例模型用于技术演示；用于商业产品前，
请自行确认训练数据和模型权重的再分发权利。

## License

[Apache License 2.0](LICENSE)
