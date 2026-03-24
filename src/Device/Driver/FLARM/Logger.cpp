// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Device.hpp"
#include "Device/Error.hpp"
#include "Device/RecordedFlight.hpp"
#include "io/FileOutputStream.hxx"
#include "io/BufferedOutputStream.hxx"
#include "system/Path.hpp"
#include "Operation/Operation.hpp"
#include "LogFile.hpp"
#include "time/TimeoutClock.hpp"

#include <cstdlib>
#include <cstring>
#include <exception>

static bool
ParseDate(const char *str, BrokenDate &date)
{
  char *endptr;

  // Parse year
  date.year = strtoul(str, &endptr, 10);

  // Check if parsed correctly and following character is a separator
  if (str == endptr || *endptr != '-')
    return false;

  // Set str pointer to first character after the separator
  str = endptr + 1;

  // Parse month
  date.month = strtoul(str, &endptr, 10);

  // Check if parsed correctly and following character is a separator
  if (str == endptr || *endptr != '-')
    return false;

  // Set str pointer to first character after the separator
  str = endptr + 1;

  // Parse day
  date.day = strtoul(str, &endptr, 10);

  // Check if parsed correctly and following character is a separator
  return str != endptr;
}

static bool
ParseTime(const char *str, BrokenTime &time)
{
  char *endptr;

  // Parse year
  time.hour = strtoul(str, &endptr, 10);

  // Check if parsed correctly and following character is a separator
  if (str == endptr || *endptr != ':')
    return false;

  // Set str pointer to first character after the separator
  str = endptr + 1;

  // Parse month
  time.minute = strtoul(str, &endptr, 10);

  // Check if parsed correctly and following character is a separator
  if (str == endptr || *endptr != ':')
    return false;

  // Set str pointer to first character after the separator
  str = endptr + 1;

  // Parse day
  time.second = strtoul(str, &endptr, 10);

  // Check if parsed correctly and following character is a separator
  return str != endptr;
}

static BrokenTime
operator+(BrokenTime &a, BrokenTime &b)
{
  BrokenTime c;

  c.hour = a.hour + b.hour;
  c.minute = a.minute + b.minute;
  c.second = a.second + b.second;

  while (c.second >= 60) {
    c.second -= 60;
    c.minute++;
  }

  while (c.minute >= 60) {
    c.minute -= 60;
    c.hour++;
  }

  while (c.hour >= 23)
    c.hour -= 24;

  return c;
}

static bool
ParseRecordInfo(char *record_info, RecordedFlightInfo &flight)
{
  // According to testing with firmware 5.03:
  // 18CG6NG1.IGC|2011-08-12|12:23:48|02:03:25|TOBIAS BIENIEK|TH|Club

  // According to documentation:
  // 2000-11-08|20:05:21|01:21:09|J.Doe|XYZ|15M

  // Where the pilot name may take up to 100 bytes, while class, glider-
  // and competition ID can take up to 32 bytes.

  // Search for first separator
  char *p = strchr(record_info, '|');
  if (p == nullptr)
    return false;

  // Replace separator by \0
  *p = '\0';

  // Move pointer to first character after the replaced separator
  // and check for valid character
  p++;
  if (*p == '\0')
    return false;

  // Check if first field is NOT the date (length > 10)
  if (strlen(record_info) > 10) {
    record_info = p;

    // Search for second separator
    p = strchr(record_info, '|');
    if (p == nullptr)
      return false;

    // Replace separator by \0
    *p = '\0';

    // Move pointer to first character after the replaced separator
    // and check for valid character
    p++;
    if (*p == '\0')
      return false;
  }

  // Now record_info should point to the date field,
  // the date field should be null-terminated and p should
  // point to the start time field and the rest of the null-
  // terminated string

  if (!ParseDate(record_info, flight.date))
    return false;

  record_info = p;

  // Search for next separator
  p = strchr(record_info, '|');
  if (p == nullptr)
    return false;

  // Replace separator by \0
  *p = '\0';

  // Move pointer to first character after the replaced separator
  // and check for valid character
  p++;
  if (*p == '\0')
    return false;

  // Now record_info should point to the start time field,
  // the start time field should be null-terminated and p should
  // point to the duration field and the rest of the null-
  // terminated string

  if (!ParseTime(record_info, flight.start_time))
    return false;

  record_info = p;

  // Search for next separator
  p = strchr(record_info, '|');
  if (p == nullptr)
    return false;

  // Replace separator by \0
  *p = '\0';

  // Move pointer to first character after the replaced separator
  // and check for valid character
  p++;
  if (*p == '\0')
    return false;

  // Now record_info should point to the duration field,
  // the duration field should be null-terminated and p should
  // point to the pilot field and the rest of the null-
  // terminated string

  BrokenTime duration;
  if (!ParseTime(record_info, duration))
    return false;

  flight.end_time = flight.start_time + duration;

  return true;
}

bool
FlarmDevice::ReadFlightInfo(RecordedFlightInfo &flight,
                            OperationEnvironment &env)
{
  // Create header for getting record information
  FLARM::FrameHeader header = PrepareFrameHeader(FLARM::MessageType::GETRECORDINFO);

  // Send request
  SendStartByte();
  SendFrameHeader(header, env, std::chrono::seconds(1));

  // Wait for an answer and save the payload for further processing
  AllocatedArray<std::byte> data;
  uint16_t length;
  const auto ack_result =
    WaitForACKOrNACK(header.sequence_number, data, length,
                     env, std::chrono::seconds(5));

  // If neither ACK nor NACK was received
  if (ack_result != FLARM::MessageType::ACK || length <= 2)
    return false;

  char *record_info = (char *)data.data() + 2;
  return ParseRecordInfo(record_info, flight);
}

FLARM::MessageType
FlarmDevice::SelectFlight(uint8_t record_number, OperationEnvironment &env)
{
  // Create header for selecting a log record
  std::byte data[] = { static_cast<std::byte>(record_number) };
  FLARM::FrameHeader header = PrepareFrameHeader(FLARM::MessageType::SELECTRECORD,
                                                 std::span{data});

  // Send request
  SendStartByte();
  SendFrameHeader(header, env, std::chrono::seconds(1));
  SendEscaped(std::span{data}, env, std::chrono::seconds(1));

  // Wait for an answer
  return WaitForACKOrNACK(header.sequence_number,
                          env, std::chrono::seconds(5));
}

bool
FlarmDevice::ReadFlightList(RecordedFlightList &flight_list,
                            OperationEnvironment &env)
{
  if (!BinaryMode(env))
    return false;

  // Try to receive flight information until the list is full
  for (uint8_t i = 0; !flight_list.full(); ++i) {
    try {
      FLARM::MessageType ack_result = SelectFlight(i, env);

      // Last record reached -> bail out and return list
      if (ack_result == FLARM::MessageType::NACK)
        break;

      // If neither ACK nor NACK was received
      if (ack_result != FLARM::MessageType::ACK) {
        mode = Mode::UNKNOWN;
        return false;
      }

      RecordedFlightInfo flight_info;
      flight_info.internal.flarm = i;
      if (ReadFlightInfo(flight_info, env))
        flight_list.append(flight_info);
    } catch (const DeviceTimeout &) {  }
  }

  return true;
}

bool
FlarmDevice::DownloadFlight(Path path, OperationEnvironment &env)
{
  FileOutputStream fos(path);
  BufferedOutputStream os(fos);

  constexpr auto total_download_timeout = std::chrono::minutes(2);
  constexpr auto packet_timeout = std::chrono::seconds(10);
  constexpr unsigned max_packet_retries = 3;
  const TimeoutClock total_timeout(total_download_timeout);
  unsigned last_progress = 0;
  env.SetProgressRange(100);
  while (true) {
    if (total_timeout.GetRemainingOrZero() <=
        std::chrono::steady_clock::duration::zero()) {
      LogFormat("FLARM download total timeout exceeded (%us)",
                (unsigned)std::chrono::duration_cast<std::chrono::seconds>(
                    total_download_timeout).count());
      throw DeviceTimeout{"FLARM total download timeout"};
    }

    AllocatedArray<std::byte> data;
    uint16_t length;
    FLARM::MessageType ack_result = FLARM::MessageType::ERROR;
    unsigned retry_count = 0;
    uint16_t request_sequence = 0;
    while (retry_count < max_packet_retries) {
      const auto total_remaining = total_timeout.GetRemainingOrZero();
      if (total_remaining <= std::chrono::steady_clock::duration::zero()) {
        LogFormat("FLARM download total timeout reached before request");
        throw DeviceTimeout{"FLARM total download timeout"};
      }

      const auto wait_timeout = std::min(
          std::chrono::duration_cast<std::chrono::steady_clock::duration>(
              packet_timeout),
          total_remaining);

      // Sequence number shall increase for every transmitted frame.
      FLARM::FrameHeader header =
        PrepareFrameHeader(FLARM::MessageType::GETIGCDATA);
      request_sequence = header.sequence_number;
      LogFormat("FLARM download request: seq=%u retry=%u/%u wait_timeout=%us total_remaining=%us",
                (unsigned)request_sequence,
                retry_count + 1, max_packet_retries,
                (unsigned)std::chrono::duration_cast<std::chrono::seconds>(
                    wait_timeout).count(),
                (unsigned)std::chrono::duration_cast<std::chrono::seconds>(
                    total_remaining).count());

      SendStartByte();
      SendFrameHeader(header, env, std::chrono::seconds(1));

      try {
        ack_result = WaitForACKOrNACK(header.sequence_number, data,
                                      length, env,
                                      wait_timeout);
      } catch (const DeviceTimeout &e) {
        ++retry_count;
        LogFormat("FLARM download packet timeout: seq=%u retry=%u/%u timeout=%us total_remaining=%us: %s",
                  (unsigned)request_sequence,
                  retry_count, max_packet_retries,
                  (unsigned)std::chrono::duration_cast<std::chrono::seconds>(
                      wait_timeout).count(),
                  (unsigned)std::chrono::duration_cast<std::chrono::seconds>(
                      total_timeout.GetRemainingOrZero()).count(),
                  e.what());
        if (retry_count < max_packet_retries)
          continue;

        throw;
      }

      if (ack_result == FLARM::MessageType::ACK && length > 3)
        break;

      ++retry_count;
      LogFormat("FLARM download packet failed: seq=%u ack=%u length=%u retry=%u/%u",
                (unsigned)request_sequence,
                (unsigned)ack_result,
                (unsigned)length,
                retry_count, max_packet_retries);
      if (retry_count < max_packet_retries) {
        LogFormat("FLARM download packet retrying with new seq (previous=%u)",
                  (unsigned)request_sequence);
        continue;
      }

      return false;
    }

    length -= 3;

    // Read progress (in percent)
    const auto progress = static_cast<unsigned>(data[2]);
    env.SetProgressPosition(std::min(progress, 100u));
    if (progress != last_progress) {
      LogFormat("FLARM download progress: %u%% (seq=%u payload=%u)",
                progress,
                (unsigned)request_sequence,
                (unsigned)length);
      last_progress = progress;
    }

    const char last_char = (char)data.back();
    bool is_last_packet = (last_char == 0x1A);
    if (is_last_packet)
      length--;

    // Read IGC data
    os.Write({data.data() + 3, length});

    if (is_last_packet)
      break;
  }

  os.Flush();
  fos.Commit();

  return true;
}


bool
FlarmDevice::DownloadFlight(const RecordedFlightInfo &flight,
                            Path path, OperationEnvironment &env)
{
  if (!BinaryMode(env))
    return false;

  FLARM::MessageType ack_result = SelectFlight(flight.internal.flarm, env);

  // If no ACK was received -> cancel
  if (ack_result != FLARM::MessageType::ACK)
    return false;

  try {
    if (DownloadFlight(path, env))
      return true;
    LogFormat("FLARM download failed without exception for record=%u",
              (unsigned)flight.internal.flarm);
  } catch (const DeviceTimeout &e) {
    LogFormat("FLARM download timeout for record=%u: %s",
              (unsigned)flight.internal.flarm, e.what());
    mode = Mode::UNKNOWN;
    throw;
  } catch (const std::exception &e) {
    LogFormat("FLARM download exception for record=%u: %s",
              (unsigned)flight.internal.flarm, e.what());
    mode = Mode::UNKNOWN;
    throw;
  } catch (...) {
    LogFormat("FLARM download unknown exception for record=%u",
              (unsigned)flight.internal.flarm);
    mode = Mode::UNKNOWN;
    throw;
  }

  mode = Mode::UNKNOWN;

  return false;
}
