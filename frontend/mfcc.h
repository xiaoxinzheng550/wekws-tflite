// Copyright (c) 2017 Personal (Binbin Zhang)
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

// zgx 新增：MFCC 特征提取类，基于 Fbank 实现并添加 DCT 变换

#ifndef FRONTEND_MFCC_H_
#define FRONTEND_MFCC_H_

#include <cstring>
#include <limits>
#include <random>
#include <utility>
#include <vector>

#include "frontend/fft.h"
#include "utils/log.h"

namespace wenet {

// zgx 新增：MFCC 特征提取类
// 计算流程：预加重 -> 加窗 -> FFT -> Power Spectrum -> Mel 滤波器组 -> 对数 -> DCT -> MFCC
class Mfcc {
 public:
  // zgx 新增：构造函数
  // num_ceps: MFCC 系数数量（不包括能量）
  // num_mel_bins: Mel 滤波器组数量
  Mfcc(int num_ceps, int num_mel_bins, int sample_rate, int frame_length,
       int frame_shift)
      : num_ceps_(num_ceps),
        num_mel_bins_(num_mel_bins),
        sample_rate_(sample_rate),
        frame_length_(frame_length),
        frame_shift_(frame_shift),
        use_log_(true),
        remove_dc_offset_(true),
        generator_(0),
        distribution_(0, 1.0),
        dither_(0.0) {
    fft_points_ = UpperPowerOfTwo(frame_length_);
    // zgx 新增：生成 FFT 所需的位反转表和三角函数表
    const int fft_points_4 = fft_points_ / 4;
    bitrev_.resize(fft_points_);
    sintbl_.resize(fft_points_ + fft_points_4);
    make_sintbl(fft_points_, sintbl_.data());
    make_bitrev(fft_points_, bitrev_.data());

    // zgx 新增：创建 Mel 滤波器组
    int num_fft_bins = fft_points_ / 2;
    float fft_bin_width = static_cast<float>(sample_rate_) / fft_points_;
    int low_freq = 20, high_freq = sample_rate_ / 2;
    float mel_low_freq = MelScale(low_freq);
    float mel_high_freq = MelScale(high_freq);
    float mel_freq_delta =
        (mel_high_freq - mel_low_freq) / (num_mel_bins + 1);
    bins_.resize(num_mel_bins);
    center_freqs_.resize(num_mel_bins);
    for (int bin = 0; bin < num_mel_bins; ++bin) {
      float left_mel = mel_low_freq + bin * mel_freq_delta,
            center_mel = mel_low_freq + (bin + 1) * mel_freq_delta,
            right_mel = mel_low_freq + (bin + 2) * mel_freq_delta;
      center_freqs_[bin] = InverseMelScale(center_mel);
      std::vector<float> this_bin(num_fft_bins);
      int first_index = -1, last_index = -1;
      for (int i = 0; i < num_fft_bins; ++i) {
        float freq = (fft_bin_width * i);
        float mel = MelScale(freq);
        if (mel > left_mel && mel < right_mel) {
          float weight;
          if (mel <= center_mel)
            weight = (mel - left_mel) / (center_mel - left_mel);
          else
            weight = (right_mel - mel) / (right_mel - center_mel);
          this_bin[i] = weight;
          if (first_index == -1) first_index = i;
          last_index = i;
        }
      }
      CHECK(first_index != -1 && last_index >= first_index);
      bins_[bin].first = first_index;
      int size = last_index + 1 - first_index;
      bins_[bin].second.resize(size);
      for (int i = 0; i < size; ++i) {
        bins_[bin].second[i] = this_bin[first_index + i];
      }
    }

    // Match torchaudio.compliance.kaldi.mfcc's default Povey window.
    povey_window_.resize(frame_length_);
    double a = M_2PI / (frame_length - 1);
    for (int i = 0; i < frame_length; i++) {
      double i_fl = static_cast<double>(i);
      const double hann = 0.5 - 0.5 * cos(a * i_fl);
      povey_window_[i] = std::pow(hann, 0.85);
    }

    // zgx 新增：预计算 DCT 变换矩阵
    ComputeDctMatrix();
  }

  void set_use_log(bool use_log) { use_log_ = use_log; }

  void set_remove_dc_offset(bool remove_dc_offset) {
    remove_dc_offset_ = remove_dc_offset;
  }

  void set_dither(float dither) { dither_ = dither; }

  int num_ceps() const { return num_ceps_; }

  static inline float InverseMelScale(float mel_freq) {
    return 700.0f * (expf(mel_freq / 1127.0f) - 1.0f);
  }

  static inline float MelScale(float freq) {
    return 1127.0f * logf(1.0f + freq / 700.0f);
  }

  static int UpperPowerOfTwo(int n) {
    return static_cast<int>(pow(2, ceil(log(n) / log(2))));
  }

  // zgx 新增：预加重
  void PreEmphasis(float coeff, std::vector<float>* data) const {
    if (coeff == 0.0) return;
    for (int i = data->size() - 1; i > 0; i--)
      (*data)[i] -= coeff * (*data)[i - 1];
    (*data)[0] -= coeff * (*data)[0];
  }

  // Apply the Povey window used by Kaldi MFCC.
  void Povey(std::vector<float>* data) const {
    CHECK(data->size() >= povey_window_.size());
    for (size_t i = 0; i < povey_window_.size(); ++i) {
      (*data)[i] *= povey_window_[i];
    }
  }

  // zgx 新增：计算 MFCC 特征
  // 返回帧数
  int Compute(const std::vector<float>& wave,
              std::vector<std::vector<float>>* feat);

 private:
  // zgx 新增：计算 DCT 变换矩阵
  void ComputeDctMatrix();

  // zgx 新增：DCT 变换
  void Dct(const std::vector<float>& mel_energies,
           std::vector<float>* mfcc);

  int num_ceps_;              // zgx 新增：MFCC 系数数量
  int num_mel_bins_;          // zgx 新增：Mel 滤波器组数量
  int sample_rate_;
  int frame_length_, frame_shift_;
  int fft_points_;
  bool use_log_;
  bool remove_dc_offset_;
  std::vector<float> center_freqs_;
  std::vector<std::pair<int, std::vector<float>>> bins_;
  std::vector<float> povey_window_;
  std::default_random_engine generator_;
  std::normal_distribution<float> distribution_;
  float dither_;

  // zgx 新增：DCT 变换矩阵
  std::vector<std::vector<float>> dct_matrix_;

  // zgx 新增：位反转表
  std::vector<int> bitrev_;
  // zgx 新增：三角函数表
  std::vector<float> sintbl_;
};

}  // namespace wenet

#endif  // FRONTEND_MFCC_H_
