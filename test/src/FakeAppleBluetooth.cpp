// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

/* Stubs for the iOS/macOS Bluetooth LE support: test programs link
   Device/Config.cpp and Device/Port/ConfiguredPort.cpp, which refer
   to these symbols on Apple targets, but the CoreBluetooth
   implementation is not part of the test binaries. */

#ifdef __APPLE__

#include "Apple/BluetoothHelper.hpp"
#include "Device/Port/AppleBluetoothPort.hpp"

#include <stdexcept>

BluetoothHelper *bluetooth_helper;

const char *
BluetoothHelper::GetNameFromAddress(const char *) const noexcept
{
  return nullptr;
}

std::unique_ptr<Port>
OpenAppleBleSerialPort(BluetoothHelper &, const char *,
                       PortListener *, DataHandler &)
{
  throw std::runtime_error("Bluetooth not available");
}

#endif
