# wekws-tflite

A lightweight keyword spotting inference example based on [WeKWS](https://github.com/wenet-e2e/wekws) and TensorFlow Lite Micro. It provides offline WAV detection, live microphone detection, and a tool for generating embedded model arrays.

## Upstream Project and Dataset

- Original WeKWS project: [wenet-e2e/wekws](https://github.com/wenet-e2e/wekws)
- "Nihao Wenwen" dataset: [MobvoiHotwords (OpenSLR SLR87)](https://www.openslr.org/87/)

MobvoiHotwords is a Chinese wake-word dataset provided by Mobvoi. It contains keyword and non-keyword speech for "Hi Xiaowen" and "Nihao Wenwen." Refer to the OpenSLR page for download information, licensing, and full dataset details.

The currently embedded model is the quantized DS-TCN model at `model/tflite/ds_tcn_fixed_quantized_backup.tflite`. Its input consists of 40-dimensional Fbank features extracted from 16 kHz mono PCM, with a fixed input window of 256 frames. The model contains internal quantize/dequantize nodes, while its input, cache, and output interfaces remain float32. This repository focuses on KWS inference and does not include SpeexDSP, AEC, noise suppression, or AGC. Product integrations can add their own audio preprocessing before passing audio to the feature extractor.

The two output classes of the current model correspond to "Hi Xiaowen" and "Nihao Wenwen." Both the offline and live programs support these two wake words.

## Features

- TFLite Micro C++ inference with float32, int8, and uint8 tensor support.
- 80-dimensional MFCC and 40-dimensional Log-Mel Fbank frontends.
- Sliding-window inference and duplicate wake-up suppression.
- Dual wake-word detection for "Hi Xiaowen" and "Nihao Wenwen."
- Offline WAV file testing.
- Live capture examples using PortAudio on macOS and `arecord` on Linux.
- Script for converting a `.tflite` model into a static C++ array.
- Bundled prebuilt TFLM libraries for macOS arm64, Linux x86_64, and ARMv7.

## Project Status

| Status | Item |
| --- | --- |
| Completed | Integrated the quantized DS-TCN model with TFLite Micro inference and static model-array loading |
| Completed | 40-dimensional Log-Mel Fbank and 80-dimensional MFCC feature extraction |
| Completed | 256-frame sliding window, wake-up threshold, consecutive-hit, and cooldown controls |
| Completed | Offline WAV detection and live macOS microphone detection through PortAudio |
| Completed | Platform-specific selection of macOS arm64, Linux x86_64, and ARMv7 TFLM libraries |
| In progress | Native Linux x86_64 compilation, recording, and end-to-end wake-up verification |
| Planned | ONNX-to-TFLite conversion script and model conversion workflow |
| Planned | Improve MDTC-small wake-up accuracy |

## Known Optimization Issues

- MDTC-family models have insufficient wake-up accuracy, although their inference latency supports real-time wake-up on i.MX6ULL. One option is to retrain a single-wake-word model and test it again.
- DS-TCN reaches 98% wake-up accuracy, but its inference latency is unsuitable for low-resource devices. Response latency has not yet been tested on RK3566/RK3588-class devices.

## Edge Deployment and Model Inference Latency

The model table records pure inference latency for each model on different platforms. The data represents the current verification status and is not a universal performance benchmark across devices.

### Platform Porting Status

| Platform | Current status | Capture/runtime method | Notes |
| --- | --- | --- | --- |
| macOS arm64 | Offline inference verified | WAV; PortAudio for live capture | Primary development and verification platform |
| Linux x86_64 | Build dependencies available; native verification pending | WAV; `arecord` for live capture | Verification pending |
| Linux i.MX6ULL/ARMv7 | Streaming verified on the development board | `arecord` or a board-level PCM interface | DS-TCN, MDTC, and MDTC-small tested |
| NuttX/ARMv7 | Model inference verified | Board-level PCM callback | DS-TCN and MDTC-small tested |

### Inference Latency by Model

The latency shown below is the inference time for one fixed window. It excludes WAV reading, live recording, and feature extraction. `Pending` means the repository contains the model but does not yet have reproducible data for that platform. `Currently unsupported` means the model has a known kernel compatibility issue.

| Model | Input features | macOS arm64 | Linux x86_64 | Linux i.MX6ULL/ARMv7 | NuttX/ARMv7 | Current verification |
| --- | --- | --- | --- | --- | --- | --- |
| `ds_tcn_fixed_quantized_backup.tflite` | 40-dimensional Fbank; 256 frames | About 100 ms | Pending | About 1 s | About 6 s | Wake-up succeeds on both macOS and i.MX6ULL; this model is unsuitable for the current low-resource board configuration |
| `avg_mdtc_256.tflite` | 80-dimensional MFCC; 256 frames | Pending | Pending | About 520 ms | Pending | Lower inference latency, but lower wake-up accuracy |
| `avg_mdtc_small_256.tflite` | 80-dimensional MFCC; 256 frames | Pending | Pending | About 150 ms | About 500 ms | Lower inference latency, but very poor wake-up accuracy |

## Repository Layout

```text
wekws-tflite/
├── bin/                 # Offline and live example entry points
├── frontend/            # MFCC/Fbank, FFT, and WAV reading
├── kws/                 # TFLite Micro KWS wrapper
├── model/               # Default model, embedded array, and alternative models
├── examples/test_audio/ # 16 kHz mono WAV test samples
├── tools/               # Model inspection and array-conversion tools
├── third_party/tflm/    # TFLM headers and platform-specific static libraries
├── third_party/portaudio/ # Official PortAudio v19.7.0 source
├── CMakeLists.txt
├── requirements.txt
└── LICENSE
```

## ONNX-to-TFLite Environment and Script (Planned; Can Be Ignored for Now)

1. This repository does not yet provide an ONNX-to-TFLite conversion script. It will be added later.
2. The existing Python environment and `requirements.txt` are used only by `tools/verify_tflite.py` to inspect TFLite models.
3. Building and running the C++ wake-word programs does not require Conda, NumPy, or TensorFlow. The verified Python combination is Python 3.10, NumPy 1.24.0, and TensorFlow 2.16.2.

Create an isolated environment and install the pinned optional dependencies:

```bash
conda create -n wekws-tflite python=3.10 pip -y
conda activate wekws-tflite
python -m pip install --upgrade pip
python -m pip install -r requirements.txt
python -m pip check
```

Inspect TFLite tensors and run one zero-input inference:

```bash
python tools/verify_tflite.py
```

If the Conda environment is located on a FAT/exFAT external drive on macOS, the filesystem may create many `._*` AppleDouble metadata files. This can cause `pip` to report `Ignoring invalid distribution` or `conda list` to raise a `UnicodeDecodeError`. This does not mean the project is missing Python dependencies. Prefer creating the environment on APFS or an internal disk. To clean an existing environment, deactivate it first and run `dot_clean -m`:

```bash
conda deactivate
dot_clean -m "absolute path to conda/envs/wekws-tflite"
conda activate wekws-tflite
python -m pip check
```

You can temporarily inspect packages in such an environment with `conda --no-plugins list`, but cleaning the AppleDouble files or recreating the environment on APFS is still recommended.

## Convert a Model into an Embeddable C++ Array

```bash
python tools/convert_tflite_to_cc.py \
  /path/to/model.tflite model/model_data.cc
```

Rebuild after conversion. A custom model must satisfy these constraints:

- Two inputs: acoustic features and the streaming cache.
- Two outputs: keyword probabilities and the updated cache.
- All required operators must be registered in `kws/keyword_spotting.cc`.
- The feature dimension and fixed window must match the runtime arguments.

`model/onnx/` and `model/tflite/` retain model files from the original project. The current `model_data.cc` embeds the quantized DS-TCN model; the build system does not automatically select any other model. See [`model/README.md`](model/README.md) for each file's purpose, correct test parameters, SHA-256 digest, and replacement notes.

## Quick Start

### 1. Build

Requirements: CMake 3.16+ and a compiler with C++14 support.

```bash
git clone https://github.com/xiaoxinzheng550/wekws-tflite.git
cd wekws-tflite
./build.sh
```

With no subcommand, the script builds both the offline and live programs. You can also build either target separately:

```bash
./build.sh              # Build both kws_main and stream_kws_main
./build.sh kws          # Build only the offline WAV program, kws_main
./build.sh kws_stream   # Build only the live program, stream_kws_main
```

`build.sh` never downloads PortAudio. When live mode is enabled, it first checks for `third_party/portaudio/CMakeLists.txt` and exits with an error if the file is missing. PortAudio intermediate build files are stored under `build/third_party/portaudio/`.

macOS arm64 and Linux x86_64 are supported by default. Binaries are written to `build/bin/`. Additional CMake options can follow the subcommand, for example `./build.sh kws -DCMAKE_BUILD_TYPE=Debug`.

CMake's build cache records the source tree's absolute path. If the repository is moved or copied together with an old `build/` directory, `build.sh` detects the path change, removes only the default generated directory, and configures it again. Source files, models, and test audio are not affected. When using a custom `BUILD_DIR`, the script does not delete the external directory automatically; it asks you to choose a new build directory instead.

To use a different PortAudio source tree, specify it explicitly:

```bash
./build.sh kws_stream \
  -DWEKWS_PORTAUDIO_SOURCE_DIR=/absolute/path/to/portaudio
```

### TFLite Micro Dependencies on Other Platforms

Prebuilt static libraries are not portable across platforms. If your system is not supported by default, build TFLite Micro with the target platform's toolchain and explicitly provide its headers and static library. See [`third_party/tflm/README.md`](third_party/tflm/README.md) for platform selection, an ARMv7 example, and the two compatibility changes required by the current model.

### 2. Clean Build Files

Remove the default generated `build/` directory:

```bash
./build.sh clean
```

If you used a custom `BUILD_DIR`, specify the same directory when cleaning:

```bash
BUILD_DIR=/tmp/wekws-build ./build.sh clean
```

`clean` removes only build output and preserves source code, models, test audio, and Python environments. For safety, the script refuses to clean the project root or `/`.

### 3. Test a WAV File

Input must be a 16 kHz, mono PCM WAV file:

```bash
./build/bin/kws_main fbank 40 256 examples/test_audio/0000e12e2402775c2d506d77b6dbb411.wav
```

The arguments are the feature type, feature dimension, fixed-window frame count, and WAV file. The repository includes wake-word, conversational-speech, and noise samples. All are 16 kHz, 16-bit, mono PCM WAV files.

### 4. Live Microphone Detection

The macOS build uses the bundled PortAudio v19.7.0 source and does not require network access during configuration. It uses the system's default input device. Run live wake-word detection with:

```bash
./build/bin/stream_kws_main default fbank 40 0.80 50
```

Linux uses the system's `arecord` command and requires ALSA utilities:

```bash
./build/bin/stream_kws_main plughw:CARD,DEV fbank 40 0.80 50
```

The final argument is the sliding stride in feature frames; 50 frames is approximately 500 ms.

The live log reports `hi_xiaowen_score` and `nihao_wenwen_score` separately. On a successful wake-up, `keyword=hi_xiaowen class=0` identifies "Hi Xiaowen," while `keyword=nihao_wenwen class=1` identifies "Nihao Wenwen."

After a successful wake-up, the program asynchronously plays `examples/test_audio/wozai.wav` by default. It uses the system `afplay` command on macOS and `aplay -q` on Linux. Playback runs in a separate thread and does not block audio capture or model inference. Linux requires ALSA utilities, which provides `aplay`. You can select another prompt sound with the sixth optional argument:

```bash
./build/bin/stream_kws_main default fbank 40 0.80 50 /absolute/path/to/wakeup.wav
```

## Embedded Integration

A product integration normally needs only:

```text
PCM callback
  -> optional AEC/NS/AGC
  -> FeaturePipeline::AcceptWaveform()
  -> fixed-window MFCC/Fbank
  -> KeywordSpotting::Forward()
  -> threshold, debounce, and application callback
```

When porting to an MCU/RTOS, also:

- Replace `std::vector` and dynamic allocation with fixed memory pools.
- Replace the recording thread with DMA/audio-interrupt callbacks.
- Adjust the Tensor Arena based on the model's actual peak usage.
- Register only the TFLM operators actually used by the model.
- Recalibrate the threshold, consecutive-hit count, and cooldown time using target-domain speech.

## Third-Party Dependency Versions and Modifications

The PortAudio source itself is unmodified. `cmake/portaudio.cmake` adds the local source directly through `add_subdirectory()`, disables unnecessary shared libraries, tests, and examples, and handles the policy requirements of CMake 4. The project no longer contains logic that downloads PortAudio over the network.

The bundled TFLite Micro static libraries contain two type-compatibility changes required by the current quantized model: `FLOAT32 -> UINT8` quantization and `UINT8` input/output support for `CAST`. If you rebuild TFLM from the official source, you must reapply these changes. See [`third_party/tflm/README.md`](third_party/tflm/README.md) for details.

## Third-Party Code and Models

The code is based on WeKWS and distributed under Apache-2.0. TFLite Micro and PortAudio retain their respective licenses; see [NOTICE](NOTICE). The example models are provided for technical demonstration. Before using them in a commercial product, verify the redistribution rights for the training data and model weights.

## License

[Apache License 2.0](LICENSE)
