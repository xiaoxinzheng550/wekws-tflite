# TensorFlow Lite Micro 依赖说明

本目录保存 `wekws-tflite` 编译和运行时使用的 TensorFlow Lite Micro（TFLM）
文件，包括：

```text
third_party/tflm/
├── include/   # 编译 C++ 程序时使用的头文件
├── lib/       # 针对不同系统、CPU和C运行库预编译的静态库
├── LICENSE    # TFLM 许可证
├── README.md  # 英文说明
└── README_cn.md
```

TFLM 负责加载 `.tflite` 模型、分配 Tensor Arena、执行模型算子。这里的
`libtensorflow-microlite.a` 是静态库：编译 `kws_main` 时，链接器会把程序
实际使用的 TFLM 代码合并进最终可执行文件，运行时不需要再安装 TensorFlow、
NumPy 或动态链接库。

## 预编译静态库

同一个静态库不能跨平台通用。操作系统、CPU 架构、编译器 ABI 或 C 运行库不
匹配时，可能出现“文件格式不识别”“符号未定义”或程序无法启动等问题。

| 运行平台 | C 运行库/ABI | 仓库内的静态库 | SHA-256 |
| --- | --- | --- | --- |
| macOS Apple Silicon | macOS arm64 | `lib/macos-arm64/libtensorflow-microlite.a` | `7e78cc5d80207e88eece4fa5c4b5fb553cf65322b463c4dc4a6cf181f38e69e1` |
| Linux x86_64 | glibc | `lib/linux-x86_64/libtensorflow-microlite.a` | `1d2140b1d4f0751409bbdfc9e507820dd94f4ff556ed13b67af0ff81089b6925` |
| Linux ARMv7 | glibc、gnueabihf | `lib/linux-armv7-gnueabihf/libtensorflow-microlite.a` | `d553265f01accaa73f3c22e457ad4f9144f146b551024a528fced6dda066bc02` |
| Linux ARMv7 | musl、musleabihf | `lib/linux-armv7-musleabihf/libtensorflow-microlite.a` | `e9cdf01b1ae974775fe5058f94b49be9c9b2e69b046502d6d9fa9a97bf2f214` |


## CMake 如何选择静态库

项目目前会自动选择以下两种静态库：

- Apple Silicon Mac：`lib/macos-arm64/libtensorflow-microlite.a`
- Linux x86_64：`lib/linux-x86_64/libtensorflow-microlite.a`

ARMv7 库虽然已放在仓库中，但构建脚本不会猜测目标设备使用 glibc 还是 musl，
需要显式指定。例如，ARMv7 glibc 工具链使用：

```bash
cmake -S . -B build-arm \
  -DCMAKE_TOOLCHAIN_FILE=/absolute/path/to/armv7-toolchain.cmake \
  -DTFLM_LIBRARY="$PWD/third_party/tflm/lib/linux-armv7-gnueabihf/libtensorflow-microlite.a"
cmake --build build-arm --parallel
```

ARMv7 musl 工具链则将路径改为：

```text
third_party/tflm/lib/linux-armv7-musleabihf/libtensorflow-microlite.a
```

指定静态库只能解决文件选择问题，不能把本机编译器变成交叉编译器。编译 ARMv7
程序时仍然需要配置对应的交叉编译工具链。

## SHA-256 是什么

SHA-256 是一种文件摘要算法，可以把任意大小的文件计算成固定长度的 64 位
十六进制字符串。例如表格中的：

```text
7e78cc5d80207e88eece4fa5c4b5fb553cf65322b463c4dc4a6cf181f38e69e1
```

可以把它理解为文件的“数字指纹”：

- 两个文件内容完全相同，计算出的 SHA-256 也相同。
- 文件哪怕只改变一个字节，结果通常也会完全不同。
- 它可以发现下载不完整、文件损坏或拿错版本。
- 它不是文件大小、版本号，也不能单独证明文件来源绝对可信；可信的校验值仍应
  来自可信的项目页面或发布说明。

在 macOS 上校验全部静态库：

```bash
find third_party/tflm/lib -name 'libtensorflow-microlite.a' \
  -exec shasum -a 256 {} \;
```

Linux 通常使用：

```bash
find third_party/tflm/lib -name 'libtensorflow-microlite.a' \
  -exec sha256sum {} \;
```

将命令输出的 64 位字符串与表格中对应平台的一项比较即可。相同表示文件内容
一致，不同表示文件不是同一份，应先确认是否损坏、被修改或选择了错误版本。

## 当前量化模型需要对 tflite-micro依赖静态库做兼容性修改（待补充）
GitHub 官方 `tflite-micro` 当前默认实现仍缺少上述 `FLOAT32 -> UINT8` 量化分支和 `CAST` 的 `UINT8` 分支。因此，如果以后不使用本仓库的预编译静态库，而是从官方源码重新构建，也要应用这两项兼容性修改，或者重新导出模型以消除这些节点。

当前嵌入的 `ds_tcn_fixed_quantized_backup.tflite` 在模型内部重复使用以下路径（可以用netron查看模型结构）：

```text
FLOAT32 -> QUANTIZE -> UINT8 -> CAST(UINT8到UINT8) -> DEQUANTIZE -> FLOAT32
```

相对于所用的 TFLM 基础源码，需要补充两项类型支持：

1. `quantize_common.cc`：在浮点输入分支增加 `FLOAT32 -> UINT8`。
2. `cast.cc`：在输入和输出分支都增加 `UINT8`，支持模型中的
   `CAST(UINT8 -> UINT8)` 数据复制。

模型还包含 `INT8 -> FLOAT32` 和 `UINT8 -> FLOAT32` 两种反量化。默认
`Register_DEQUANTIZE()` 已经支持这两条路径，因此不需要修改
`dequantize.cc`。专用的 `Register_DEQUANTIZE_INT8()` 属于算子裁剪或模型编译器
适配。



