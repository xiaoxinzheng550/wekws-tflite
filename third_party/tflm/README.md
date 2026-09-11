# Bundled TensorFlow Lite Micro files

These headers and prebuilt libraries were copied from
`runtime/tflite_micro_runtime/fc_base/tflite_micro`. The headers and library
binaries were not modified. Library names were normalized by target platform.

| Target | Original filename | SHA-256 |
| --- | --- | --- |
| macOS arm64 | `libtensorflow-microlite.a` | `7e78cc5d80207e88eece4fa5c4b5fb553cf65322b463c4dc4a6cf181f38e69e1` |
| Linux x86_64 | `libtensorflow-microlite.a.linux_x86_64.bak` | `1d2140b1d4f0751409bbdfc9e507820dd94f4ff556ed13b67af0ff81089b6925` |
| Linux ARMv7 glibc | `libtensorflow-microlite-armv7.a` | `d553265f01accaa73f3c22e457ad4f9144f146b551024a528fced6dda066bc02` |
| Linux ARMv7 musl | `libtensorflow-microlite-armv7-musl.a` | `e9cdf01b1ae974775fe5058f94b49be9c9b2e69b046502d6d9fa9aa97bf2f214` |

The extra original file `libtensorflow-microlite.a.arm32` was not referenced by
the original CMake configuration and its ABI cannot be identified reliably
from its filename. It is therefore not selected or distributed as a default
target library. Pass `-DTFLM_LIBRARY=/absolute/path/to/library.a` if that exact
artifact is required and its ABI has been verified for the target toolchain.
