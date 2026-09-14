# wekws-tflite

基于 [WeKWS](https://github.com/wenet-e2e/wekws) 和 TensorFlow Lite Micro的轻量级关键词唤醒推理示例，提供离线 WAV 检测、实时麦克风检测和嵌入式模型数组生成工具。

## 上游项目与数据集

- WeKWS 原始项目：[wenet-e2e/wekws](https://github.com/wenet-e2e/wekws)
- “你好问问”数据集：[MobvoiHotwords（OpenSLR SLR87）](https://www.openslr.org/87/)

MobvoiHotwords 是由出门问问提供的中文唤醒词数据集，包含“Hi Xiaowen”和“Nihao Wenwen（你好问问）”关键词语音及非关键词语音。下载、许可和数据集详细说明以 OpenSLR 页面为准。

当前嵌入的是量化 DS-TCN 模型`model/tflite/ds_tcn_fixed_quantized_backup.tflite`，输入为 16 kHz单声道 PCM 提取的 40 维 Fbank，固定输入窗口为 256 帧。该模型内部包含量化/反量化节点，但输入、缓存和输出接口仍为 float32。仓库专注 KWS 推理，不内置SpeexDSP、AEC、降噪或 AGC；产品使用时可在送入特征提取器前接入自己的音频前处理模块。

## 功能

- TFLite Micro C++ 推理，支持 float32、int8 和 uint8 张量。
- 80 维 MFCC、40 维 Log-Mel Fbank 前端。
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
├── model/               # 默认模型、嵌入数组及备选模型
├── examples/audio/      # 16 kHz单声道WAV测试样本
├── tools/               # 模型检查和数组转换工具
├── third_party/tflm/    # TFLM头文件及分平台静态库
├── third_party/portaudio/ # 官方PortAudio v19.7.0源码
├── CMakeLists.txt
├── requirements.txt
└── LICENSE
```

## ONNX 转 TFLite 依赖环境与脚本（待添加，暂时可以不管）

1、当前仓库尚未提供 ONNX 转 TFLite 脚本，后续添加。
2、现有 Python 环境和requirements.txt只用于`tools/verify_tflite.py` 检查 TFLite 模型；
3、编译和运行 C++ 唤醒程序不依赖Conda、NumPy 或 TensorFlow。已验证的 Python 组合为 Python 3.10、NumPy 1.24.0 和 TensorFlow 2.16.2。

创建独立环境并安装固定版本的可选依赖：

```bash
conda create -n wekws-tflite python=3.10 pip -y
conda activate wekws-tflite
python -m pip install --upgrade pip
python -m pip install -r requirements.txt
python -m pip check
```

检查 TFLite 张量并执行一次零输入推理：

```bash
python tools/verify_tflite.py
```

如果 Conda 环境位于 macOS 的 FAT/exFAT 外接盘，文件系统可能生成大量`._*` AppleDouble 元数据，使 `pip` 报 `Ignoring invalid distribution`，或使`conda list` 报 `UnicodeDecodeError`。这不是项目缺少 Python 依赖。优先把环境创建在 APFS/本机磁盘；已有环境可退出后使用 `dot_clean -m` 清理：

```bash
conda deactivate
dot_clean -m "absolute path to conda/envs/wekws-tflite"
conda activate wekws-tflite
python -m pip check
```

临时查看这类环境的包列表可使用 `conda --no-plugins list`，但仍建议清理AppleDouble 文件或重新在 APFS 文件系统创建环境。

## 把模型转换为可烧录的 C++ 数组

```bash
python tools/convert_tflite_to_cc.py \
  /path/to/model.tflite model/model_data.cc
```

转换后重新编译即可。自己的模型必须满足以下约束：

- 两个输入：声学特征和流式缓存。
- 两个输出：关键词概率和更新后的缓存。
- 使用的算子必须已在 `kws/keyword_spotting.cc` 注册。
- 特征维度和固定窗口必须与运行参数一致。

`model/onnx/` 和 `model/tflite/` 保留了原工程中的模型文件。当前 `model_data.cc` 嵌入的是量化 DS-TCN 模型；其他模型不会被构建系统自动选用。各文件用途、正确测试参数、SHA-256 和替换注意事项见 [`model/README_cn.md`](model/README_cn.md)。

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
`build.sh` 不会下载 PortAudio。开启实时模式时，它会先检查`third_party/portaudio/CMakeLists.txt`，缺失时直接报错退出。PortAudio 的构建中间文件位于 `build/third_party/portaudio/`。

默认支持 macOS arm64 和 Linux x86_64，编译结果位于 `build/bin/`。额外的CMake 参数可以放在子命令后，例如 `./build.sh kws -DCMAKE_BUILD_TYPE=Debug`。

CMake 的构建缓存会记录源码绝对路径。如果工程连同旧 `build/` 一起移动或复制到其他目录，`build.sh` 会识别路径变化，自动删除默认的生成目录并重新配置；源码、模型和测试音频不会受影响。使用自定义 `BUILD_DIR` 时，为避免误删外部目录，脚本只会提示换用新的构建目录，不会自动清理。

如需改用另一份 PortAudio 源码，可以显式指定：

```bash
./build.sh kws_stream \
  -DWEKWS_PORTAUDIO_SOURCE_DIR=/absolute/path/to/portaudio
```

### 使用其他平台的 TFLite Micro 依赖

预编译静态库不能跨平台通用。如果你的系统不在默认支持范围，需要使用目标平台工具链重新构建 TFLite Micro，并显式指定头文件和静态库。平台选择、ARMv7 示例以及当前模型所需的两项兼容修改，详见[`third_party/tflm/README_cn.md`](third_party/tflm/README_cn.md)。

### 2. 清理构建文件

删除默认的 `build/` 生成目录：

```bash
./build.sh clean
```

如果编译时使用了自定义 `BUILD_DIR`，清理时需要指定同一个目录：

```bash
BUILD_DIR=/tmp/wekws-build ./build.sh clean
```

`clean` 只删除构建输出，保留源码、模型、测试音频和 Python 环境。出于安全考虑，脚本拒绝将项目根目录或 `/` 作为清理目标。

### 3. 测试 WAV

输入必须是 16 kHz、单声道 PCM WAV：

```bash
./build/bin/kws_main fbank 40 256 examples/audio/0000e12e2402775c2d506d77b6dbb411.wav
```

参数依次为：特征类型、特征维度、固定窗口帧数、WAV 文件。仓库附带多份唤醒、口语和噪声测试音频，全部为 16 kHz、16-bit、单声道PCM WAV。

### 4. 实时麦克风检测

macOS 构建使用仓库内置的 PortAudio v19.7.0 源码，不需要在配置阶段联网，并使用系统默认输入设备，可以使用下述指令进行流式唤醒测试：

```bash
./build/bin/stream_kws_main default fbank 40 0.80 50
```

Linux 使用系统的 `arecord`，需要先安装 ALSA utilities：

```bash
./build/bin/stream_kws_main plughw:CARD,DEV fbank 40 0.80 50
```

最后一个参数是滑动步长，单位为特征帧；50 帧约为 500 ms。

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

PortAudio 源码本身没有打补丁。`cmake/portaudio.cmake` 通过`add_subdirectory()` 直接加入本地源码，关闭不需要的共享库、测试和示例，并兼容 CMake 4 的策略要求；工程中不再包含 PortAudio 的网络下载逻辑。

仓库附带的 TFLite Micro 静态库已经包含当前量化模型所需的两项类型兼容修改：补充 `FLOAT32 -> UINT8` 量化，以及 `CAST` 的 `UINT8` 输入和输出支持。如果改用官方源码自行构建 TFLM，需要重新应用这些修改；具体说明见[`third_party/tflm/README_cn.md`](third_party/tflm/README_cn.md)。

## 第三方代码与模型

代码基于 WeKWS，按 Apache-2.0 发布。TFLite Micro 和 PortAudio 保留各自许可证，具体见 [NOTICE](NOTICE)。示例模型用于技术演示；用于商业产品前，请自行确认训练数据和模型权重的再分发权利。

## License

[Apache License 2.0](LICENSE)
