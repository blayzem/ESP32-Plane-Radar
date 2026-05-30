#include "ui/status_screens.h"

#include <lgfx/v1/lgfx_fonts.hpp>

#include <cmath>
#include <cstdio>
#include <cstddef>
#include <cstring>

#include "config.h"
#include "hardware/display.h"
#include "hardware/display_font.h"

namespace fonts = lgfx::v1::fonts;

namespace {

constexpr int kLineGap = 6;
const int kCenterX = config::kDisplayWidth / 2;
const int kCenterY = config::kDisplayHeight / 2;

constexpr int kSpinnerDotCount = 10;
constexpr int kSpinnerRadius = 113;
constexpr int kSpinnerDotRadius = 2;
constexpr int kSpinnerEraseRadius = 4;
constexpr float kSpinnerStepDeg = 6.0f;

struct SpinnerDot {
  int x = 0;
  int y = 0;
  bool drawn = false;
};

char s_connecting_ssid[33];
constexpr int kConnectingTextMaxWidthPx = 220;
constexpr int kMaxSsidDisplayLines = 3;
constexpr size_t kSsidDisplayLineLen = 33;
char s_ssid_display_lines[kMaxSsidDisplayLines][kSsidDisplayLineLen];
int s_ssid_display_line_count = 0;
float s_spinner_angle_deg = -90.0f;
SpinnerDot s_spinner_dots[kSpinnerDotCount];
bool s_connecting_text_drawn = false;

constexpr auto& kGfxTitle = fonts::FreeSans18pt7b;
constexpr auto& kGfxBody = fonts::FreeSans12pt7b;
constexpr auto& kGfxDetail = fonts::Font2;
constexpr auto& kPortalGfxTitle = fonts::FreeSansBold18pt7b;
constexpr auto& kPortalGfxBody = fonts::FreeSansBold12pt7b;
constexpr auto& kPortalGfxEmphasis = fonts::FreeSansBold18pt7b;
constexpr auto& kConnectingGfxTitle = fonts::FreeSansBold12pt7b;
constexpr auto& kConnectingGfxDetail = fonts::FreeSans9pt7b;

struct TextLine {
  const char* text;
  float vlw_size;
  const lgfx::GFXfont* gfx_font;
};

int lineHeightGfx(const lgfx::GFXfont* font) {
  tft.setFont(font);
  return tft.fontHeight();
}

int lineHeightVlw(float size) {
  tft.setTextSize(size);
  return tft.fontHeight();
}

void applyLineStyle(const TextLine& line) {
  if (displayFontIsSmooth()) {
    tft.setTextSize(line.vlw_size);
  } else {
    tft.setFont(line.gfx_font);
  }
}

void drawTextBlock(uint16_t bg, uint16_t fg, const TextLine* lines, size_t count) {
  tft.fillScreen(bg);
  tft.setTextColor(fg, bg);
  tft.setTextDatum(textdatum_t::middle_center);

  int total_h = 0;
  for (size_t i = 0; i < count; ++i) {
    if (displayFontIsSmooth()) {
      total_h += lineHeightVlw(lines[i].vlw_size);
    } else {
      total_h += lineHeightGfx(lines[i].gfx_font);
    }
    if (i + 1 < count) {
      total_h += kLineGap;
    }
  }

  int y = (config::kDisplayHeight - total_h) / 2;
  for (size_t i = 0; i < count; ++i) {
    applyLineStyle(lines[i]);
    const int h =
        displayFontIsSmooth() ? lineHeightVlw(lines[i].vlw_size)
                              : lineHeightGfx(lines[i].gfx_font);
    tft.drawString(lines[i].text, kCenterX, y + h / 2);
    y += h + kLineGap;
  }
}

constexpr float kConnectingTitleVlw = 1.0f;
constexpr float kConnectingDetailVlw = 0.92f;

void applyConnectingTitleStyle() {
  if (displayFontIsSmooth()) {
    tft.setTextSize(kConnectingTitleVlw);
  } else {
    tft.setFont(&kConnectingGfxTitle);
  }
}

void applyConnectingDetailStyle() {
  if (displayFontIsSmooth()) {
    tft.setTextSize(kConnectingDetailVlw);
  } else {
    tft.setFont(&kConnectingGfxDetail);
  }
}

/** Split SSID into centered lines that fit kConnectingTextMaxWidthPx (detail font). */
void layoutConnectingSsidLines() {
  applyConnectingDetailStyle();
  const char* text = s_connecting_ssid;
  const size_t len = strlen(text);
  s_ssid_display_line_count = 0;
  size_t start = 0;
  while (start < len && s_ssid_display_line_count < kMaxSsidDisplayLines) {
    size_t last_fit = start;
    for (size_t end = start + 1; end <= len; ++end) {
      const size_t chunk_len = end - start;
      if (chunk_len >= kSsidDisplayLineLen) {
        break;
      }
      char trial[kSsidDisplayLineLen];
      memcpy(trial, text + start, chunk_len);
      trial[chunk_len] = '\0';
      if (tft.textWidth(trial) <= kConnectingTextMaxWidthPx) {
        last_fit = end;
      } else {
        break;
      }
    }
    size_t take = last_fit - start;
    if (take == 0) {
      take = 1;
    }
    memcpy(s_ssid_display_lines[s_ssid_display_line_count], text + start, take);
    s_ssid_display_lines[s_ssid_display_line_count][take] = '\0';
    ++s_ssid_display_line_count;
    start += take;
  }
  if (s_ssid_display_line_count == 0) {
    strncpy(s_ssid_display_lines[0], text, kSsidDisplayLineLen - 1);
    s_ssid_display_lines[0][kSsidDisplayLineLen - 1] = '\0';
    s_ssid_display_line_count = 1;
  }
}

void drawConnectingText() {
  tft.fillScreen(config::kColorBlack);

  tft.setTextDatum(textdatum_t::middle_center);
  tft.setTextColor(config::kTextOnBlack, config::kColorBlack);

  applyConnectingTitleStyle();
  const int title_h = tft.fontHeight();
  applyConnectingDetailStyle();
  const int detail_h = tft.fontHeight();

  const int detail_lines = 1 + s_ssid_display_line_count;
  const int total_h =
      title_h + kLineGap + detail_h * detail_lines + kLineGap * (detail_lines - 1);
  const int block_top = (config::kDisplayHeight - total_h) / 2;
  constexpr int kPanelPadY = 8;
  tft.fillRect(kCenterX - kConnectingTextMaxWidthPx / 2, block_top - kPanelPadY,
               kConnectingTextMaxWidthPx, total_h + kPanelPadY * 2, config::kColorBlack);

  int y = block_top;
  applyConnectingTitleStyle();
  tft.drawString("Connecting", kCenterX, y + title_h / 2);
  y += title_h + kLineGap;

  applyConnectingDetailStyle();
  tft.drawString("Connecting to", kCenterX, y + detail_h / 2);
  y += detail_h + kLineGap;

  for (int i = 0; i < s_ssid_display_line_count; ++i) {
    tft.drawString(s_ssid_display_lines[i], kCenterX, y + detail_h / 2);
    if (i + 1 < s_ssid_display_line_count) {
      y += detail_h + kLineGap;
    }
  }

  s_connecting_text_drawn = true;
}

void eraseSpinnerDots() {
  for (int i = 0; i < kSpinnerDotCount; ++i) {
    if (!s_spinner_dots[i].drawn) {
      continue;
    }
    tft.fillCircle(s_spinner_dots[i].x, s_spinner_dots[i].y, kSpinnerEraseRadius,
                   config::kColorBlack);
    s_spinner_dots[i].drawn = false;
  }
}

void drawSpinnerDots() {
  constexpr float kDegToRad = 0.01745329252f;
  const float head_rad = s_spinner_angle_deg * kDegToRad;

  for (int i = 0; i < kSpinnerDotCount; ++i) {
    const float a = head_rad - static_cast<float>(i) * (6.283185307f / kSpinnerDotCount);
    const int x = kCenterX + static_cast<int>(std::lround(std::cos(a) * kSpinnerRadius));
    const int y = kCenterY + static_cast<int>(std::lround(std::sin(a) * kSpinnerRadius));

    const int fade = 255 - i * 22;
    const uint16_t color = tft.color565(0, fade, 0);
    tft.fillSmoothCircle(x, y, kSpinnerDotRadius, color);

    s_spinner_dots[i].x = x;
    s_spinner_dots[i].y = y;
    s_spinner_dots[i].drawn = true;
  }
}

}  // namespace

void statusScreenConnectingBegin(const char* ssid) {
  const char* name = (ssid != nullptr && ssid[0] != '\0') ? ssid : "network";
  strncpy(s_connecting_ssid, name, sizeof(s_connecting_ssid) - 1);
  s_connecting_ssid[sizeof(s_connecting_ssid) - 1] = '\0';
  layoutConnectingSsidLines();
  s_spinner_angle_deg = -90.0f;
  for (auto& dot : s_spinner_dots) {
    dot.drawn = false;
  }
  s_connecting_text_drawn = false;
  drawConnectingText();
  drawSpinnerDots();
}

void statusScreenConnectingTick() {
  if (!s_connecting_text_drawn) {
    drawConnectingText();
  }
  eraseSpinnerDots();
  s_spinner_angle_deg += kSpinnerStepDeg;
  if (s_spinner_angle_deg >= 270.0f) {
    s_spinner_angle_deg -= 360.0f;
  }
  drawSpinnerDots();
}

void statusScreenPortal() {
  const TextLine lines[] = {
      {"Wi-Fi setup", 1.15f, &kPortalGfxTitle},
      {"1. Join network:", 1.05f, &kPortalGfxBody},
      {config::kPortalApName, 1.12f, &kPortalGfxEmphasis},
      {"2. Open in browser:", 1.05f, &kPortalGfxBody},
      {config::kPortalHostUrl, 1.12f, &kPortalGfxEmphasis},
      {"or 192.168.4.1", 1.0f, &kPortalGfxBody},
  };
  drawTextBlock(config::kColorYellow, config::kTextOnYellow, lines,
                sizeof(lines) / sizeof(lines[0]));
}

void statusScreenConnectFailed() {
  const TextLine lines[] = {
      {"Could not connect", 1.15f, &kGfxTitle},
      {"Check Wi-Fi password", 1.0f, &kGfxBody},
      {"and signal strength.", 1.0f, &kGfxBody},
      {"Hold BOOT 3 sec", 1.0f, &kGfxBody},
      {"to reset Wi-Fi", 1.0f, &kGfxBody},
  };
  drawTextBlock(config::kColorYellow, config::kTextOnYellow, lines,
                sizeof(lines) / sizeof(lines[0]));
}

void statusScreenWifiReset() {
  const TextLine lines[] = {
      {"Wi-Fi reset", 1.15f, &kPortalGfxTitle},
      {"Restarting...", 1.05f, &kPortalGfxBody},
  };
  drawTextBlock(config::kColorYellow, config::kTextOnYellow, lines,
                sizeof(lines) / sizeof(lines[0]));
}
