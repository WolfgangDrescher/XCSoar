// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "OnboardingLinkWindow.hpp"
#include "Widget/WindowWidget.hpp"

class Canvas;

class PreflightWindow final : public OnboardingLinkWindow {
  enum class LinkAction : std::uint8_t {
    CHECKLIST,
    PLANE,
    FLIGHT,
    WIND,
    TASK_MANAGER,
    COUNT
  };

public:
  PreflightWindow() noexcept;
protected:
  void OnPaint(Canvas &canvas) noexcept override;
private:
  unsigned DrawLink(Canvas &canvas, LinkAction link, PixelRect rc, const TCHAR *text) noexcept;
  bool HandleLink(LinkAction link) noexcept;
  bool OnLinkActivated(std::size_t index) noexcept override;
};

class PreflightWidget final : public WindowWidget {
public:
  PixelSize GetMinimumSize() const noexcept override;
  PixelSize GetMaximumSize() const noexcept override;
  void Initialise(ContainerWindow &parent, const PixelRect &rc) noexcept override;
};
