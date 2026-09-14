# 模型说明

## 当前嵌入模型

`model_data.cc` 当前由 `tflite/ds_tcn_fixed_quantized_backup.tflite` 生成：

- 架构：DS-TCN
- 特征：40 维 Log-Mel Fbank
- 特征输入：`[1, 256, 40]`
- 流式缓存：`[1, 256, 105]`
- 概率输出：`[1, 256, 2]`
- 类别映射：`0` 为“嗨小问”（`HI_XIAOWEN`），`1` 为“你好问问”（`NIHAO_WENWEN`）
- 输入、缓存和输出接口：float32
- 模型内部：包含 `QUANTIZE`、`CAST` 和 `DEQUANTIZE`，属于量化模型，但不是全 INT8 输入输出模型
- 当前 TFLite Micro Tensor Arena 实测使用：1,766,163 字节

编译并测试当前模型：

```bash
./build.sh kws
./build/bin/kws_main fbank 40 256 \
  examples/test_audio/0000e12e2402775c2d506d77b6dbb411.wav
```

指定音频实测类别 1 最高分为 `1.0`，超过默认 `0.80` 阈值，能够识别成功。

## `avg_30_256.tflite` 的限制

该模型不是 MFCC 模型，其张量配置为：

- 特征：40 维 Log-Mel Fbank
- 特征输入：`[1, 256, 40]`，float32
- 流式缓存：`[1, 256, 105]`，float32
- 概率输出：`[1, 256, 2]`，float32

它目前不能直接由仓库内置的 TFLite Micro 静态库运行。模型包含多个 `GATHER` 节点，其 positions 常量为 INT64；当前 TFLite Micro Gather 内核不支持 INT64 positions，初始化时会报：

```text
Positions of type 'INT64' are not supported by gather.
Node GATHER failed to prepare
```

仅在 `MicroOpResolver` 中注册 `GATHER` 仍不够。需要重新导出模型，把 Gather 索引转换为 INT32，或者使用明确支持 INT64 Gather 的 TFLite Micro 实现。修正模型后，测试命令应使用 Fbank，而不是 MFCC：

```bash
python tools/convert_tflite_to_cc.py \
  model/tflite/avg_30_256_fixed.tflite model/model_data.cc
./build.sh kws
./build/bin/kws_main fbank 40 256 examples/test_audio/your_test.wav
```

## MDTC 模型

`tflite/avg_mdtc_256.tflite` 和 `tflite/avg_mdtc_small_256.tflite` 使用 80 维 MFCC。切换到 MDTC-small 的示例：

```bash
python tools/convert_tflite_to_cc.py \
  model/tflite/avg_mdtc_small_256.tflite model/model_data.cc
./build.sh kws
./build/bin/kws_main mfcc 80 256 examples/test_audio/your_test.wav
```

切换模型后必须核对输入输出形状、缓存布局、张量类型、所需算子和 Tensor Arena 大小。将文件放进 `model/` 不会自动切换嵌入模型。

## 模型算子依赖

DS-TCN、MDTC 和 MDTC-small 共同依赖以下算子，因此这些公共算子统一放在 `kws/keyword_spotting.cc` 的注册列表前部：

```text
ADD
CONCATENATION
CONV_2D
FULLY_CONNECTED
LOGISTIC
MUL
RESHAPE
SUB
TRANSPOSE
```

不同模型额外依赖的算子如下：

| 模型 | 额外算子 | 说明 |
| --- | --- | --- |
| `ds_tcn_fixed_quantized_backup.tflite` | `QUANTIZE`、`CAST`、`DEQUANTIZE`、`RELU`、`SLICE`、`DEPTHWISE_CONV_2D` | 量化 DS-TCN 当前嵌入模型 |
| `avg_mdtc_256.tflite` | `STRIDED_SLICE` | 标准 MDTC |
| `avg_mdtc_small_256.tflite` | `STRIDED_SLICE` | MDTC-small；与标准 MDTC 使用相同种类的算子，但节点数量不同 |

`avg_30_256.tflite` 不属于上面的共同集合。它实际依赖 `SUB`、`FULLY_CONNECTED`、`TRANSPOSE`、`GATHER`、`CONCATENATION`、`RESHAPE`、`DEPTHWISE_CONV_2D`、`CONV_2D`、`ADD` 和 `LOGISTIC`。其中 `GATHER` 已注册，但该模型使用 INT64 positions，仍受前文所述的 TFLite Micro 内核限制。

当前注册列表取这些备选模型所需算子的并集，暂不裁剪。`ROUND` 暂时保留，但上述仓库内模型均未使用该算子。

## 目录

| 目录 | 内容 |
| --- | --- |
| `onnx/` | MDTC 及量化 DS-TCN ONNX 模型 |
| `tflite/` | DS-TCN、MDTC 及其他 TFLite 模型 |

ONNX 文件不能直接由 TFLite Micro 执行。

## SHA-256

```text
03d1349db80c5b43596049702b1181c51ddd904e695b005a4aebc3628e17f00f  onnx/avg_mdtc.onnx
674dea0326022836c805a0d39935e22b4a3fd6995e050b7590a38bfb00f1e9b8  onnx/avg_mdtc_small.onnx
bd10bcabee89254c8a2aa59707a3c4744ddebe13888c4a77b1dcab9f2b007b5a  onnx/ds_tcn_quantized.onnx
44442061d4764d18c1ed85e0b60d66ebbfa88d2e227c388ec84a6cd2bdc2980f  tflite/avg_30_256.tflite
1263a4db50ac761dba38f2fad4d1c91e2c8d933dd9dc4a72aa49586262a7629f  tflite/avg_mdtc_256.tflite
0c8d8ae47d91a248e4d1166d46ce45869b352328b4b439051ded22f839e2d78a  tflite/avg_mdtc_small_256.tflite
55eac0f9aac067df19edd72b0d96c4e148dd5337ef4520245813c1bd9a442cdc  tflite/ds_tcn_fixed_quantized_backup.tflite
```
