// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "ToneSynthesiser.hpp"
#include "Math/FastTrig.hpp"

#include <cassert>

void
ToneSynthesiser::SetTone(unsigned tone_hz)
{
  target_increment = ISINETABLE.size() * tone_hz / sample_rate;
}

void
ToneSynthesiser::Synthesise(int16_t *buffer, size_t n)
{
  assert(angle < ISINETABLE.size());

  for (int16_t *end = buffer + n; buffer != end; ++buffer) {
    *buffer = ISINETABLE[angle] * (32767 / 1024) * (int)volume / 100;

    /* glide towards the target frequency instead of jumping there
       instantly, to avoid a clicking noise caused by an abrupt
       change of the waveform's slope */
    if (increment < target_increment)
      ++increment;
    else if (increment > target_increment)
      --increment;

    angle = (angle + increment) & (ISINETABLE.size() - 1);
  }
}
