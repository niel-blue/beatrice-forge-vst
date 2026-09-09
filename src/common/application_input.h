// Copyright (c) 2026 Project Beatrice and Contributors

#ifndef BEATRICE_COMMON_APPLICATION_INPUT_H_
#define BEATRICE_COMMON_APPLICATION_INPUT_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace beatrice::common {

// A Windows audio session which can be selected as an Application Input
// source.  The process id is intentionally runtime-only; identity is the
// executable path and is the value suitable for persistence.  Inactive
// sessions are retained so a paused player does not immediately disappear
// from the selector.
struct ApplicationInputInfo {
  std::string identity;
  std::string display_name;
  std::uint32_t process_id = 0;
  bool active = false;
};

struct ApplicationInputSnapshot {
  std::vector<ApplicationInputInfo> applications;
  std::string error;
};

struct ApplicationInputExclusions {
  // Process ids are preferred because they identify the current DAW/VST host
  // instance without hiding another instance of the same executable.
  std::vector<std::uint32_t> process_ids;
  // Identities are a fallback for a process whose PID is not available yet,
  // and for excluding the current executable in the standalone client.
  std::vector<std::string> identities;
};

// Enumerates processes which own a render audio session.  This is deliberately
// not a complete process list: an application without an audio session cannot
// be selected until Windows has created one for it.  Both active and inactive
// sessions are retained; expired sessions are ignored.
auto EnumerateApplicationInputs(
    const ApplicationInputExclusions& exclusions = {})
    -> ApplicationInputSnapshot;

// Returns the current process executable identity in the same normalized
// form used by ApplicationInputInfo::identity.
auto CurrentApplicationInputIdentity() -> std::string;

// Returns the current process id.  In a VST this is the DAW/VST host process;
// in the standalone build it is Beatrice itself.
auto CurrentApplicationInputProcessId() -> std::uint32_t;

// Builds the common exclusion set for the current process.  Both frontends
// use this so the host/self exclusion rule cannot drift between them.
auto CurrentApplicationInputExclusions() -> ApplicationInputExclusions;

// Keeps a persisted selection visible even when its process has stopped or
// has not been launched yet.  A remembered entry has process_id == 0 and is
// resolved to a live PID on the next refresh.
auto MergeRememberedApplicationInput(
    ApplicationInputSnapshot snapshot, std::string_view identity,
    const ApplicationInputExclusions& exclusions = {})
    -> ApplicationInputSnapshot;

// Resolve a persisted executable identity against a fresh runtime snapshot.
auto FindApplicationInput(const ApplicationInputSnapshot& snapshot,
                          const std::string& identity)
    -> std::optional<ApplicationInputInfo>;

enum class ApplicationInputStatus {
  kOff,
  kStarting,
  kActive,
  kUnavailable,
};

// Captures one selected process tree through the Windows process-loopback
// WASAPI source.  Read() is a realtime-safe, non-blocking FIFO read; it never
// waits for the Windows audio worker and fills unavailable frames with zero.
class ApplicationInput final {
 public:
  using StatusCallback =
      std::function<void(ApplicationInputStatus, const std::string&)>;

  explicit ApplicationInput(StatusCallback callback = {});
  ~ApplicationInput();

  ApplicationInput(const ApplicationInput&) = delete;
  auto operator=(const ApplicationInput&) -> ApplicationInput& = delete;

  auto Start(std::uint32_t process_id, double output_sample_rate) -> bool;
  void Stop();

  // Returns the number of frames read from the source before any missing
  // frames were zero-filled. The method itself always writes count frames.
  auto Read(float* left, float* right, std::size_t count) noexcept
      -> std::size_t;

  [[nodiscard]] auto GetStatus() const noexcept -> ApplicationInputStatus;
  [[nodiscard]] auto GetLastError() const -> std::string;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace beatrice::common

#endif  // BEATRICE_COMMON_APPLICATION_INPUT_H_
