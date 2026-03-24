// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "Device/Port/State.hpp"
#include "LogFile.hpp"
#include "NativeInputListener.hpp"
#include "NativePortListener.hpp"
#include <atomic>
#include <cstddef>
#include <span>
#include <string>

class PortBridge {
public:
  PortBridge(const char *deviceAddress);

  void setListener(PortListener *listener);
  void setInputListener(DataHandler *handler);
  DataHandler *getInputListener();

  int getState();

  bool drain();

  int getBaudRate() const;

  bool setBaudRate(int baud_rate);

  virtual std::size_t write(std::span<const std::byte> src);

private:
  std::atomic<PortListener *> portListener{nullptr};
  std::atomic<DataHandler *> inputListener{nullptr};
  std::string deviceAddress;
};

class iOSPortBridge : public PortBridge {
public:
  std::size_t write(std::span<const std::byte> src) override;
};
