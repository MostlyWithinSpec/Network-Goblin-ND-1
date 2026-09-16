/* ng_lldp.h - LLDP/CDP discovery + port flash (on-demand, v3) */
#pragma once
#include <Arduino.h>

struct NgSwitchInfo {
  bool have_lldp;
  bool have_cdp;
  char sys_name[48];
  char port_id[48];
  int  vlan;
  uint32_t last_seen_ms;
};

void ngLldpBegin();          // call once in setup() after ngEthBegin()

// On-demand capture (mutually exclusive with networking):
void ngLldpStart();          // begin capture window (pauses networking)
void ngLldpStop();           // end capture + restart Ethernet (restores net)
void ngLldpEnsureStopped();  // call from other tools before using network
bool ngLldpCapturing();

void ngLldpGet(NgSwitchInfo& out);
bool ngLldpFresh(uint32_t max_age_ms);

void ngLldpPortFlash(int cycles);
bool ngLldpFlashing();
