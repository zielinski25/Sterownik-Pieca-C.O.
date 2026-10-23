#include <assert.h>
#include <string.h>

#include "../AlarmBuzzerProfiles.h"

using namespace AlarmBuzzerProfiles;

static void testSoundIdsRoundTrip() {
  static const char* expected[SOUND_COUNT] = {
      "syrena", "dzwonek", "pikanie", "dwutonowy", "impulsowy", "narastajacy",
      "niski_puls", "syrena_falujaca", "syrena_ratunkowa", "klakson_pulsacyjny",
      "gong_ostrzegawczy", "system"};

  for (uint8_t raw = 0; raw < SOUND_COUNT; ++raw) {
    const SoundId sound = static_cast<SoundId>(raw);
    SoundId parsed = SoundId::Count;
    assert(strcmp(soundName(sound), expected[raw]) == 0);
    assert(soundIdFromName(expected[raw], parsed));
    assert(parsed == sound);
    assert(isValidSound(raw));
  }

  SoundId ignored = SoundId::Siren;
  assert(!soundIdFromName("not-a-sound", ignored));
  assert(!soundIdFromName(nullptr, ignored));
  assert(!isValidSound(SOUND_COUNT));
}

static void testCommandTargetAndSoundValidation() {
  AlarmKind kind = AlarmKind::Smoke;
  SoundId sound = SoundId::Siren;

  assert(parseAlarmSoundSelection("dym", "system", kind, sound));
  assert(kind == AlarmKind::Smoke && sound == SoundId::System);
  assert(parseAlarmSoundSelection("ogrz", "dzwonek", kind, sound));
  assert(kind == AlarmKind::Heat && sound == SoundId::Bell);
  assert(parseAlarmSoundSelection("oba", "dwutonowy", kind, sound));
  assert(kind == AlarmKind::Both && sound == SoundId::TwoTone);

  assert(!parseAlarmSoundSelection("dym+ogrz", "system", kind, sound));
  assert(!parseAlarmSoundSelection("dym", "unknown", kind, sound));
  assert(!alarmKindFromName(nullptr, kind));
  assert(!alarmKindFromName("DYM", kind));
}

static void testEveryToneStepIsBoundedAndNonEmpty() {
  const AlarmKind kinds[] = {AlarmKind::Smoke, AlarmKind::Heat, AlarmKind::Both};
  for (uint8_t raw = 0; raw < SOUND_COUNT; ++raw) {
    const SoundId sound = static_cast<SoundId>(raw);
    for (size_t kindIndex = 0; kindIndex < sizeof(kinds) / sizeof(kinds[0]); ++kindIndex) {
      const Pattern& pattern = patternFor(sound, kinds[kindIndex]);
      assert(pattern.steps != nullptr);
      assert(pattern.count > 0);
      for (uint8_t stepIndex = 0; stepIndex < pattern.count; ++stepIndex) {
        const ToneStep& step = pattern.steps[stepIndex];
        assert(step.durationMs > 0);
        assert(step.frequencyHz == 0 || (step.frequencyHz >= 200 && step.frequencyHz <= 2000));
      }
    }
  }
}

static void testSystemSelectionUsesKindDefault() {
  const Pattern& smoke = patternFor(SoundId::System, AlarmKind::Smoke);
  const Pattern& heat = patternFor(SoundId::System, AlarmKind::Heat);
  const Pattern& both = patternFor(SoundId::System, AlarmKind::Both);
  assert(smoke.steps == patternFor(SoundId::Siren, AlarmKind::Smoke).steps);
  assert(heat.steps == patternFor(SoundId::Bell, AlarmKind::Heat).steps);
  assert(both.steps == patternFor(SoundId::TwoTone, AlarmKind::Both).steps);
}

int main() {
  testSoundIdsRoundTrip();
  testCommandTargetAndSoundValidation();
  testEveryToneStepIsBoundedAndNonEmpty();
  testSystemSelectionUsesKindDefault();
  return 0;
}
