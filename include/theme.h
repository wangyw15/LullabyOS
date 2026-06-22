#ifndef THEME_H
#define THEME_H

#include <M5GFX.h>

static const uint32_t C_BG         = 0x000000;
static const uint32_t C_TEXT       = 0xFFFFFF;
static const uint32_t C_ACCENT     = 0x19D1FF;
static const uint32_t C_DARK_PANEL = 0x111111;
static const uint32_t C_GRAY       = 0x666666;
static const uint32_t C_DIM        = 0x333333;

static const lgfx::v1::IFont* UI_FONT      = &fonts::Font0;
static const lgfx::v1::IFont* UI_FONT_BIG  = &fonts::Font2;

#endif // THEME_H
