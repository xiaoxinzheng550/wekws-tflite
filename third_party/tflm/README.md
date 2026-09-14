# TensorFlow Lite Micro Dependency Guide

This directory contains the TensorFlow Lite Micro (TFLM) files used to build and run `wekws-tflite`:

```text
third_party/tflm/
├── include/   # Headers used to compile the C++ programs
├── lib/       # Prebuilt static libraries for different systems, CPUs, and C runtimes
├── LICENSE    # TFLM license
├── README.md  # English documentation
└── README_cn.md
```

TFLM loads `.tflite` models, allocates the Tensor Arena, and executes model operators. `libtensorflow-microlite.a` is a static library: when `kws_main` is linked, the linker copies the required TFLM code into the final executable. TensorFlow, NumPy, and dynamic TFLM libraries are therefore not required at runtime.

## Prebuilt Static Libraries

A static library is not portable across arbitrary platforms. A mismatch in the operating system, CPU architecture, compiler ABI, or C runtime may cause errors such as an unrecognized file format or undefined symbols, or prevent the program from starting.

| Runtime platform | C runtime/ABI | Bundled static library | SHA-256 |
| --- | --- | --- | --- |
| macOS Apple Silicon | macOS arm64 | `lib/macos-arm64/libtensorflow-microlite.a` | `7e78cc5d80207e88eece4fa5c4b5fb553cf65322b463c4dc4a6cf181f38e69e1` |
| Linux x86_64 | glibc | `lib/linux-x86_64/libtensorflow-microlite.a` | `1d2140b1d4f0751409bbdfc9e507820dd94f4ff556ed13b67af0ff81089b6925` |
| Linux ARMv7 | glibc, gnueabihf | `lib/linux-armv7-gnueabihf/libtensorflow-microlite.a` | `d553265f01accaa73f3c22e457ad4f9144f146b551024a528fced6dda066bc02` |
| Linux ARMv7 | musl, musleabihf | `lib/linux-armv7-musleabihf/libtensorflow-microlite.a` | `e9cdf01b1ae974775fe5058f94b49be9c9b2e69b046502d6d9fa9a97bf2f214` |

## How CMake Selects a Static Library

The project currently selects these two static libraries automatically:

- Apple Silicon Mac: `lib/macos-arm64/libtensorflow-microlite.a`
- Linux x86_64: `lib/linux-x86_64/libtensorflow-microlite.a`

Although ARMv7 libraries are included, the build script cannot determine whether the target uses glibc or musl. You must select one explicitly. For example, use the following with an ARMv7 glibc toolchain:

```bash
cmake -S . -B build-arm \
  -DCMAKE_TOOLCHAIN_FILE=/absolute/path/to/armv7-toolchain.cmake \
  -DTFLM_LIBRARY="$PWD/third_party/tflm/lib/linux-armv7-gnueabihf/libtensorflow-microlite.a"
cmake --build build-arm --parallel
```

For an ARMv7 musl toolchain, change the path to:

```text
third_party/tflm/lib/linux-armv7-musleabihf/libtensorflow-microlite.a
```

Specifying a static library only selects the file; it does not turn the host compiler into a cross-compiler. An ARMv7 build still requires the corresponding cross-compilation toolchain.

## What Is SHA-256?

SHA-256 is a file-digest algorithm that converts a file of any size into a fixed 64-character hexadecimal string. For example:

```text
7e78cc5d80207e88eece4fa5c4b5fb553cf65322b463c4dc4a6cf181f38e69e1
```

You can think of it as a digital fingerprint:

- Two files with identical content have the same SHA-256 digest.
- Changing even one byte usually produces a completely different digest.
- It can reveal an incomplete download, file corruption, or the wrong file version.
- It is not a file size or version number, and it cannot by itself prove that a file came from a trusted source. A trusted digest should still come from a trusted project page or release record.

Verify every static library on macOS with:

```bash
find third_party/tflm/lib -name 'libtensorflow-microlite.a' \
  -exec shasum -a 256 {} \;
```

On Linux, normally use:

```bash
find third_party/tflm/lib -name 'libtensorflow-microlite.a' \
  -exec sha256sum {} \;
```

Compare each 64-character output value with the corresponding entry in the table. A match means the file content is identical. A mismatch means the file differs; confirm whether it is corrupted, modified, or the wrong platform build.

## TFLM Compatibility Changes Required by the Current Quantized Model

The prebuilt TFLM static libraries bundled with this repository already contain the following two compatibility changes. No further changes are required when using these libraries directly. The current default implementation in the official GitHub `tflite-micro` repository still lacks the `FLOAT32 -> UINT8` quantization branch and the `UINT8` branches in `CAST`. If you stop using the bundled prebuilt libraries and rebuild from the official source, reapply these two changes or export the model again without the affected nodes.

The currently embedded `ds_tcn_fixed_quantized_backup.tflite` repeatedly uses this path internally, which can be inspected with Netron:

```text
FLOAT32 -> QUANTIZE -> UINT8 -> CAST(UINT8 to UINT8) -> DEQUANTIZE -> FLOAT32
```

Compared with the TFLM base source used for these builds, two type paths must be added:

1. `quantize_common.cc`: add `FLOAT32 -> UINT8` to the floating-point input branch.
2. `cast.cc`: add `UINT8` to both the input and output branches so the model can perform `CAST(UINT8 -> UINT8)` data copies.

The model also contains `INT8 -> FLOAT32` and `UINT8 -> FLOAT32` dequantization paths. The default `Register_DEQUANTIZE()` already supports both paths, so `dequantize.cc` does not need to be modified. The specialized `Register_DEQUANTIZE_INT8()` is intended for operator pruning or model-compiler integration.
