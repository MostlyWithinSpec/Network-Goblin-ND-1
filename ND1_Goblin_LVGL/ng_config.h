/* ============================================================
 *  Network Goblin ND-1  -  Arduino + LVGL
 *  ng_config.h  -  ALL pins / tunables in one place. EDIT FIRST.
 * ============================================================ */
#define LV_LVGL_H_INCLUDE_SIMPLE 1
#pragma once

// ---- identity ----
#define NG_MODEL     "ND-1"
#define NG_FW        "0.4.0-arduino"
#define NG_TAGLINE   "Here be packets."

// ============================================================
//  ETHERNET  -  ESP32 EMAC + LAN8742 (RMII)
//  RMII data pins FIXED: TXD0=19 TXD1=22 TX_EN=21 RXD0=25 RXD1=26 CRS_DV=27
// ============================================================
#define NG_ETH_MDC        23
#define NG_ETH_MDIO       18
#define NG_ETH_PHY_POWER  -1
#define NG_ETH_PHY_ADDR   1
#define NG_ETH_PHY_TYPE   ETH_PHY_LAN8720
#define NG_ETH_CLK_MODE   ETH_CLOCK_GPIO0_IN

// ============================================================
//  DISPLAY  -  ST7796U, 320x480, 4-wire SPI  
// ============================================================
#define NG_LCD_SCLK   14
#define NG_LCD_MOSI   13
#define NG_LCD_MISO   35
#define NG_LCD_DC     2
#define NG_LCD_CS     15
#define NG_LCD_RST    4
#define NG_LCD_BL     17
#define NG_LCD_HOR    320
#define NG_LCD_VER    480
#define NG_LCD_ROT    0

// ============================================================
//  TOUCH  -  FT6336U capacitive, I2C
// ============================================================
#define NG_TP_SDA     32
#define NG_TP_SCL     33
#define NG_TP_RST     -1
#define NG_TP_INT     34
#define NG_TP_ADDR    0x38
