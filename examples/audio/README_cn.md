# 测试音频

本目录中的所有文件均为 16 kHz、16-bit、单声道 PCM WAV 文件，可以直接传给
`kws_main` 进行测试。

示例：

```bash
./build/bin/kws_main mfcc 80 256 examples/audio/silence.wav
./build/bin/kws_main mfcc 80 256 examples/audio/wozai.wav
./build/bin/kws_main mfcc 80 256 examples/audio/haode.wav
```

这些文件包括唤醒词样本、普通语音、静音和带噪测试录音，用于复现实验结果和
技术评估。将它们重新分发到其他产品前，请先确认音频来源及相应授权。
