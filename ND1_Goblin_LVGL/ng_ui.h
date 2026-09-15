/* LVGL UI glue */
#pragma once
#include "ng_eth.h"
#include "ng_tdr.h"
#include "ng_pairs.h"
void ngUiInit();
void ngUiLoadHome();
void ngUiPublishCable(const NgCable& c);
void ngUiPublishTdr(const NgTdrResult& r);
void ngUiPublishPairs(const NgWiremap& w);
void ngUiPublishNet(const NgNet& n);
void ngUiOnCable();
void ngUiOnNetwork();
void ngUiOnAutotest();
void ngUiService();
