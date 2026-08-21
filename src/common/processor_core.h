// Copyright (c) 2024-2026 Project Beatrice and Contributors

#ifndef BEATRICE_COMMON_PROCESSOR_CORE_H_
#define BEATRICE_COMMON_PROCESSOR_CORE_H_

#include <array>
#include <cstring>

#include "common/error.h"
#include "common/model_config.h"
#include "common/output_effects.h"

namespace beatrice::common {

// 任意のサンプリング周波数と任意のブロックサイズで
// Beatrice の推論を行う、ミニマルな信号処理クラス。
// 1 つの子クラスは 1 つのモデルバージョンに対応する。
// このクラスはピッチシフト量やモデルのファイルパスなどの設定の
// 保存と読み込みの機能を提供しないが、外部からそれを可能にするために、
// 状態を設定する異なる種類のメンバ関数の呼び出し順が前後したとしても
// 同じ結果が得られるように実装する必要がある。
// LoadModel() も含めメンバ関数は任意の順で呼ばれる可能性があるため、
// 不整合な状態では Process() 内で処理を行わないなどの対応が必要。
class ProcessorCoreBase {
 public:
  explicit ProcessorCoreBase(const double sample_rate = 0.0)
      : output_effects_(sample_rate) {}
  ProcessorCoreBase(const ProcessorCoreBase&) = delete;
  auto operator=(const ProcessorCoreBase&) -> ProcessorCoreBase& = delete;
  ProcessorCoreBase(ProcessorCoreBase&&) = delete;
  auto operator=(ProcessorCoreBase&&) -> ProcessorCoreBase& = delete;
  virtual ~ProcessorCoreBase() = default;
  [[nodiscard]] virtual auto GetVersion() const -> int = 0;
  [[nodiscard]] virtual auto GetLatencySamples() const -> int { return 0; }
  virtual auto ProcessWithoutConversion(const float* /*input*/,
                                        float* /*output*/, int /*n_samples*/,
                                        float* /*pre_conversion*/ = nullptr)
      -> ErrorCode {
    return ErrorCode::kSuccess;
  }
  virtual auto Process(const float* input, float* output, int n_samples,
                       float* output_right = nullptr,
                       float* pre_conversion = nullptr) -> ErrorCode = 0;
  virtual auto ProcessOutputEffectsTail(float* output, float* output_right,
                                        int n_samples) -> ErrorCode {
    std::memset(output, 0, sizeof(float) * n_samples);
    if (output_right != nullptr) {
      std::memset(output_right, 0, sizeof(float) * n_samples);
    }
    return ErrorCode::kSuccess;
  }
  virtual auto ResetContext() -> ErrorCode { return ErrorCode::kSuccess; }
  virtual auto LoadModel(const ModelConfig& /*config*/,
                         const std::filesystem::path& /*file*/) -> ErrorCode {
    return ErrorCode::kSuccess;
  }

 protected:
  virtual auto SetSampleRate(double /*sample_rate*/) -> ErrorCode {
    return ErrorCode::kSuccess;
  }

 public:
  virtual auto SetTargetSpeaker(int /*target_speaker*/) -> ErrorCode {
    return ErrorCode::kSuccess;
  }
  virtual auto SetFormantShift(double /*formant_shift*/) -> ErrorCode {
    return ErrorCode::kSuccess;
  }
  virtual auto SetPitchShift(double /*pitch_shift*/) -> ErrorCode {
    return ErrorCode::kSuccess;
  }
  virtual auto SetInputGain(double /*input_gain*/) -> ErrorCode {
    return ErrorCode::kSuccess;
  }
  virtual auto SetOutputGain(double /*output_gain*/) -> ErrorCode {
    return ErrorCode::kSuccess;
  }
  virtual auto SetCompensatedDrive(double /*drive*/) -> ErrorCode {
    return ErrorCode::kSuccess;
  }
  virtual auto SetLowCutHz(double /*low_cut_hz*/) -> ErrorCode {
    return ErrorCode::kSuccess;
  }
  virtual auto SetLightDenoise(int /*denoise_mode*/) -> ErrorCode {
    return ErrorCode::kSuccess;
  }
  virtual auto SetDeClickStrength(double /*strength*/) -> ErrorCode {
    return ErrorCode::kSuccess;
  }
  virtual auto SetDeMud(const double amount) -> ErrorCode {
    output_effects_.SetDeMud(amount);
    return ErrorCode::kSuccess;
  }
  virtual auto SetPresence(const double amount) -> ErrorCode {
    output_effects_.SetPresence(amount);
    return ErrorCode::kSuccess;
  }
  virtual auto SetReverbMix(const double mix) -> ErrorCode {
    output_effects_.SetReverbMix(mix);
    return ErrorCode::kSuccess;
  }
  virtual auto SetReverbDecay(const double seconds) -> ErrorCode {
    output_effects_.SetReverbDecay(seconds);
    return ErrorCode::kSuccess;
  }
  virtual auto SetReverbTone(const double tone) -> ErrorCode {
    output_effects_.SetReverbTone(tone);
    return ErrorCode::kSuccess;
  }
  [[nodiscard]] auto HasOutputEffectsTail() const noexcept -> bool {
    return output_effects_.HasReverbTail();
  }
  void DiscardOutputEffectsTail() noexcept {
    output_effects_.DiscardReverbTail();
  }
  virtual auto SetAverageSourcePitch(double /*average_pitch*/) -> ErrorCode {
    return ErrorCode::kSuccess;
  }
  // NOLINTNEXTLINE(readability/casting)
  virtual auto SetIntonationIntensity(double /*intonation_intensity*/)
      -> ErrorCode {
    return ErrorCode::kSuccess;
  }
  virtual auto SetPitchCorrection(double /*pitch_correction*/) -> ErrorCode {
    return ErrorCode::kSuccess;
  }
  // NOLINTNEXTLINE(readability/casting)
  virtual auto SetPitchCorrectionType(int /*pitch_correction_type*/)
      -> ErrorCode {
    return ErrorCode::kSuccess;
  }
  virtual auto SetMinSourcePitch(double /*min_source_pitch*/) -> ErrorCode {
    return ErrorCode::kSuccess;
  }
  virtual auto SetMaxSourcePitch(double /*max_source_pitch*/) -> ErrorCode {
    return ErrorCode::kSuccess;
  }
  virtual auto SetVQNumNeighbors(int /*vq_num_neighbors*/) -> ErrorCode {
    return ErrorCode::kSuccess;
  }

  virtual auto SetSpeakerMorphingWeights(
      const std::array<float, kMaxNSpeakers>& /*weights*/) -> ErrorCode {
    return ErrorCode::kSuccess;
  }

  friend class ProcessorProxy;

 protected:
  void ProcessOutputEffects(float* const output, float* const output_right,
                            const int n_samples) {
    output_effects_.Process(output, output_right, n_samples);
  }
  void SetOutputEffectsSampleRate(const double sample_rate) {
    output_effects_.SetSampleRate(sample_rate);
  }
  void ResetOutputEffects() { output_effects_.Reset(); }

 private:
  OutputEffects output_effects_;
};

// 初期状態やエラー時に使用される ProcessorCore
class ProcessorCoreUnloaded : public ProcessorCoreBase {
 public:
  using ProcessorCoreBase::ProcessorCoreBase;
  [[nodiscard]] auto GetVersion() const -> int override { return -1; }
  [[nodiscard]] auto GetLatencySamples() const -> int override { return 0; }
  auto Process(const float* const /*input*/, float* const output,
               const int n_samples,
               float* const output_right = nullptr,
               float* const /*pre_conversion*/ = nullptr) -> ErrorCode override {
    std::memset(output, 0, sizeof(float) * n_samples);
    if (output_right != nullptr) {
      std::memset(output_right, 0, sizeof(float) * n_samples);
    }
    return ErrorCode::kSuccess;
  }
};
}  // namespace beatrice::common

#endif  // BEATRICE_COMMON_PROCESSOR_CORE_H_
