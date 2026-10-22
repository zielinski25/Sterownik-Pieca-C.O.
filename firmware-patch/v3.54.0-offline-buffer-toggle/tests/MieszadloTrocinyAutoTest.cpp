#include "../MieszadloTrocinyAuto.h"

#include <assert.h>
#include <limits>
#include <iostream>

namespace {
using MieszadloTrocinyAuto::State;
using MieszadloTrocinyAuto::UpdateResult;

void testEnablesOnlyStrictlyAboveThirtyDegrees() {
  State state = {false};

  UpdateResult atThreshold = MieszadloTrocinyAuto::update(state, false, true, 30.0f);
  assert(!atThreshold.enabled);
  assert(!atThreshold.autoEnabled);

  UpdateResult aboveThreshold = MieszadloTrocinyAuto::update(state, false, true, 30.01f);
  assert(aboveThreshold.enabled);
  assert(aboveThreshold.autoEnabled);
}

void testManualOffHoldsWhileTemperatureRemainsHigh() {
  State state = {false};
  UpdateResult initial = MieszadloTrocinyAuto::update(state, false, true, 31.0f);
  assert(initial.autoEnabled);

  MieszadloTrocinyAuto::applyManualChoice(state, false);
  assert(state.manualOffUntilCooling);

  for (int i = 0; i < 5; ++i) {
    UpdateResult stillHot = MieszadloTrocinyAuto::update(state, false, true, 35.0f);
    assert(!stillHot.enabled);
    assert(!stillHot.autoEnabled);
    assert(state.manualOffUntilCooling);
  }
}

void testFreshReadingAtOrBelowThirtyRearmsTheNextRise() {
  State state = {true};

  UpdateResult atThreshold = MieszadloTrocinyAuto::update(state, false, true, 30.0f);
  assert(!atThreshold.enabled);
  assert(atThreshold.lockoutReleased);
  assert(!state.manualOffUntilCooling);

  UpdateResult laterRise = MieszadloTrocinyAuto::update(state, false, true, 30.1f);
  assert(laterRise.enabled);
  assert(laterRise.autoEnabled);
}

void testStaleOrInvalidSamplesDoNotEnableOrClearTheLatch() {
  State state = {true};

  UpdateResult staleCool = MieszadloTrocinyAuto::update(state, false, false, 20.0f);
  assert(!staleCool.enabled);
  assert(!staleCool.lockoutReleased);
  assert(state.manualOffUntilCooling);

  UpdateResult staleHot = MieszadloTrocinyAuto::update(state, false, false, 40.0f);
  assert(!staleHot.enabled);
  assert(state.manualOffUntilCooling);

  UpdateResult notANumber = MieszadloTrocinyAuto::update(
      state, false, true, std::numeric_limits<float>::quiet_NaN());
  assert(!notANumber.enabled);
  assert(!notANumber.lockoutReleased);
  assert(state.manualOffUntilCooling);

  UpdateResult infinity = MieszadloTrocinyAuto::update(
      state, false, true, std::numeric_limits<float>::infinity());
  assert(!infinity.enabled);
  assert(state.manualOffUntilCooling);
}

void testManualEnableClearsTheManualOffLatch() {
  State state = {true};
  MieszadloTrocinyAuto::applyManualChoice(state, true);
  assert(!state.manualOffUntilCooling);

  UpdateResult remainsEnabled = MieszadloTrocinyAuto::update(state, true, true, 40.0f);
  assert(remainsEnabled.enabled);
  assert(!remainsEnabled.autoEnabled);
}
}  // namespace

int main() {
  testEnablesOnlyStrictlyAboveThirtyDegrees();
  testManualOffHoldsWhileTemperatureRemainsHigh();
  testFreshReadingAtOrBelowThirtyRearmsTheNextRise();
  testStaleOrInvalidSamplesDoNotEnableOrClearTheLatch();
  testManualEnableClearsTheManualOffLatch();
  std::cout << "MieszadloTrocinyAuto: all tests passed\n";
}
