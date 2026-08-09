// Copyright (c) 2024-2026 Project Beatrice and Contributors

#ifndef BEATRICE_COMMON_SIMPLE_MORPH_H_
#define BEATRICE_COMMON_SIMPLE_MORPH_H_

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <string>
#include <string_view>

#include "common/model_config.h"

namespace beatrice::common {

using SimpleMorphWeights = std::array<float, kMaxNSpeakers>;

[[nodiscard]] inline auto NormalizeSimpleMorphWeights(
    SimpleMorphWeights weights, const int voice_count) -> SimpleMorphWeights {
  const auto count =
      std::clamp(voice_count, 0, static_cast<int>(kMaxNSpeakers));
  for (auto i = 0; i < count; ++i) {
    weights[i] = std::max(0.0f, weights[i]);
  }
  std::fill(weights.begin() + count, weights.end(), 0.0f);

  auto total = 0.0f;
  for (auto i = 0; i < count; ++i) {
    total += weights[i];
  }
  if (count == 0) {
    return weights;
  }
  if (total <= 0.000001f) {
    weights[0] = 1.0f;
    return weights;
  }
  for (auto i = 0; i < count; ++i) {
    weights[i] /= total;
  }
  return weights;
}

[[nodiscard]] inline auto AdjustSimpleMorphWeight(
    SimpleMorphWeights weights, const int voice_count, const int changed_index,
    const float requested_weight) -> SimpleMorphWeights {
  const auto count =
      std::clamp(voice_count, 0, static_cast<int>(kMaxNSpeakers));
  if (changed_index < 0 || changed_index >= count) {
    return NormalizeSimpleMorphWeights(weights, count);
  }
  weights = NormalizeSimpleMorphWeights(weights, count);

  const auto new_weight = std::clamp(requested_weight, 0.0f, 1.0f);
  auto other_total = 0.0f;
  for (auto i = 0; i < count; ++i) {
    if (i != changed_index) {
      other_total += weights[i];
    }
  }
  weights[changed_index] = new_weight;
  const auto remainder = 1.0f - new_weight;
  if (count == 1) {
    weights[changed_index] = 1.0f;
    return weights;
  }
  if (other_total <= 0.000001f) {
    const auto share = remainder / static_cast<float>(count - 1);
    for (auto i = 0; i < count; ++i) {
      if (i != changed_index) {
        weights[i] = share;
      }
    }
  } else {
    const auto scale = remainder / other_total;
    for (auto i = 0; i < count; ++i) {
      if (i != changed_index) {
        weights[i] *= scale;
      }
    }
  }
  return NormalizeSimpleMorphWeights(weights, count);
}

[[nodiscard]] inline auto SerializeSimpleMorphWeights(
    const SimpleMorphWeights& weights, const int voice_count)
    -> std::u8string {
  const auto normalized = NormalizeSimpleMorphWeights(weights, voice_count);
  const auto count =
      std::clamp(voice_count, 0, static_cast<int>(kMaxNSpeakers));
  auto result = std::string();
  for (auto i = 0; i < count; ++i) {
    if (i > 0) {
      result.push_back(',');
    }
    result += std::to_string(normalized[i]);
  }
  return {result.begin(), result.end()};
}

[[nodiscard]] inline auto ParseSimpleMorphWeights(
    const std::u8string_view value) -> SimpleMorphWeights {
  auto weights = SimpleMorphWeights{};
  const auto text = std::string_view(
      reinterpret_cast<const char*>(value.data()), value.size());
  auto begin = std::size_t{0};
  auto index = std::size_t{0};
  while (begin <= text.size() && index < weights.size()) {
    const auto end = text.find(',', begin);
    const auto token =
        text.substr(begin, end == std::string_view::npos ? text.size() - begin
                                                        : end - begin);
    auto parsed = 0.0f;
    const auto parse_result =
        std::from_chars(token.data(), token.data() + token.size(), parsed);
    if (parse_result.ec == std::errc{}) {
      weights[index] = std::max(0.0f, parsed);
    }
    ++index;
    if (end == std::string_view::npos) {
      break;
    }
    begin = end + 1;
  }
  return weights;
}

}  // namespace beatrice::common

#endif  // BEATRICE_COMMON_SIMPLE_MORPH_H_
