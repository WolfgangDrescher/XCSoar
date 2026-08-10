// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#import <CoreBluetooth/CoreBluetooth.h>

/**
 * Well-known Bluetooth GATT UUIDs used by XCSoar.  Keep this in sync
 * with android/src/BluetoothUuids.java.
 */
namespace BluetoothUuids {

/**
 * The HM-10 and compatible Bluetooth modules provide a serial port
 * service with this UUID.
 */
[[gnu::pure]]
static inline CBUUID *
Hm10Service() noexcept
{
  static CBUUID *const uuid = [CBUUID UUIDWithString:@"FFE0"];
  return uuid;
}

/**
 * The HM-10 and compatible Bluetooth modules use a GATT
 * characteristic with this UUID for sending and receiving data.
 */
[[gnu::pure]]
static inline CBUUID *
Hm10RxTxCharacteristic() noexcept
{
  static CBUUID *const uuid = [CBUUID UUIDWithString:@"FFE1"];
  return uuid;
}

/**
 * The Nordic UART Service (NUS), a serial port emulation used by many
 * BLE modules, e.g. SoftRF devices.
 */
[[gnu::pure]]
static inline CBUUID *
NordicUartService() noexcept
{
  static CBUUID *const uuid =
    [CBUUID UUIDWithString:@"6E400001-B5A3-F393-E0A9-E50E24DCCA9E"];
  return uuid;
}

/**
 * The Nordic UART "RX" characteristic (from the peripheral's point of
 * view); the central writes data to it.
 */
[[gnu::pure]]
static inline CBUUID *
NordicUartRxCharacteristic() noexcept
{
  static CBUUID *const uuid =
    [CBUUID UUIDWithString:@"6E400002-B5A3-F393-E0A9-E50E24DCCA9E"];
  return uuid;
}

/**
 * The Nordic UART "TX" characteristic (from the peripheral's point of
 * view); the central receives notifications from it.
 */
[[gnu::pure]]
static inline CBUUID *
NordicUartTxCharacteristic() noexcept
{
  static CBUUID *const uuid =
    [CBUUID UUIDWithString:@"6E400003-B5A3-F393-E0A9-E50E24DCCA9E"];
  return uuid;
}

/**
 * The Microchip/ISSC transparent UART service (e.g. BlueFly Vario
 * BLE).
 */
[[gnu::pure]]
static inline CBUUID *
IsscUartService() noexcept
{
  static CBUUID *const uuid =
    [CBUUID UUIDWithString:@"49535343-FE7D-4AE5-8FA9-9FAFD205E455"];
  return uuid;
}

/**
 * The ISSC UART "RX" characteristic; the central writes data to it.
 */
[[gnu::pure]]
static inline CBUUID *
IsscUartRxCharacteristic() noexcept
{
  static CBUUID *const uuid =
    [CBUUID UUIDWithString:@"49535343-8841-43F4-A8D4-ECBE34729BB3"];
  return uuid;
}

/**
 * The ISSC UART "TX" characteristic; the central receives
 * notifications from it.
 */
[[gnu::pure]]
static inline CBUUID *
IsscUartTxCharacteristic() noexcept
{
  static CBUUID *const uuid =
    [CBUUID UUIDWithString:@"49535343-1E4D-4BD9-BA61-23C647249616"];
  return uuid;
}

[[gnu::pure]]
static inline CBUUID *
HeartRateService() noexcept
{
  static CBUUID *const uuid = [CBUUID UUIDWithString:@"180D"];
  return uuid;
}

/* Flytec Sensbox */
[[gnu::pure]]
static inline CBUUID *
FlytecSensboxService() noexcept
{
  static CBUUID *const uuid =
    [CBUUID UUIDWithString:@"ABA27100-143B-4B81-A444-EDCD0000F020"];
  return uuid;
}

/**
 * All service UUIDs which provide a serial port emulation.
 */
[[gnu::pure]]
static inline NSArray<CBUUID *> *
SerialServiceUuids() noexcept
{
  static NSArray<CBUUID *> *const uuids = @[
    Hm10Service(),
    NordicUartService(),
    IsscUartService(),
  ];
  return uuids;
}

} // namespace BluetoothUuids
