/* ng_eth.cpp - LAN8742 Ethernet + MDIO + cable length */
#include "ng_config.h"
#include "ng_eth.h"
#include <WiFi.h>
#include <ETH.h>
#include "esp_eth.h"

static volatile bool s_link = false;
static volatile bool s_ip   = false;
static esp_eth_handle_t s_eth = NULL;

#define REG_BMSR 0x01
#define REG_PHYID1 0x02
#define REG_PHYID2 0x03
#define REG_PSCSR 0x1F
#define REG_CBLN 0x1C

static void onEth(WiFiEvent_t e) {
  switch (e) {
    case ARDUINO_EVENT_ETH_START:        ETH.setHostname("network-goblin-nd1"); break;
    case ARDUINO_EVENT_ETH_CONNECTED:    s_link = true;  break;
    case ARDUINO_EVENT_ETH_GOT_IP:       s_ip   = true;  break;
    case ARDUINO_EVENT_ETH_DISCONNECTED: s_link = false; s_ip = false; break;
    default: break;
  }
}

void ngEthBegin() {
  WiFi.onEvent(onEth);
  bool ok = false;
  for (int attempt = 1; attempt <= 5; attempt++) {
    ok = ETH.begin(NG_ETH_PHY_TYPE, NG_ETH_PHY_ADDR, NG_ETH_MDC, NG_ETH_MDIO,
                   NG_ETH_PHY_POWER, NG_ETH_CLK_MODE);
    delay(300);
    uint32_t id = 0;
    if (ok && ngPhyId(&id)) {
      Serial.printf("[ETH] up on attempt %d, PHY=0x%08lX\n", attempt, (unsigned long)id);
      return;
    }
    Serial.printf("[ETH] attempt %d failed, retrying...\n", attempt);
    ETH.end();
    delay(200);
  }
  Serial.println("[ETH] PHY init FAILED after retries");
}

bool ngEthLink()  { return s_link; }
bool ngEthGotIp() { return s_ip; }
String ngEthGateway(){ return s_ip ? ETH.gatewayIP().toString() : String(""); }

void ngEthSnapshot(NgNet& n) {
  n.link_up = s_link; n.got_ip = s_ip;
  n.speed_mbps = ETH.linkSpeed(); n.mac = ETH.macAddress();
  if (s_ip) {
    n.ip=ETH.localIP().toString(); n.gw=ETH.gatewayIP().toString();
    n.mask=ETH.subnetMask().toString(); n.dns=ETH.dnsIP().toString();
  } else { n.ip=n.gw=n.mask=n.dns="--"; }
}

static esp_eth_handle_t getH(){ if(!s_eth) s_eth=ETH.handle(); return s_eth; }
bool ngPhyRead(uint8_t reg, uint16_t* val){
  esp_eth_handle_t h=getH(); if(!h) return false;
  uint32_t v=0; esp_eth_phy_reg_rw_data_t d; d.reg_addr=reg; d.reg_value_p=&v;
  if(esp_eth_ioctl(h,ETH_CMD_READ_PHY_REG,&d)!=ESP_OK) return false;
  *val=(uint16_t)v; return true;
}
bool ngPhyWrite(uint8_t reg, uint16_t val){
  esp_eth_handle_t h=getH(); if(!h) return false;
  uint32_t v=val; esp_eth_phy_reg_rw_data_t d; d.reg_addr=reg; d.reg_value_p=&v;
  return esp_eth_ioctl(h,ETH_CMD_WRITE_PHY_REG,&d)==ESP_OK;
}
bool ngPhyId(uint32_t* out){
  uint16_t a=0,b=0;
  if(!ngPhyRead(REG_PHYID1,&a)||!ngPhyRead(REG_PHYID2,&b)) return false;
  *out=((uint32_t)a<<16)|b; return (*out!=0 && *out!=0xFFFFFFFF);
}
void ngCableRun(NgCable& c){
  memset(&c,0,sizeof(c)); c.state="?";
  uint16_t bmsr=0,ps=0; ngPhyRead(REG_BMSR,&bmsr); ngPhyRead(REG_PSCSR,&ps);
  c.link_up=bmsr&(1<<2);
  switch((ps>>2)&0x7){
    case 0x1:c.speed_mbps=10;c.full_duplex=false;break;
    case 0x2:c.speed_mbps=100;c.full_duplex=false;break;
    case 0x5:c.speed_mbps=10;c.full_duplex=true;break;
    case 0x6:c.speed_mbps=100;c.full_duplex=true;break;
    default:c.speed_mbps=0;c.full_duplex=false;break;
  }
  uint16_t cbln=0; ngPhyRead(REG_CBLN,&cbln); c.raw_cbln=cbln;
  uint8_t idx=(cbln>>12)&0xF;
  if(c.link_up&&c.speed_mbps==100&&idx){ c.length_ft=6.5f*idx*3.28084f; c.length_valid=true; }
  c.state=c.link_up?"GOOD":"no link";
}
