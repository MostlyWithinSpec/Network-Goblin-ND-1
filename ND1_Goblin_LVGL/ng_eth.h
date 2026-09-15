/* ng_eth.h - Ethernet (LAN8742) + PHY MDIO + cable length */
#pragma once
#include <Arduino.h>

struct NgCable {
  const char* state;
  bool  length_valid; float length_ft;
  bool  link_up; int speed_mbps; bool full_duplex;
  uint16_t raw_cbln;
};
struct NgNet {
  bool link_up; bool got_ip;
  String ip, gw, mask, dns, mac;
  int speed_mbps;
};

void ngEthBegin();
bool ngEthLink();
bool ngEthGotIp();
void ngEthSnapshot(NgNet& out);
String ngEthGateway();        // "" if none

bool ngPhyRead(uint8_t reg, uint16_t* val);
bool ngPhyWrite(uint8_t reg, uint16_t val);
bool ngPhyId(uint32_t* out);
void ngCableRun(NgCable& out);
