# PortAudio——跨平台音频输入输出库

PortAudio 是面向跨平台音频处理的便携式输入输出库。它既可以通过回调机制请求
音频处理，也可以使用阻塞式读写接口，在系统原生音频子系统和客户端之间传递
缓冲数据。音频可以采用多种格式进行处理，包括 32-bit 浮点格式；PortAudio 会在
内部将其转换为系统原生格式。

## 文档

- 在线文档：[http://www.portaudio.com/docs/](http://www.portaudio.com/docs/)
- 运行 Doxygen 后生成的本地文档：`/doc/html/index.html`
- API 规范：`src/common/portaudio.h`
- 使用示例：`examples/` 和 `test/` 目录；入门可参考 `examples/paex_saw.c`

编译 PortAudio 应用程序的说明参见：
[PortAudio 教程](http://portaudio.com/docs/v19-doxydocs/tutorial_start.html)。

PortAudio 设有供用户和开发者交流的活跃邮件列表，加入方式参见
[PortAudio 官方网站](http://www.portaudio.com)。

## 重要文件和目录

```text
include/portaudio.h  = PortAudio API 头文件及接口规范
src/common/          = 与平台和宿主 API 无关的通用代码
src/os               = 操作系统相关、但与宿主 API 无关的代码
src/hostapi          = 不同宿主音频 API 的实现
```

### 宿主 API 实现

```text
src/hostapi/alsa      = Advanced Linux Sound Architecture（ALSA）
src/hostapi/asihpi    = AudioScience HPI
src/hostapi/asio      = Windows 和 Macintosh 上的 ASIO
src/hostapi/coreaudio = macOS Core Audio
src/hostapi/dsound    = Windows DirectSound
src/hostapi/jack      = JACK Audio Connection Kit
src/hostapi/oss       = Unix Open Sound System（OSS）
src/hostapi/wasapi    = Windows Vista WASAPI
src/hostapi/wdmks     = Windows WDM Kernel Streaming
src/hostapi/wmme      = Windows MultiMedia Extensions（MME）
```

### 测试程序

```text
test/pa_fuzz.c          = 吉他失真效果器
test/pa_devs.c          = 输出可用设备列表
test/pa_minlat.c        = 测量当前设备的最低延迟
test/paqa_devs.c        = 打开所有设备进行自检
test/paqa_errs.c        = 测试错误检测和报告
test/patest_clip.c      = 播放削波和未削波的正弦波
test/patest_dither.c    = 演示抖动效果（效果非常细微）
test/patest_pink.c      = 粉红噪声示例
test/patest_record.c    = 录音和回放
test/patest_maxsines.c  = 测试能够播放多少路正弦波，并测试 Pa_GetCPULoad()
test/patest_sine.c      = 播放简单正弦波
test/patest_sync.c      = 测试音视频同步
test/patest_wire.c      = 将输入直通到输出，模拟音频连线
```
