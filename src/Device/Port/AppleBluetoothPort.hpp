// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include <memory>
#include <tchar.h>

class BluetoothHelper;
class Port;
class PortListener;
class DataHandler;

/**
 * Open a serial port connection to a Bluetooth LE (GATT) device.
 */
std::unique_ptr<Port>
OpenAppleBleHm10Port(BluetoothHelper &bluetooth_helper,
                     const TCHAR *address, PortListener *_listener,
                     DataHandler &_handler);
