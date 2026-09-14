# Model Guide

## Currently Embedded Model

`model_data.cc` is currently generated from `tflite/ds_tcn_fixed_quantized_backup.tflite`:

- Architecture: DS-TCN
- Features: 40-dimensional Log-Mel Fbank
- Feature input: `[1, 256, 40]`
- Streaming cache: `[1, 256, 105]`
- Probability output: `[1, 256, 2]`
- Input, cache, and output interfaces: float32
- Internal model structure: contains `QUANTIZE`, `CAST`, and `DEQUANTIZE`; it is a quantized model but does not expose fully INT8 input and output interfaces
- Measured TFLite Micro Tensor Arena usage: 1,766,163 bytes

Build and test the current model:

```bash
./build.sh kws
./build/bin/kws_main fbank 40 256 \
  examples/test_audio/0000e12e2402775c2d506d77b6dbb411.wav
```

For the specified audio file, the measured maximum score for class 1 is `1.0`, exceeding the default threshold of `0.80`; wake-word recognition succeeds.

## Limitations of `avg_30_256.tflite`

This is not an MFCC model. Its tensor configuration is:

- Features: 40-dimensional Log-Mel Fbank
- Feature input: `[1, 256, 40]`, float32
- Streaming cache: `[1, 256, 105]`, float32
- Probability output: `[1, 256, 2]`, float32

It cannot currently run directly with the TFLite Micro static libraries bundled in this repository. The model contains several `GATHER` nodes whose positions constants are INT64, while the current TFLite Micro Gather kernel does not support INT64 positions. Initialization reports:

```text
Positions of type 'INT64' are not supported by gather.
Node GATHER failed to prepare
```

Registering `GATHER` in `MicroOpResolver` is not sufficient. The model must be exported again with Gather indices converted to INT32, or it must use a TFLite Micro implementation that explicitly supports INT64 Gather. After correcting the model, use Fbank rather than MFCC in the test command:

```bash
python tools/convert_tflite_to_cc.py \
  model/tflite/avg_30_256_fixed.tflite model/model_data.cc
./build.sh kws
./build/bin/kws_main fbank 40 256 examples/test_audio/your_test.wav
```

## MDTC Models

`tflite/avg_mdtc_256.tflite` and `tflite/avg_mdtc_small_256.tflite` use 80-dimensional MFCC features. Example for switching to MDTC-small:

```bash
python tools/convert_tflite_to_cc.py \
  model/tflite/avg_mdtc_small_256.tflite model/model_data.cc
./build.sh kws
./build/bin/kws_main mfcc 80 256 examples/test_audio/your_test.wav
```

After switching models, verify the input/output shapes, cache layout, tensor types, required operators, and Tensor Arena size. Placing a file under `model/` does not automatically select it as the embedded model.

## Model Operator Dependencies

DS-TCN, MDTC, and MDTC-small all require the following operators. These common operators therefore appear first in the registration list in `kws/keyword_spotting.cc`:

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

Additional model-specific operators are listed below:

| Model | Additional operators | Notes |
| --- | --- | --- |
| `ds_tcn_fixed_quantized_backup.tflite` | `QUANTIZE`, `CAST`, `DEQUANTIZE`, `RELU`, `SLICE`, `DEPTHWISE_CONV_2D` | Currently embedded quantized DS-TCN model |
| `avg_mdtc_256.tflite` | `STRIDED_SLICE` | Standard MDTC |
| `avg_mdtc_small_256.tflite` | `STRIDED_SLICE` | MDTC-small; uses the same operator types as standard MDTC, but with different node counts |

`avg_30_256.tflite` does not belong to the common set above. It actually requires `SUB`, `FULLY_CONNECTED`, `TRANSPOSE`, `GATHER`, `CONCATENATION`, `RESHAPE`, `DEPTHWISE_CONV_2D`, `CONV_2D`, `ADD`, and `LOGISTIC`. Although `GATHER` is registered, the model still has the previously described TFLite Micro kernel limitation because it uses INT64 positions.

The current registration list is the union of operators required by the alternative models and has not yet been pruned. `ROUND` is temporarily retained, although none of the models currently included in the repository uses it.

## Directories

| Directory | Contents |
| --- | --- |
| `onnx/` | MDTC and quantized DS-TCN ONNX models |
| `tflite/` | DS-TCN, MDTC, and other TFLite models |

ONNX files cannot be executed directly by TFLite Micro.

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
