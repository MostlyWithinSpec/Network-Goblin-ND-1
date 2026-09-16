/* LAN8742 TDR cable diagnostics */
#pragma once
#include <Arduino.h>
enum NgTdrType { NG_TDR_DEFAULT=0, NG_TDR_SHORT=1, NG_TDR_OPEN=2, NG_TDR_MATCH=3 };
struct NgTdrPair { NgTdrType type; uint8_t rawLen; float dist_m; float dist_ft; uint16_t raw25; };
struct NgTdrResult { NgTdrPair mdi; NgTdrPair mdix; bool ok; };
bool ngTdrRun(NgTdrResult& out);
const char* ngTdrTypeStr(NgTdrType t);
