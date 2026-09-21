/* ng_goblin.h - Network Goblin personality engine */
#pragma once
#include <Arduino.h>
enum NgGoblinEvent { NGG_IDLE, NGG_CABLE_OK, NGG_CABLE_FAULT, NGG_NET_OK, NGG_NET_FAIL, NGG_SWITCH_OK, NGG_SWITCH_FAIL };
void ngGoblinInit();
const char* ngGoblinBootLine();
const char* ngGoblinLine(NgGoblinEvent event, bool extreme=false);
