// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "PortBridge.hpp"
#include "LogFile.hpp"
#include "NativeInputListener.hpp"
#include "NativePortListener.hpp"

#include "Apple/BluetoothHelper.hpp"
#include "Apple/Services.hpp"

#include <span>
#include <stdexcept>
#include <string.h>

PortBridge::PortBridge(const char *address)
  :deviceAddress(address != nullptr ? address : "")
{
  setListener(new NativePortListener());
  setInputListener(new NativeInputListener());
}

void
PortBridge::setListener(PortListener *listener)
{
  portListener.store(listener, std::memory_order_release);
}

void
PortBridge::setInputListener(DataHandler *handler)
{
  inputListener.store(handler, std::memory_order_release);
}

DataHandler *
PortBridge::getInputListener()
{
  return inputListener.load(std::memory_order_acquire);
}

int
PortBridge::getState()
{
  if (deviceAddress.empty() || bluetooth_helper == nullptr)
    return static_cast<int>(PortState::FAILED);

  auto *helper = dynamic_cast<BluetoothHelperIOS *>(bluetooth_helper);
  if (helper == nullptr)
    return static_cast<int>(PortState::FAILED);

  IOSBluetoothManager *manager = helper->getManager();
  if (manager == nil)
    return static_cast<int>(PortState::FAILED);

  NSString *addrStr = [NSString stringWithUTF8String:deviceAddress.c_str()];
  return [manager portStateForDeviceAddress:addrStr];
}

bool
PortBridge::drain()
{
  if (deviceAddress.empty() || bluetooth_helper == nullptr)
    return false;

  auto *helper = dynamic_cast<BluetoothHelperIOS *>(bluetooth_helper);
  if (helper == nullptr)
    return false;

  IOSBluetoothManager *manager = helper->getManager();
  if (manager == nil)
    return false;

  NSString *addrStr = [NSString stringWithUTF8String:deviceAddress.c_str()];
  const BOOL drained = [manager drainWritesForDeviceAddress:addrStr timeoutMs:2000];
  if (!drained) {
    LogFormat("BT PortBridge drain timeout for addr=%s", deviceAddress.c_str());
  }

  return drained;
}

int
PortBridge::getBaudRate() const
{
  return 0;
}

bool
PortBridge::setBaudRate(int baud_rate)
{
  if (baud_rate != 0) {
    LogFormat("BT PortBridge ignores setBaudRate(%d) for addr=%s",
              baud_rate, deviceAddress.c_str());
  }

  return true;
}

std::size_t
PortBridge::write(std::span<const std::byte> src)
{
  if (src.empty())
    return 0;

  if (bluetooth_helper == nullptr) {
    LogFormat("BT PortBridge write failed: bluetooth helper unavailable");
    throw std::runtime_error{"Port write failed"};
  }

  auto *helper = dynamic_cast<BluetoothHelperIOS *>(bluetooth_helper);
  if (helper == nullptr) {
    LogFormat("BT PortBridge write failed: wrong helper type");
    throw std::runtime_error{"Port write failed"};
  }

  IOSBluetoothManager *manager = helper->getManager();
  if (manager == nil) {
    LogFormat("BT PortBridge write failed: manager unavailable");
    throw std::runtime_error{"Port write failed"};
  }

  if (deviceAddress.empty()) {
    LogFormat("BT PortBridge write failed: empty device address");
    throw std::runtime_error{"Port write failed"};
  }

  NSData *data = [NSData dataWithBytes:src.data() length:src.size()];
  NSString *addrStr = [NSString stringWithUTF8String:deviceAddress.c_str()];
  const BOOL success = [manager writeData:data toDeviceAddress:addrStr];
  if (!success) {
    LogFormat("BT PortBridge write failed: queue rejected len=%lu addr=%s",
              (unsigned long)src.size(), deviceAddress.c_str());
    throw std::runtime_error{"Port write failed"};
  }

  return src.size();
}
