// Copyright (c) 2017 Personal (Binbin Zhang)
// Modifications Copyright (c) 2026 GengXin Zheng (xiaoxinzheng35@foxmail.com)
// Modified by GengXin Zheng in 2026 to add MFCC feature extraction.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// zgx 新增：MFCC 特征提取实现

#include "frontend/mfcc.h"

#include <algorithm>
#include <cmath>

namespace wenet {

// zgx 新增：计算 DCT 变换矩阵
// 使用 DCT-II 公式：C[i][j] = cos(i * π * (j + 0.5) / n)
void Mfcc::ComputeDctMatrix() {
  int n = num_mel_bins_;
  dct_matrix_.resize(num_ceps_);
  for (int i = 0; i < num_ceps_; ++i) {
    dct_matrix_[i].resize(n);
    for (int j = 0; j < n; ++j) {
      dct_matrix_[i][j] = std::cos(M_PI * i * (j + 0.5) / n);
    }
  }
}

// zgx 新增：DCT 变换
// 输入：Mel 滤波器组的对数能量
// 输出：MFCC 系数
void Mfcc::Dct(const std::vector<float>& mel_energies,
               std::vector<float>* mfcc) {
  int n = mel_energies.size();
  for (int i = 0; i < num_ceps_; ++i) {
    float sum = 0.0;
    for (int j = 0; j < n; ++j) {
      sum += mel_energies[j] * dct_matrix_[i][j];
    }
    // Kaldi uses an orthonormal DCT: C0 has sqrt(1/N), while all remaining
    // coefficients have sqrt(2/N). It then applies the default lifter 22.
    const float dct_scale =
        (i == 0) ? std::sqrt(1.0f / n) : std::sqrt(2.0f / n);
    const float lifter =
        1.0f + 0.5f * 22.0f * std::sin(M_PI * i / 22.0f);
    (*mfcc)[i] = sum * dct_scale * lifter;
  }
}

// zgx 新增：计算 MFCC 特征
// 流程：预加重 -> 加窗 -> FFT -> Power Spectrum -> Mel 滤波器组 -> 对数 -> DCT
int Mfcc::Compute(const std::vector<float>& wave,
                  std::vector<std::vector<float>>* feat) {
  int num_samples = wave.size();
  if (num_samples < frame_length_) return 0;
  int num_frames = 1 + ((num_samples - frame_length_) / frame_shift_);
  feat->resize(num_frames);

  // zgx 新增：FFT 所需的缓冲区
  std::vector<float> fft_real(fft_points_, 0), fft_img(fft_points_, 0);
  std::vector<float> power(fft_points_ / 2);

  // zgx 新增：Mel 滤波器组能量缓冲区
  std::vector<float> mel_energies(num_mel_bins_);

  for (int i = 0; i < num_frames; ++i) {
    // zgx 新增：提取当前帧
    std::vector<float> data(wave.data() + i * frame_shift_,
                            wave.data() + i * frame_shift_ + frame_length_);

    // zgx 新增：添加噪声（dither）
    if (dither_ != 0.0) {
      for (size_t j = 0; j < data.size(); ++j)
        data[j] += dither_ * distribution_(generator_);
    }

    // zgx 新增：去直流偏移
    if (remove_dc_offset_) {
      float mean = 0.0;
      for (size_t j = 0; j < data.size(); ++j) mean += data[j];
      mean /= data.size();
      for (size_t j = 0; j < data.size(); ++j) data[j] -= mean;
    }

    // zgx 新增：预加重
    PreEmphasis(0.97, &data);

    // Match torchaudio.compliance.kaldi.mfcc(window_type="povey").
    Povey(&data);

    // zgx 新增：FFT 准备
    memset(fft_img.data(), 0, sizeof(float) * fft_points_);
    memset(fft_real.data() + frame_length_, 0,
           sizeof(float) * (fft_points_ - frame_length_));
    memcpy(fft_real.data(), data.data(), sizeof(float) * frame_length_);

    // zgx 新增：FFT 变换
    fft(bitrev_.data(), sintbl_.data(), fft_real.data(), fft_img.data(),
        fft_points_);

    // zgx 新增：计算 Power Spectrum
    for (int j = 0; j < fft_points_ / 2; ++j) {
      power[j] = fft_real[j] * fft_real[j] + fft_img[j] * fft_img[j];
    }

    // zgx 新增：应用 Mel 滤波器组并计算对数能量
    for (int j = 0; j < num_mel_bins_; ++j) {
      float mel_energy = 0.0;
      int s = bins_[j].first;
      for (size_t k = 0; k < bins_[j].second.size(); ++k) {
        mel_energy += bins_[j].second[k] * power[s + k];
      }

      // zgx 新增：对数运算
      if (use_log_) {
        if (mel_energy < std::numeric_limits<float>::epsilon())
          mel_energy = std::numeric_limits<float>::epsilon();
        mel_energy = logf(mel_energy);
      }

      mel_energies[j] = mel_energy;
    }

    // zgx 新增：DCT 变换得到 MFCC 系数
    (*feat)[i].resize(num_ceps_);
    Dct(mel_energies, &(*feat)[i]);
  }
  return num_frames;
}

}  // namespace wenet
