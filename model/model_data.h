// 模型数据文件 - TFLite Micro 嵌入模型
//
// 此文件包含 TFLite 模型的 C 数组表示
// 使用以下命令从 .tflite 文件生成:
//
//   xxd -i ds_tcn_fixed.tflite > model_data.cc
//
// 或者使用 Python 脚本:
//
//   python -c "
//   import sys
//   with open('ds_tcn_fixed.tflite', 'rb') as f:
//       data = f.read()
//   print(f'const unsigned char g_model_data[] = {{')
//   for i in range(0, len(data), 12):
//       line = ', '.join(f'0x{b:02x}' for b in data[i:i+12])
//       print(f'  {line},' if i + 12 < len(data) else f'  {line}')
//   print('};')
//   print(f'const unsigned int g_model_data_len = {len(data)};')
//   "
//
// 注意: 实际模型数据需要用户根据上述方法生成后替换此文件内容

#ifndef MODEL_MODEL_DATA_H_
#define MODEL_MODEL_DATA_H_

#include <cstdint>

// 模型数据 (需要使用 xxd 或脚本生成后替换)
// extern const unsigned char g_model_data[];
// extern const unsigned int g_model_data_len;

// 占位符 - 实际使用时替换为真实模型数据
// 以下是示例结构，实际模型数据会很大
extern const unsigned char g_model_data[];
extern const unsigned int g_model_data_len;

#endif  // MODEL_MODEL_DATA_H_
