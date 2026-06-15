// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "PCMBufferDataSource.hpp"

#include <algorithm>

unsigned
PCMBufferDataSource::Add(PCMData &&data)
{
  const std::lock_guard protect{lock};

  queued_data.emplace_back(data);
  unsigned size = queued_data.size();
  if (1 == size)
    offset = 0;

  return size;
}

void
PCMBufferDataSource::Clear()
{
  const std::lock_guard protect{lock};
  queued_data.clear();
}

size_t
PCMBufferDataSource::GetData(int16_t *buffer, size_t n)
{
  size_t copied = 0;

  const std::lock_guard protect{lock};

  /* number of samples used to ramp the volume envelope between
     silence and full volume; this avoids a clicking noise when this
     sound starts or stops while being mixed with other audio */
  const size_t fade_samples = std::max<size_t>(1, GetSampleRate() / 200);

  while ((copied < n) && !queued_data.empty()) {
    PCMData &current_pcm_data = queued_data.front();
    size_t current_available = current_pcm_data.size() - offset;
    size_t max = n - copied;
    size_t to_copy = std::min(current_available, max);

    std::copy(current_pcm_data.data() + offset,
              current_pcm_data.data() + offset + to_copy,
              buffer + copied);

    /* total number of samples left in the queue, used to fade out
       towards the end of the last chunk */
    size_t total_remaining = current_available;
    auto it = queued_data.begin();
    for (++it; it != queued_data.end(); ++it)
      total_remaining += it->size();

    for (size_t i = 0; i < to_copy; ++i) {
      const unsigned target =
        total_remaining - i <= fade_samples ? 0 : fade_samples;

      if (fade < target)
        ++fade;
      else if (fade > target)
        --fade;

      buffer[copied + i] = (int32_t)buffer[copied + i] * (int)fade / (int)fade_samples;
    }

    copied += to_copy;
    offset += to_copy;

    if (offset == current_pcm_data.size()) {
      queued_data.pop_front();
      offset = 0;
    }
  }

  return copied;
}
