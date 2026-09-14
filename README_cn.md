# wekws-tflite

基于 [WeKWS](https://github.com/wenet-e2e/wekws) 和 TensorFlow Lite Micro的轻量级关键词唤醒推理示例，提供离线 WAV 检测、实时麦克风检测和嵌入式模型数组生成工具。

## 上游项目与数据集

- WeKWS 原始项目：[wenet-e2e/wekws](https://github.com/wenet-e2e/wekws)
- “你好问问”数据集：[MobvoiHotwords（OpenSLR SLR87）](https://www.openslr.org/87/)

MobvoiHotwords 是由出门问问提供的中文唤醒词数据集，包含“Hi Xiaowen”和“Nihao Wenwen（你好问问）”关键词语音及非关键词语音。下载、许可和数据集详细说明以 OpenSLR 页面为准。

当前嵌入的是量化 DS-TCN 模型`model/tflite/ds_tcn_fixed_quantized_backup.tflite`，输入为 16 kHz单声道 PCM 提取的 40 维 Fbank，固定输入窗口为 256 帧。该模型内部包含量化/反量化节点，但输入、缓存和输出接口仍为 float32。仓库专注 KWS 推理，不内置SpeexDSP、AEC、降噪或 AGC；产品使用时可在送入特征提取器前接入自己的音频前处理模块。

当前模型的两个输出类别分别对应“嗨小问”和“你好问问”，离线与实时程序均支持识别这两个唤醒词。

## 功能

- TFLite Micro C++ 推理，支持 float32、int8 和 uint8 张量。
- 80 维 MFCC、40 维 Log-Mel Fbank 前端。
- 滑动窗口推理和重复唤醒抑制。
- “嗨小问”和“你好问问”双唤醒词检测。
- WAV 文件离线测试。
- macOS PortAudio、Linux `arecord` 实时采集示例。
- `.tflite` 转 C++ 静态数组工具脚本。
- 随仓库提供 macOS arm64、Linux x86_64 和 ARMv7 预编译 TFLM 库。

## 项目进度

| 状态 | 内容 |
| --- | --- |
| 已完成 | 量化 DS-TCN 模型接入，支持 TFLite Micro 推理及模型静态数组加载 |
| 已完成 | 40 维 Log-Mel Fbank 和 80 维 MFCC 特征提取 |
| 已完成 | 256 帧滑动窗口、唤醒阈值、连续命中和冷却时间控制 |
| 已完成 | WAV 离线检测及 macOS PortAudio 实时麦克风检测 |
| 已完成 | 按平台选择 macOS arm64、Linux x86_64 和 ARMv7 TFLM 静态库 |
| 进行中 | Linux x86_64 实机编译、录音和端到端唤醒验证 |
| 待完成 | 补充 ONNX 转 TFLite 脚本及模型转换流程 |
| 待完成 | 优化mdtc_small唤醒精度 |

## 待优化问题

- mdtc架构模型，唤醒精度不足，但是推理延迟可以支撑IMX6ULL芯片的实时唤醒（可以考虑改单一唤醒词，重新训练模型进行测试）
- ds_tcn唤醒精度98%，但是推理延迟不适合低资源设备。（暂未使用RK3566/3588等设备验证过响应延迟）

## 端侧移植与模型推理延迟

模型表记录每个模型在不同平台上的纯推理延迟。以下数据用于记录当前验证进度，不代表不同设备上的统一性能指标。

### 平台移植进度

| 平台 | 当前状态 | 采集/运行方式 | 说明 |
| --- | --- | --- | --- |
| macOS arm64 | 已验证离线推理 | WAV；实时采集使用 PortAudio | 当前主要开发和验证平台 |
| Linux x86_64 | 已提供构建依赖，待实机验证 | WAV；实时采集使用 `arecord` | 仍待验证 |
| Linux i.MX6ULL/ARMv7 | 已完成开发板端的流式验证 | `arecord` 或板端 PCM 接口 | 已测试 DS-TCN、MDTC 和 MDTC-small |
| NuttX/ARMv7 | 已完成模型推理验证 | 板端 PCM 回调 | 已测试 DS-TCN 和 MDTC-small |

### 不同模型的推理延迟

表中的延迟是单个固定窗口的模型推理时间，不包含 WAV 读取、实时录音和特征提取。`待测试` 表示仓库中已有模型但尚未在该平台形成可复现数据，`暂不可测` 表示模型当前存在已知的内核兼容问题。

| 模型 | 输入特征 | macOS arm64 | Linux x86_64 | Linux i.MX6ULL/ARMv7 | NuttX/ARMv7 | 当前验证情况 |
| --- | --- | --- | --- | --- | --- | --- |
| `ds_tcn_fixed_quantized_backup.tflite` | 40 维 Fbank；256 帧 | 约 100 ms | 待测试 | 约 1 s | 约 6 s | macOS 和 i.MX6ULL 均已唤醒成功；该模型不适合当前低资源板端配置 |
| `avg_mdtc_256.tflite` | 80 维 MFCC；256 帧 | 待测试 | 待测试 | 约520 ms | 待测试 | 推理延迟下降，唤醒精度下降 |
| `avg_mdtc_small_256.tflite` | 80 维 MFCC；256 帧 | 待测试 | 待测试 | 约150 ms | 约500 ms | 推理延迟下降，唤醒精度极差 |

## 目录

```text
wekws-tflite/
├── bin/                 # 离线和实时示例入口
│   └── stream_kws_main.cc       # 流式唤醒主流程
├── frontend/            # MFCC/Fbank、FFT、WAV读取
├── kws/                 # TFLite Micro KWS封装
├── stream/              # 流式推理辅助模块
│   ├── audio_recorder.h/.cc     # 实时录音
│   ├── sliding_feature_window.h/.cc # 256帧滑动窗口
│   ├── wakeup_postprocessor.h/.cc   # 双唤醒词后处理
│   ├── wakeup_notifier.h/.cc    # 播放提示音、控制LED
│   └── inference_stats.h/.cc    # 推理性能统计与日志
├── model/               # 默认模型、嵌入数组及备选模型
├── examples/test_audio/ # 16 kHz单声道WAV测试样本
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

如果 Conda 环境位于 macOS 的 FAT/exFAT 外接硬盘，文件系统可能生成大量`._*` AppleDouble 元数据，使 `pip` 报 `Ignoring invalid distribution`，或使`conda list` 报 `UnicodeDecodeError`。这不是项目缺少 Python 依赖。优先把环境创建在 APFS/本机磁盘；已有环境可退出后使用 `dot_clean -m` 清理：

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
./build/bin/kws_main fbank 40 256 examples/test_audio/0000e12e2402775c2d506d77b6dbb411.wav
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

实时日志分别输出 `hi_xiaowen_score` 和 `nihao_wenwen_score`。唤醒成功时，`keyword=hi_xiaowen class=0` 表示“嗨小问”，`keyword=nihao_wenwen class=1` 表示“你好问问”。

唤醒成功后，程序默认异步播放 `examples/test_audio/wozai.wav`：macOS 使用系统 `afplay`，Linux 使用 `aplay -q`。播放在线程中执行，不阻塞录音和模型推理。Linux 需要安装包含 `aplay` 的 ALSA utilities。也可以通过第六个可选参数指定其他提示音：

```bash
./build/bin/stream_kws_main default fbank 40 0.80 50 /absolute/path/to/wakeup.wav
```

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
