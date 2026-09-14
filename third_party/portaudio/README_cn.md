# PortAudio 依赖说明

本目录保存 `wekws-tflite` 在 macOS 上构建实时麦克风示例时使用的 PortAudio 源码，版本为官方 `v19.7.0`，对应提交 `147dd722548358763a8b649b3e4b41dfffbcfbb6`。

## 当前仓库保留的内容

PortAudio 源码本身没有修改。为减小仓库体积，只保留编译静态库所需的文件：

```text
third_party/portaudio/
├── CMakeLists.txt   # PortAudio 官方 CMake 构建入口
├── cmake_support/   # CMake 辅助模块
├── include/         # PortAudio 公共头文件
├── src/             # 通用代码及各平台宿主 API 实现
├── LICENSE.txt      # PortAudio 许可证
└── README_cn.md     # 本项目中文说明
```

原仓库的 `.git`、构建产物、文档、示例和测试程序没有随本项目发布，因此本目录中不存在 `doc/`、`examples/` 和 `test/`。如需这些内容，请前往 [PortAudio 官方仓库](https://github.com/PortAudio/portaudio/tree/v19.7.0) 获取。

## 在本项目中的构建方式

执行 `./build.sh` 或 `./build.sh kws_stream` 时，CMake 会通过 `add_subdirectory()` 直接编译本目录源码，并生成静态库；构建过程不会从网络下载 PortAudio。中间文件位于项目的 `build/third_party/portaudio/`。

如需改用其他 PortAudio 源码目录：

```bash
./build.sh kws_stream \
  -DWEKWS_PORTAUDIO_SOURCE_DIR=/absolute/path/to/portaudio
```

## 主要目录

```text
include/portaudio.h  = PortAudio API 头文件及接口规范
src/common/          = 与操作系统和宿主 API 无关的通用代码
src/os/              = 操作系统相关的公共代码
src/hostapi/         = Core Audio、ALSA、WASAPI 等宿主音频 API 实现
```

PortAudio 的 API、设备选择和平台说明见 [PortAudio v19.7.0 官方文档](https://portaudio.com/docs/v19-doxydocs/)。
