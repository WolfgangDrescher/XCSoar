// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "VarioSynthesiser.hpp"
#include "Math/FastMath.hpp"

#include <algorithm>
#include <cassert>

/**
 * The minimum and maximum vario range for the constants below [cm/s].
 */
static constexpr int min_vario = -500, max_vario = 500;

unsigned
VarioSynthesiser::VarioToFrequency(int ivario)
{
  return ivario > 0
    ? (zero_frequency + (unsigned)ivario * (max_frequency - zero_frequency)
       / (unsigned)max_vario)
    : (zero_frequency - (unsigned)(ivario * (int)(zero_frequency - min_frequency) / min_vario));
}

void
VarioSynthesiser::SetVario(double vario)
{
  const std::lock_guard lock{mutex};

  const int ivario = std::clamp((int)(vario * 100), min_vario, max_vario);

  if (dead_band_enabled && InDeadBand(ivario)) {
    /* inside the "dead band" */
    UnsafeSetSilence();
    return;
  }

  /* update the ToneSynthesiser base class */
  SetTone(VarioToFrequency(ivario));

  if (ivario > 0) {
    /* while climbing, the vario sound gets interrupted by silence
       periodically */

    const unsigned period_ms = sample_rate
      * (min_period_ms + (max_vario - ivario)
         * (max_period_ms - min_period_ms) / max_vario)
      / 1000;

    silence_count = period_ms / 3;
    audible_count = period_ms - silence_count;

    /* preserve the old "_remaining" values as much as possible, to
       avoid chopping off the previous tone */

    if (audible_remaining > audible_count)
      audible_remaining = audible_count;

    if (silence_remaining > silence_count)
      silence_remaining = silence_count;
  } else {
    /* continuous tone while sinking */
    audible_count = 1;
    silence_count = 0;
  }
}

void
VarioSynthesiser::SetSilence()
{
  const std::lock_guard lock{mutex};
  UnsafeSetSilence();
}

void
VarioSynthesiser::UnsafeSetSilence()
{
  audible_count = 0;
  silence_count = 1;

  /* quit the current period as early as possible; the volume
     envelope in Synthesise() will fade out smoothly to avoid
     clicking noise */
  audible_remaining = 0;
  silence_remaining = 0;
}


void
VarioSynthesiser::Synthesise(int16_t *buffer, size_t n)
{
  const std::lock_guard lock{mutex};

  assert(audible_count > 0 || silence_count > 0);

  /* generate the raw tone for the whole buffer; the volume envelope
     below fades it in and out to avoid clicking noise */
  ToneSynthesiser::Synthesise(buffer, n);

  /* number of samples used to ramp the volume envelope between
     silence and full volume; applying this envelope continuously
     (regardless of the waveform phase) avoids clicking noise both
     at the begin/end of each beep and at abrupt mode changes
     (e.g. when switching between climbing and sinking) */
  const unsigned fade_samples = std::max(1u, sample_rate / 200);

  for (size_t i = 0; i < n; ++i) {
    unsigned target;

    if (silence_count == 0)
      /* continuous tone while sinking */
      target = fade_samples;
    else {
      if (audible_remaining == 0 && silence_remaining == 0) {
        /* period finished, begin next one */
        audible_remaining = audible_count;
        silence_remaining = silence_count;
      }

      if (audible_remaining > 0) {
        --audible_remaining;
        target = fade_samples;
      } else {
        --silence_remaining;
        target = 0;
      }
    }

    if (fade < target)
      ++fade;
    else if (fade > target)
      --fade;

    buffer[i] = (int32_t)buffer[i] * (int)fade / (int)fade_samples;
  }
}
