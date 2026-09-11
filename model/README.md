# Model

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
