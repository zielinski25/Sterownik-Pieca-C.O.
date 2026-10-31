#pragma once

#include <cmath>

/** Automatic mixer enable from fresh sawdust-boiler temperature readings. */
namespace MieszadloTrocinyAuto {

static const float kEnableThresholdC = 30.0f;

struct State {
  // A manual checkbox-off is held until a fresh reading cools to <=30 C.
  bool manualOffUntilCooling;
};

struct UpdateResult {
  bool enabled;
  bool autoEnabled;
  bool lockoutReleased;
};

/** Record an explicit checkbox choice. True means enable the automation. */
inline bool applyManualChoice(State &state, bool enabled) {
  const bool previous = state.manualOffUntilCooling;
  state.manualOffUntilCooling = !enabled;
  return previous != state.manualOffUntilCooling;
}

/**
 * Update the checkbox state from one sensor sample. Invalid or stale readings
 * are ignored completely, so they can neither enable the mixer nor rearm a
 * manual-off latch.
 */
inline UpdateResult update(State &state, bool currentlyEnabled,
                           bool temperatureFresh, float temperatureC) {
  UpdateResult result = {currentlyEnabled, false, false};
  if (!temperatureFresh || !std::isfinite(temperatureC)) return result;

  if (state.manualOffUntilCooling && temperatureC <= kEnableThresholdC) {
    state.manualOffUntilCooling = false;
    result.lockoutReleased = true;
  }

  if (!state.manualOffUntilCooling && !currentlyEnabled &&
      temperatureC > kEnableThresholdC) {
    result.enabled = true;
    result.autoEnabled = true;
  }

  return result;
}

}  // namespace MieszadloTrocinyAuto
