#pragma once

#include <stdint.h>

/** RAM-only, one-shot cold-temperature timer for the shared ignition mode. */
class AutoIgnitionColdTimer {
 public:
  static uint32_t requiredDurationMs() { return 60UL * 60UL * 1000UL; }

  /** Returns true once the full fresh, safe, below-threshold interval has elapsed. */
  bool update(uint32_t nowMs, bool temperatureFresh, float temperatureC,
              float thresholdC, bool safetyAlarm, bool sharedModeActive) {
    if (sharedModeActive) coldCycleAttempted_ = true;

    if (!temperatureFresh) {
      clearCountdown();
      return false;
    }
    if (temperatureC >= thresholdC) {
      clearCountdown();
      coldCycleAttempted_ = false;
      return false;
    }
    if (sharedModeActive || safetyAlarm || coldCycleAttempted_) {
      clearCountdown();
      return false;
    }

    if (!countdownActive_) {
      countdownActive_ = true;
      startedAtMs_ = nowMs;
      return false;
    }
    return static_cast<uint32_t>(nowMs - startedAtMs_) >= requiredDurationMs();
  }

  /** Latch a successful manual or automatic start until a fresh warm reading. */
  void markStarted() {
    coldCycleAttempted_ = true;
    clearCountdown();
  }

  /** On a rejected start, require a new complete interval before retrying. */
  void retryAfterFailure() { clearCountdown(); }

  bool coldCycleAttempted() const { return coldCycleAttempted_; }
  bool countdownActive() const { return countdownActive_; }

 private:
  void clearCountdown() {
    countdownActive_ = false;
    startedAtMs_ = 0;
  }

  uint32_t startedAtMs_ = 0;
  bool countdownActive_ = false;
  bool coldCycleAttempted_ = false;
};
