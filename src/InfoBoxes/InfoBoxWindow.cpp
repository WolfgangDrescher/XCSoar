// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "InfoBoxWindow.hpp"
#include "InfoBoxSettings.hpp"
#include "Border.hpp"
#include "Look/InfoBoxLook.hpp"
#include "Look/Colors.hpp"
#include "Input/InputEvents.hpp"
#include "Renderer/GlassRenderer.hpp"
#include "ui/canvas/Brush.hpp"
#include "Renderer/UnitSymbolRenderer.hpp"
#include "Screen/Layout.hpp"
#include "ui/canvas/Canvas.hpp"
#include "ui/event/KeyCode.hpp"
#include "Dialogs/dlgInfoBoxAccess.hpp"
#include "InfoBoxes/InfoBoxManager.hpp"
#include "UIGlobals.hpp"
#include "ui/window/SingleWindow.hpp"
#include "Asset.hpp"

#include <algorithm>

/** timeout of infobox focus */
static constexpr std::chrono::steady_clock::duration FOCUS_TIMEOUT_MAX = std::chrono::seconds(20);

InfoBoxWindow::InfoBoxWindow(ContainerWindow &parent, PixelRect rc,
                             unsigned border_flags,
                             const InfoBoxSettings &_settings,
                             const InfoBoxLook &_look,
                             unsigned _id,
                             WindowStyle style)
  :settings(_settings), look(_look),
   border_kind(border_flags),
   id(_id)
{
  data.Clear();

  Create(parent, rc, style);
}

void
InfoBoxWindow::SetTitle(const char *_title)
{
  data.SetTitle(_title);
  Invalidate(title_rect);
}

void
InfoBoxWindow::PaintTitle(Canvas &canvas)
{
  if (data.title.empty())
    return;

  if (settings.border_style == InfoBoxSettings::BorderStyle::SHADED)
    canvas.DrawFilledRectangle(title_rect, look.caption_background_color);
  else if (settings.border_style == InfoBoxSettings::BorderStyle::OVERLAY)
    ; /* no shaded title bar for rounded style */

  const bool is_selected = HasFocus() || dragging || force_draw_selector;
  if (settings.border_style == InfoBoxSettings::BorderStyle::OVERLAY) {
    if (data.title_color > 0)
      canvas.SetTextColor(look.GetTitleColor(data.title_color));
    else {
      const Color label_color = look.inverse
        ? Color(0xd8, 0xd8, 0xd8)
        : Color(0x30, 0x30, 0x30);
      canvas.SetTextColor(label_color);
    }
  } else if (is_selected)
    canvas.SetTextColor(look.title.fg_color);
  else
    canvas.SetTextColor(look.GetTitleColor(data.title_color));

  const Font &font = (settings.border_style == InfoBoxSettings::BorderStyle::OVERLAY)
    ? look.small_title_font
    : (is_selected ? look.title_font_bold : look.title_font);
  canvas.Select(font);

  PixelSize tsize = canvas.CalcTextSize(data.title);

  int halftextwidth = (title_rect.left + title_rect.right - (int)tsize.width) / 2;
  int x = std::max(1, title_rect.left + halftextwidth);
  int y = title_rect.top;

  canvas.TextAutoClipped({x, y}, data.title);

  if (settings.border_style == InfoBoxSettings::BorderStyle::TAB &&
      halftextwidth > Layout::Scale(3)) {

    const auto pad = Layout::Scale(2);
    const auto text_pad = Layout::GetTextPadding()*2;

    int ytop = title_rect.top + font.GetCapitalHeight() / 2;
    int ytopedge = ytop + pad;
    int ybottom = title_rect.top + Layout::Scale(6)
      + font.GetCapitalHeight();

    canvas.Select(look.border_pen);

    BulkPixelPoint tab[8];
    tab[0].x = tab[1].x = title_rect.left;
    tab[0].y = tab[7].y = ybottom;
    tab[2].x = title_rect.left + pad;
    tab[2].y = tab[5].y = tab[3].y = tab[4].y = ytop;
    tab[1].y = tab[6].y = ytopedge;
    tab[5].x = title_rect.right - pad;
    tab[6].x = tab[7].x = title_rect.right;
    tab[3].x = title_rect.left + halftextwidth - text_pad;
    tab[4].x = title_rect.right - halftextwidth + text_pad;

    canvas.DrawPolyline(tab, 4);
    canvas.DrawPolyline(tab + 4, 4);
  }
}

void
InfoBoxWindow::PaintValue(Canvas &canvas, [[maybe_unused]] Color background_color)
{
  if (data.value.empty())
    return;

  if (settings.border_style == InfoBoxSettings::BorderStyle::OVERLAY) {
    /* use special colors (blue, green, etc.) if set; fall back to
       readable gray for the default case */
    if (data.value_color > 0)
      canvas.SetTextColor(look.GetValueColor(data.value_color));
    else
      canvas.SetTextColor(look.inverse ? COLOR_WHITE : COLOR_BLACK);
  } else
    canvas.SetTextColor(look.GetValueColor(data.value_color));

  canvas.Select(look.value_font);
  int ascent_height = look.value_font.GetAscentHeight();

  PixelSize value_size = canvas.CalcTextSize(data.value);
  if (unsigned(value_size.width + unit_width) > value_rect.GetWidth()) {
    canvas.Select(look.small_value_font);
    ascent_height = look.small_value_font.GetAscentHeight();
    value_size = canvas.CalcTextSize(data.value);
  }

  const PixelSize value_unit_size = value_size + PixelSize{unit_width, 0u};

  auto value_p = value_rect.CenteredTopLeft(value_unit_size);
  if (value_p.x < 0)
    value_p.x = 0;

  canvas.TextAutoClipped(value_p, data.value);

  if (unit_width != 0) {
    const int unit_height =
      UnitSymbolRenderer::GetAscentHeight(look.unit_font, data.value_unit);

    const auto unit_p = value_p.At(value_size.width,
                                   ascent_height - unit_height);

    canvas.Select(look.unit_font);
    UnitSymbolRenderer::Draw(canvas, unit_p,
                             data.value_unit, look.unit_fraction_pen);
  }
}

void
InfoBoxWindow::PaintComment(Canvas &canvas)
{
  if (data.comment.empty())
    return;

  if (settings.border_style == InfoBoxSettings::BorderStyle::OVERLAY) {
    if (data.comment_color > 0)
      canvas.SetTextColor(look.GetCommentColor(data.comment_color));
    else {
      const Color label_color = look.inverse
        ? Color(0xd8, 0xd8, 0xd8)
        : Color(0x30, 0x30, 0x30);
      canvas.SetTextColor(label_color);
    }
  } else
    canvas.SetTextColor(look.GetCommentColor(data.comment_color));

  const Font &font = (settings.border_style == InfoBoxSettings::BorderStyle::OVERLAY)
    ? look.small_title_font
    : look.title_font;
  canvas.Select(font);

  PixelSize tsize = canvas.CalcTextSize(data.comment);

  int x = std::max(1,
                   (comment_rect.left + comment_rect.right - (int)tsize.width) / 2);
  int y = comment_rect.top;

  canvas.TextAutoClipped({x, y}, data.comment);
}

void
InfoBoxWindow::Paint(Canvas &canvas)
{
  const bool is_selected = HasFocus() || dragging || force_draw_selector;
  const Color background_color = pressed
    ? look.pressed_background_color
    : (is_selected
       ? look.focused_background_color
       : look.background_color);

  const PixelRect rc = GetClientRect();
  if (settings.border_style == InfoBoxSettings::BorderStyle::OVERLAY) {
#ifdef ENABLE_OPENGL
    Color box_color;
    if (pressed)
      box_color = look.pressed_background_color;
    else if (is_selected)
      box_color = look.focused_background_color;
    else if (look.inverse)
      box_color = Color(0x28, 0x28, 0x28);
    else
      box_color = Color(0xf8, 0xf8, 0xf8);
    /* When a full-screen dialog (e.g. Settings panel) is open it is the
       paint-loop start point, so the map is not painted and the framebuffer
       behind InfoBoxes contains dialog content.  Alpha-blending would then
       make that dialog bleed through.  Use an opaque fill in that case only;
       small floating dialogs (InfoBox access, MacCready, etc.) are not
       full-screen so the map is still painted first — keep transparency. */
    {
      const auto &mw = UIGlobals::GetMainWindow();
      if (mw.HasDialog() && mw.IsTopDialogFullScreen())
        canvas.DrawFilledRectangle(rc, box_color);
      else
        DrawRoundedDarkBackground(canvas, rc, box_color.WithAlpha(0xe4));
    }
#else
    const Color box_color = pressed
      ? look.pressed_background_color
      : (is_selected
         ? look.focused_background_color
         : (look.inverse ? Color(0x28, 0x28, 0x28) : Color(0xf8, 0xf8, 0xf8)));
    DrawRoundedDarkBackground(canvas, rc, box_color);
#endif
  } else if (settings.border_style == InfoBoxSettings::BorderStyle::GLASS)
    DrawGlassBackground(canvas, rc, background_color);
  else
    canvas.DrawFilledRectangle(rc, background_color);

  if (data.GetCustom() && content) {
    /* if there's no comment, the content object may paint that area,
       too */
    const PixelRect &rc = data.comment.empty()
      ? value_and_comment_rect
      : value_rect;
    content->OnCustomPaint(canvas, rc);
  }

  canvas.SetBackgroundTransparent();

  PaintTitle(canvas);
  PaintComment(canvas);
  PaintValue(canvas, background_color);

  if (border_kind != 0 &&
      settings.border_style != InfoBoxSettings::BorderStyle::OVERLAY) {
    canvas.Select(look.border_pen);

    const int width = canvas.GetWidth(),
      height = canvas.GetHeight();

    if (border_kind & BORDERTOP) {
      canvas.DrawExactLine({0, 0}, {width - 1, 0});
    }

    if (border_kind & BORDERRIGHT) {
      canvas.DrawExactLine({width - 1, 0}, {width - 1, height});
    }

    if (border_kind & BORDERBOTTOM) {
      canvas.DrawExactLine({0, height - 1}, {width - 1, height - 1});
    }

    if (border_kind & BORDERLEFT) {
      canvas.DrawExactLine({0, 0}, {0, height - 1});
    }
  }
}

void
InfoBoxWindow::SetContentProvider(std::unique_ptr<InfoBoxContent> _content)
{
  content = std::move(_content);
  ++content_serial;

  data.SetInvalid();
  Invalidate();
}

void
InfoBoxWindow::UpdateContent()
{
  if (!content)
    return;

  InfoBoxData old = data;
  content->Update(data);
  data.content_serial = content_serial;

  if (old.GetCustom() || data.GetCustom()) {
    if (!data.CompareCustom(old))
      Invalidate();
  } else {
#ifdef ENABLE_OPENGL
    if (!data.CompareTitle(old) || !data.CompareValue(old) ||
        !data.CompareComment(old))
      Invalidate();
#else
    if (!data.CompareTitle(old))
      Invalidate(title_rect);
    if (!data.CompareValue(old))
      Invalidate(value_rect);
    if (!data.CompareComment(old))
      Invalidate(comment_rect);
#endif

    unit_width = UnitSymbolRenderer::GetSize(look.unit_font,
                                             data.value_unit).width;
  }
}

void
InfoBoxWindow::ShowDialog()
{
  force_draw_selector = true;
  Invalidate();

  dlgInfoBoxAccessShowModeless(id, GetDialogContent());

  force_draw_selector = false;
  Invalidate();
  
  FocusParent();
}

bool
InfoBoxWindow::HandleKey(InfoBoxContent::InfoBoxKeyCodes keycode)
{
  if (content && content->HandleKey(keycode)) {
    UpdateContent();
    return true;
  }
  return false;
}

const InfoBoxPanel *
InfoBoxWindow::GetDialogContent() const
{
  if (content)
    return content->GetDialogContent();

  return NULL;
}

void
InfoBoxWindow::OnDestroy() noexcept
{
  focus_timer.Cancel();
  dialog_timer.Cancel();
  PaintWindow::OnDestroy();
}

void
InfoBoxWindow::OnResize(PixelSize new_size) noexcept
{
  PaintWindow::OnResize(new_size);

  PixelRect rc = GetClientRect();

  if (settings.border_style == InfoBoxSettings::BorderStyle::OVERLAY) {
    const int m = Layout::Scale(2);
    rc.left += m;
    rc.right -= m;
    rc.top += m;
    rc.bottom -= m;
  } else {
    if (border_kind & BORDERLEFT)
      rc.left += look.border_width;

    if (border_kind & BORDERRIGHT)
      rc.right -= look.border_width;

    if (border_kind & BORDERTOP)
      rc.top += look.border_width;

    if (border_kind & BORDERBOTTOM)
      rc.bottom -= look.border_width;
  }

  title_rect = rc;
  title_rect.bottom = rc.top + look.title_font.GetHeight();

  comment_rect = rc;
  comment_rect.top = comment_rect.bottom - look.title_font.GetHeight();

  value_rect = rc;
  value_rect.top = title_rect.bottom;
  value_rect.bottom = comment_rect.top;

  value_and_comment_rect = value_rect;
  value_and_comment_rect.bottom = comment_rect.bottom;
}

bool
InfoBoxWindow::OnKeyDown(unsigned key_code) noexcept
{
  /* handle local hot key */

  switch (key_code) {
  case KEY_UP:
    focus_timer.Schedule(FOCUS_TIMEOUT_MAX);
    return HandleKey(InfoBoxContent::ibkUp);

  case KEY_DOWN:
    focus_timer.Schedule(FOCUS_TIMEOUT_MAX);
    return HandleKey(InfoBoxContent::ibkDown);

  case KEY_LEFT:
    focus_timer.Schedule(FOCUS_TIMEOUT_MAX);
    return HandleKey(InfoBoxContent::ibkLeft);

  case KEY_RIGHT:
    focus_timer.Schedule(FOCUS_TIMEOUT_MAX);
    return HandleKey(InfoBoxContent::ibkRight);

  case KEY_RETURN:
    ShowDialog();
    return true;

  case KEY_ESCAPE:
    focus_timer.Cancel();
    FocusParent();
    return true;
  }

  /* handle global hot key */

  if (InputEvents::ProcessKey(InputEvents::MODE_INFOBOX, key_code))
    return true;

  /* call super class */

  return PaintWindow::OnKeyDown(key_code);
}

bool
InfoBoxWindow::OnMouseDown([[maybe_unused]] PixelPoint p) noexcept
{
  dialog_timer.Cancel();

  if (!dragging) {
    dragging = true;
    SetCapture();

    pressed = true;
    Invalidate();

    long_press_pending = true;
    dialog_timer.Schedule(std::chrono::seconds(1));
  }

  return true;
}

bool
InfoBoxWindow::OnMouseUp([[maybe_unused]] PixelPoint p) noexcept
{
  dialog_timer.Cancel();

  if (dragging) {
    const bool was_pressed = pressed;

    dragging = false;
    pressed = false;
    Invalidate();

    ReleaseCapture();

    if (was_pressed) {
      if (long_press_pending) {
        long_press_pending = false;
        
        InfoBoxManager::ClearFocusExcept(id);
        SetFocus();

        const bool click_handled = content != nullptr && content->HandleClick();

        if (!click_handled && GetDialogContent() != nullptr)
          /* delay the dialog opening to prevent double click detection */
          dialog_timer.Schedule(std::chrono::milliseconds(300));
      }
    }

    return true;
  }

  return false;
}

bool
InfoBoxWindow::OnMouseDouble([[maybe_unused]] PixelPoint p) noexcept
{
  dialog_timer.Cancel();
  InputEvents::ShowMenu();
  return true;
}

bool
InfoBoxWindow::OnMouseMove(PixelPoint p, [[maybe_unused]] unsigned keys) noexcept
{
  if (dragging) {
    SetPressed(IsInside(p));
    if (!pressed)
      dialog_timer.Cancel();
    return true;
  }

  return false;
}

void
InfoBoxWindow::OnPaint(Canvas &canvas) noexcept
{
  if (settings.border_style == InfoBoxSettings::BorderStyle::OVERLAY) {
    /* bypass the FBO buffer and paint directly onto the shared
       framebuffer so the map shows through the gaps around the rounded box */
    canvas.SetBackgroundTransparent();
    Paint(canvas);
  } else {
    LazyPaintWindow::OnPaint(canvas);
  }
}

void
InfoBoxWindow::OnPaintBuffer(Canvas &canvas) noexcept
{
  Paint(canvas);
}

void
InfoBoxWindow::OnCancelMode() noexcept
{
  if (dragging) {
    dragging = false;
    pressed = false;
    Invalidate();
    ReleaseCapture();
  }

  dialog_timer.Cancel();
  long_press_pending = false;

  PaintWindow::OnCancelMode();
}

void
InfoBoxWindow::OnSetFocus() noexcept
{
  InfoBoxManager::ClearFocusExcept(id);
  
  PaintWindow::OnSetFocus();

  focus_timer.Schedule(HasCursorKeys() ? FOCUS_TIMEOUT_MAX : std::chrono::milliseconds(1100));

  Invalidate();
}

void
InfoBoxWindow::OnKillFocus() noexcept
{
  PaintWindow::OnKillFocus();

  focus_timer.Cancel();

  Invalidate();
}

void
InfoBoxWindow::OnDialogTimer() noexcept
{
  if (long_press_pending) {
    long_press_pending = false;
    
    dragging = pressed = false;
    Invalidate();
    ReleaseCapture();
    
    InfoBoxManager::ShowInfoBoxPicker(id);
  } else {
    dragging = pressed = false;
    Invalidate();
    ReleaseCapture();
    
    if (GetDialogContent() != nullptr)
      ShowDialog();
  }
}
