#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

// Mapuje te same identyfikatory wyboru, których używa aplikacja Android,
// na krótkie wzory tonów możliwe do wygenerowania przez obecny brzęczyk PWM.
// To nie są próbki WAV: pojedynczy brzęczyk nie odtwarza plików audio.
namespace AlarmBuzzerProfiles {

enum class AlarmKind : uint8_t {
  Smoke = 0,
  Heat = 1,
  Both = 2
};

enum class SoundId : uint8_t {
  Siren = 0,
  Bell = 1,
  Beep = 2,
  TwoTone = 3,
  Pulse = 4,
  Rising = 5,
  LowPulse = 6,
  WavySiren = 7,
  RescueSiren = 8,
  PulsedHorn = 9,
  WarningGong = 10,
  System = 11,
  Count = 12
};

struct ToneStep {
  uint16_t frequencyHz;  // 0 = cisza
  uint16_t durationMs;
};

struct Pattern {
  const ToneStep* steps;
  uint8_t count;
};

static const uint8_t SOUND_COUNT = static_cast<uint8_t>(SoundId::Count);

static const ToneStep kSiren[] = {
  {400, 250}, {0, 50}, {800, 250}, {0, 2050}
};
static const ToneStep kBell[] = {
  {1200, 500}, {1600, 500}
};
static const ToneStep kBeep[] = {
  {1800, 140}, {0, 860}
};
static const ToneStep kTwoTone[] = {
  {650, 300}, {0, 80}, {1050, 300}, {0, 1320}
};
static const ToneStep kPulse[] = {
  {1000, 100}, {0, 120}, {1000, 100}, {0, 680}
};
static const ToneStep kRising[] = {
  {500, 180}, {650, 180}, {800, 180}, {1000, 180}, {0, 900}
};
static const ToneStep kLowPulse[] = {
  {280, 350}, {0, 450}
};
static const ToneStep kWavySiren[] = {
  {500, 180}, {650, 180}, {800, 180}, {950, 180},
  {800, 180}, {650, 180}, {500, 180}, {0, 900}
};
static const ToneStep kRescueSiren[] = {
  {1000, 450}, {0, 80}, {700, 450}, {0, 220}
};
static const ToneStep kPulsedHorn[] = {
  {360, 150}, {0, 150}, {360, 150}, {0, 150},
  {360, 150}, {0, 950}
};
static const ToneStep kWarningGong[] = {
  {1400, 160}, {1050, 260}, {760, 420}, {0, 1160}
};

#define ALARM_BUZZER_PATTERN(name_) \
  { name_, static_cast<uint8_t>(sizeof(name_) / sizeof((name_)[0])) }

static const Pattern kSirenPattern = ALARM_BUZZER_PATTERN(kSiren);
static const Pattern kBellPattern = ALARM_BUZZER_PATTERN(kBell);
static const Pattern kBeepPattern = ALARM_BUZZER_PATTERN(kBeep);
static const Pattern kTwoTonePattern = ALARM_BUZZER_PATTERN(kTwoTone);
static const Pattern kPulsePattern = ALARM_BUZZER_PATTERN(kPulse);
static const Pattern kRisingPattern = ALARM_BUZZER_PATTERN(kRising);
static const Pattern kLowPulsePattern = ALARM_BUZZER_PATTERN(kLowPulse);
static const Pattern kWavySirenPattern = ALARM_BUZZER_PATTERN(kWavySiren);
static const Pattern kRescueSirenPattern = ALARM_BUZZER_PATTERN(kRescueSiren);
static const Pattern kPulsedHornPattern = ALARM_BUZZER_PATTERN(kPulsedHorn);
static const Pattern kWarningGongPattern = ALARM_BUZZER_PATTERN(kWarningGong);

#undef ALARM_BUZZER_PATTERN

inline bool isValidSound(uint8_t value) {
  return value < SOUND_COUNT;
}

inline bool soundIdFromName(const char* name, SoundId& out) {
  if (name == nullptr) return false;
  if (strcmp(name, "syrena") == 0) out = SoundId::Siren;
  else if (strcmp(name, "dzwonek") == 0) out = SoundId::Bell;
  else if (strcmp(name, "pikanie") == 0) out = SoundId::Beep;
  else if (strcmp(name, "dwutonowy") == 0) out = SoundId::TwoTone;
  else if (strcmp(name, "impulsowy") == 0) out = SoundId::Pulse;
  else if (strcmp(name, "narastajacy") == 0) out = SoundId::Rising;
  else if (strcmp(name, "niski_puls") == 0) out = SoundId::LowPulse;
  else if (strcmp(name, "syrena_falujaca") == 0) out = SoundId::WavySiren;
  else if (strcmp(name, "syrena_ratunkowa") == 0) out = SoundId::RescueSiren;
  else if (strcmp(name, "klakson_pulsacyjny") == 0) out = SoundId::PulsedHorn;
  else if (strcmp(name, "gong_ostrzegawczy") == 0) out = SoundId::WarningGong;
  else if (strcmp(name, "system") == 0) out = SoundId::System;
  else return false;
  return true;
}

inline bool alarmKindFromName(const char* name, AlarmKind& out) {
  if (name == nullptr) return false;
  if (strcmp(name, "dym") == 0) out = AlarmKind::Smoke;
  else if (strcmp(name, "ogrz") == 0) out = AlarmKind::Heat;
  else if (strcmp(name, "oba") == 0) out = AlarmKind::Both;
  else return false;
  return true;
}

inline bool parseAlarmSoundSelection(const char* target, const char* soundName, AlarmKind& kind, SoundId& sound) {
  return alarmKindFromName(target, kind) && soundIdFromName(soundName, sound);
}

inline const char* soundName(SoundId sound) {
  switch (sound) {
    case SoundId::Siren: return "syrena";
    case SoundId::Bell: return "dzwonek";
    case SoundId::Beep: return "pikanie";
    case SoundId::TwoTone: return "dwutonowy";
    case SoundId::Pulse: return "impulsowy";
    case SoundId::Rising: return "narastajacy";
    case SoundId::LowPulse: return "niski_puls";
    case SoundId::WavySiren: return "syrena_falujaca";
    case SoundId::RescueSiren: return "syrena_ratunkowa";
    case SoundId::PulsedHorn: return "klakson_pulsacyjny";
    case SoundId::WarningGong: return "gong_ostrzegawczy";
    case SoundId::System: return "system";
    default: return "system";
  }
}

inline SoundId defaultSound(AlarmKind kind) {
  switch (kind) {
    case AlarmKind::Heat: return SoundId::Bell;
    case AlarmKind::Both: return SoundId::TwoTone;
    case AlarmKind::Smoke:
    default: return SoundId::Siren;
  }
}

inline const Pattern& patternFor(SoundId sound, AlarmKind kind) {
  if (sound == SoundId::System) sound = defaultSound(kind);
  switch (sound) {
    case SoundId::Bell: return kBellPattern;
    case SoundId::Beep: return kBeepPattern;
    case SoundId::TwoTone: return kTwoTonePattern;
    case SoundId::Pulse: return kPulsePattern;
    case SoundId::Rising: return kRisingPattern;
    case SoundId::LowPulse: return kLowPulsePattern;
    case SoundId::WavySiren: return kWavySirenPattern;
    case SoundId::RescueSiren: return kRescueSirenPattern;
    case SoundId::PulsedHorn: return kPulsedHornPattern;
    case SoundId::WarningGong: return kWarningGongPattern;
    case SoundId::Siren:
    default: return kSirenPattern;
  }
}

}  // namespace AlarmBuzzerProfiles
