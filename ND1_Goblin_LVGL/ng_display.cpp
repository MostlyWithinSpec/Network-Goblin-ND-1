/* ng_display.cpp - LVGL 9 + ST7796 (Arduino_GFX) + FT6336U touch */
#include "ng_config.h"
#include "ng_display.h"
#include <lvgl.h>
#include <Wire.h>
#include <Arduino_GFX_Library.h>
static Arduino_DataBus *bus = new Arduino_ESP32SPI(NG_LCD_DC, NG_LCD_CS, NG_LCD_SCLK, NG_LCD_MOSI, NG_LCD_MISO);
static Arduino_GFX *gfx = new Arduino_ST7796(bus, NG_LCD_RST, NG_LCD_ROT, true);
static const uint32_t LCD_W = NG_LCD_HOR;
static const uint32_t LCD_H = NG_LCD_VER;
static const uint32_t BUF_LINES = 40;
static lv_display_t *disp;
static uint8_t *buf1 = nullptr;
static void flush_cb(lv_display_t *d, const lv_area_t *area, uint8_t *px) {
  uint32_t w = area->x2 - area->x1 + 1; uint32_t h = area->y2 - area->y1 + 1;
  gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t*)px, w, h);
  lv_display_flush_ready(d);
}
static bool ft_read(uint16_t &x, uint16_t &y) {
  Wire.beginTransmission(NG_TP_ADDR); Wire.write(0x02);
  if (Wire.endTransmission(false)) return false;
  Wire.requestFrom(NG_TP_ADDR, 5);
  if (Wire.available() < 5) return false;
  uint8_t touches = Wire.read() & 0x0F;
  uint8_t xh = Wire.read(), xl = Wire.read();
  uint8_t yh = Wire.read(), yl = Wire.read();
  if (touches == 0) return false;
  x = ((xh & 0x0F) << 8) | xl; y = ((yh & 0x0F) << 8) | yl; return true;
}
static void touch_cb(lv_indev_t *indev, lv_indev_data_t *data) {
  uint16_t x, y;
  if (ft_read(x, y)) { data->point.x = x; data->point.y = y; data->state = LV_INDEV_STATE_PRESSED; }
  else data->state = LV_INDEV_STATE_RELEASED;
}
static uint32_t tick_cb() { return millis(); }
void ngBacklight(int percent) {
#if (NG_LCD_BL >= 0)
  static bool init = false;
  if (!init) { ledcAttach(NG_LCD_BL, 5000, 8); init = true; }
  if (percent < 0) percent = 0; if (percent > 100) percent = 100;
  ledcWrite(NG_LCD_BL, (255 * percent) / 100);
#endif
}
void ngDisplayBegin() {
  gfx->begin(); gfx->fillScreen(0x0000); ngBacklight(100);
  Wire.begin(NG_TP_SDA, NG_TP_SCL, 400000);
#if (NG_TP_RST >= 0)
  pinMode(NG_TP_RST, OUTPUT); digitalWrite(NG_TP_RST, LOW); delay(10); digitalWrite(NG_TP_RST, HIGH); delay(50);
#endif
  lv_init(); lv_tick_set_cb(tick_cb);
  size_t bytes = LCD_W * BUF_LINES * 2;
  buf1 = (uint8_t*)heap_caps_malloc(bytes, MALLOC_CAP_DMA);
  disp = lv_display_create(LCD_W, LCD_H);
  lv_display_set_flush_cb(disp, flush_cb);
  lv_display_set_buffers(disp, buf1, nullptr, bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, touch_cb);
}
void ngDisplayTick() { lv_timer_handler(); }
