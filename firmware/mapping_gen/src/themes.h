#pragma once
#include "microui.h"
static const mu_Color theme_tokyonight[MU_COLOR_MAX] = {
    [MU_COLOR_TEXT]        = {192, 202, 245, 255}, // fg
    [MU_COLOR_BORDER]      = { 65,  72, 104, 000}, // comment
    [MU_COLOR_WINDOWBG]    = { 26,  27,  38, 255}, // bg
    [MU_COLOR_TITLEBG]     = { 36,  40,  59, 255}, // bg_highlight
    [MU_COLOR_TITLETEXT]   = {192, 202, 245, 255},
    [MU_COLOR_PANELBG]     = { 31,  35,  53, 255}, // bg_dark
    [MU_COLOR_BUTTON]      = { 65,  72, 104, 255},
    [MU_COLOR_BUTTONHOVER] = {122, 162, 247, 255}, // blue
    [MU_COLOR_BUTTONFOCUS] = {187, 154, 247, 255}, // purple
    [MU_COLOR_BASE]        = { 22,  22,  30, 255},
    [MU_COLOR_BASEHOVER]   = { 46,  51,  73, 255},
    [MU_COLOR_BASEFOCUS]   = { 65,  72, 104, 255},
    [MU_COLOR_SCROLLBASE]  = { 36,  40,  59, 255},
    [MU_COLOR_SCROLLTHUMB] = {122, 162, 247, 255},
};
