#include "../AutoIgnitionColdTimer.h"

#include <assert.h>
#include <stdint.h>
#include <iostream>

namespace {
constexpr float kColdLimitC = 25.0f;
constexpr uint32_t kDelayMs = 60UL * 60UL * 1000UL;

void testRequiresTheFullContinuousHour() {
  AutoIgnitionColdTimer timer;
  const uint32_t start = 10000U;

  assert(!timer.update(start, true, 24.9f, kColdLimitC, false, false));
  assert(timer.countdownActive());
  assert(!timer.update(start + kDelayMs - 1U, true, 24.9f, kColdLimitC, false, false));
  assert(timer.update(start + kDelayMs, true, 24.9f, kColdLimitC, false, false));

  timer.markStarted();
  assert(timer.coldCycleAttempted());
  assert(!timer.update(start + 2U * kDelayMs, true, 20.0f, kColdLimitC, false, false));
}

void testWarmReadingStartsANewColdCycle() {
  AutoIgnitionColdTimer timer;
  assert(!timer.update(1U, true, 24.0f, kColdLimitC, false, false));
  assert(!timer.update(kDelayMs / 2U, true, 25.0f, kColdLimitC, false, false));
  assert(!timer.countdownActive());
  assert(!timer.update(kDelayMs / 2U + 1U, true, 24.0f, kColdLimitC, false, false));
  assert(!timer.update(kDelayMs / 2U + kDelayMs, true, 24.0f, kColdLimitC, false, false));
  assert(timer.update(kDelayMs / 2U + 1U + kDelayMs, true, 24.0f, kColdLimitC, false, false));
}

void testStaleTemperatureResetsTheInterval() {
  AutoIgnitionColdTimer timer;
  assert(!timer.update(1U, true, 20.0f, kColdLimitC, false, false));
  assert(!timer.update(kDelayMs / 2U, false, 20.0f, kColdLimitC, false, false));
  assert(!timer.countdownActive());
  const uint32_t restartedAt = kDelayMs / 2U + 1U;
  assert(!timer.update(restartedAt, true, 20.0f, kColdLimitC, false, false));
  assert(!timer.update(restartedAt + kDelayMs - 1U, true, 20.0f, kColdLimitC, false, false));
  assert(timer.update(restartedAt + kDelayMs, true, 20.0f, kColdLimitC, false, false));
}

void testSafetyAlarmResetsTheInterval() {
  AutoIgnitionColdTimer timer;
  assert(!timer.update(1U, true, 20.0f, kColdLimitC, false, false));
  assert(!timer.update(kDelayMs / 2U, true, 20.0f, kColdLimitC, true, false));
  assert(!timer.countdownActive());
  const uint32_t restartedAt = kDelayMs / 2U + 1U;
  assert(!timer.update(restartedAt, true, 20.0f, kColdLimitC, false, false));
  assert(timer.update(restartedAt + kDelayMs, true, 20.0f, kColdLimitC, false, false));
}

void testManualSharedModeCountsAsTheSingleAttempt() {
  AutoIgnitionColdTimer timer;
  assert(!timer.update(1U, true, 20.0f, kColdLimitC, false, false));
  assert(!timer.update(2U, true, 20.0f, kColdLimitC, false, true));
  assert(timer.coldCycleAttempted());
  assert(!timer.update(2U + kDelayMs, true, 20.0f, kColdLimitC, false, false));
  assert(!timer.update(3U + kDelayMs, true, 25.0f, kColdLimitC, false, false));
  assert(!timer.coldCycleAttempted());
  assert(!timer.update(4U + kDelayMs, true, 20.0f, kColdLimitC, false, false));
}

void testRejectedStartRequiresAnotherFullHour() {
  AutoIgnitionColdTimer timer;
  assert(!timer.update(1U, true, 20.0f, kColdLimitC, false, false));
  assert(timer.update(1U + kDelayMs, true, 20.0f, kColdLimitC, false, false));
  timer.retryAfterFailure();
  assert(!timer.update(2U + kDelayMs, true, 20.0f, kColdLimitC, false, false));
  assert(!timer.update(2U + 2U * kDelayMs - 1U, true, 20.0f, kColdLimitC, false, false));
  assert(timer.update(2U + 2U * kDelayMs, true, 20.0f, kColdLimitC, false, false));
}

void testElapsedTimeHandlesMillisWraparound() {
  AutoIgnitionColdTimer timer;
  const uint32_t start = UINT32_MAX - 500U;
  const uint32_t beforeDeadline = static_cast<uint32_t>(start + kDelayMs - 1U);
  const uint32_t deadline = static_cast<uint32_t>(start + kDelayMs);

  assert(!timer.update(start, true, 20.0f, kColdLimitC, false, false));
  assert(!timer.update(beforeDeadline, true, 20.0f, kColdLimitC, false, false));
  assert(timer.update(deadline, true, 20.0f, kColdLimitC, false, false));
}
}  // namespace

int main() {
  testRequiresTheFullContinuousHour();
  testWarmReadingStartsANewColdCycle();
  testStaleTemperatureResetsTheInterval();
  testSafetyAlarmResetsTheInterval();
  testManualSharedModeCountsAsTheSingleAttempt();
  testRejectedStartRequiresAnotherFullHour();
  testElapsedTimeHandlesMillisWraparound();
  std::cout << "AutoIgnitionColdTimer: all tests passed\n";
}
