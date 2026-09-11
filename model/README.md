# Models

`wekws_mdtc_small.tflite` is the example model embedded by
`model_data.cc`.

- architecture: MDTC-small
- input feature: 80-dimensional MFCC
- input window: 256 frames
- tensor type: float32
- streaming cache shape: 32 x 184
- TFLite file SHA-256:
  `0c8d8ae47d91a248e4d1166d46ce45869b352328b4b439051ded22f839e2d78a`

Regenerate the embedded C++ array after replacing the model:

```bash
python tools/convert_tflite_to_cc.py model/your_model.tflite model/model_data.cc
```

The runtime also contains int8 tensor handling, but a replacement model must
use only operators registered in `kws/keyword_spotting.cc`.

## Alternative models

The original `tflite_micro_runtime/model` alternatives are copied unchanged
into the following directories:

| Directory | Contents | Size |
| --- | --- | ---: |
| `onnx/` | Quantized DS-TCN ONNX model | 333 KiB |
| `test/` | MDTC/DS-TCN TFLite variants and the original conversion helper | 7.19 MiB |
| `tflite/` | Two additional TFLite model backups | 1.40 MiB |

These files are candidates only. The build still embeds
`wekws_mdtc_small.tflite`; adding a candidate does not select it automatically.
The ONNX file cannot be executed by the TFLite Micro runtime. Before replacing
the default model, verify its input/output shapes, feature configuration,
streaming cache layout, tensor types, required operators, and Tensor Arena
size, then regenerate `model_data.cc`.

`test/avg_mdtc_small_256.tflite` is byte-for-byte identical to the default
`wekws_mdtc_small.tflite`. The retained `test/convert_tflite_to_cc.py` is the
original helper; new conversions should normally use
`tools/convert_tflite_to_cc.py`.

## SHA-256

```text
bd10bcabee89254c8a2aa59707a3c4744ddebe13888c4a77b1dcab9f2b007b5a  onnx/ds_tcn_quantized.onnx
1263a4db50ac761dba38f2fad4d1c91e2c8d933dd9dc4a72aa49586262a7629f  test/avg_mdtc_256.tflite
0c8d8ae47d91a248e4d1166d46ce45869b352328b4b439051ded22f839e2d78a  test/avg_mdtc_small_256.tflite
02df016aa2ad40d2baff66bcf60ce3ffb1c3545b544309c4d79cf9e8c0480f33  test/convert_tflite_to_cc.py
ff89f81dff200f4a29d24084794f7f34a86c90d49ad25e4495700bbd190bb10c  test/ds_tcn_direct_2.tflite
95714c1c7e65e8d63a637dda40d5afbe5d053e1888f0471ee742088e7f1479be  test/ds_tcn_direct_256.tflite
19205c2f54a76c7c16ca91de4997f6c5c693fbde0dfffb5aec995557b0b58b67  test/ds_tcn_quantized_256.tflite
87ceec281812672e75b2e14aed396159d741fbf211233e5230fb0ef4c6d6d0ff  test/model_int8_256.tflite
44442061d4764d18c1ed85e0b60d66ebbfa88d2e227c388ec84a6cd2bdc2980f  tflite/avg_30_256.tflite
55eac0f9aac067df19edd72b0d96c4e148dd5337ef4520245813c1bd9a442cdc  tflite/ds_tcn_fixed_backup.tflite
```
