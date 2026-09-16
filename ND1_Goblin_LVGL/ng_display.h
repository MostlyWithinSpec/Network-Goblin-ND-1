/* ng_display.h - LVGL + ST7796 + FT6336U */
#pragma once
#include <Arduino.h>
void ngDisplayBegin();
void ngDisplayTick();
void ngBacklight(int percent);
