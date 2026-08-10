// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

class BluetoothHelper;

extern BluetoothHelper *bluetooth_helper;

void InitializeAppleServices();
void DeinitializeAppleServices();

/**
 * Keep the app running while a bulk transfer (e.g. an IGC flight
 * download or a task declaration) is in progress: disable the idle
 * timer so the phone does not auto-lock and suspend the app - which
 * would stall all Bluetooth traffic - and request background
 * execution time to survive a brief manual lock or app switch.
 *
 * May be nested; each call must be balanced with
 * EndAppleBulkTransfer().  May only be called from the main thread.
 * No-op on macOS.
 */
void BeginAppleBulkTransfer() noexcept;
void EndAppleBulkTransfer() noexcept;
