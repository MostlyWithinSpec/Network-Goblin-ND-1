/* ng_lldp.cpp - LLDP / CDP discovery + port flash  (v3 - on-demand)
 * ------------------------------------------------------------
 *  Capture is ON-DEMAND and MUTUALLY EXCLUSIVE with networking.
 *
 *    ngLldpStart()  -> installs a frame hook that CONSUMES frames.
 *                      Networking (DHCP/ping) is paused while active.
 *    ngLldpStop()   -> removes the hook and RESTARTS Ethernet so the
 *                      normal stack works again. No frame-forwarding
 *                      trickery needed - we just rebuild the driver.
 *
 *  Any other tool (network test, ping, etc.) should call
 *  ngLldpEnsureStopped() first, which restores networking if a
 *  capture was left running.
 *
 *  This trades "both at once" for "rock solid" - exactly the tradeoff
 *  you asked for.
 * ============================================================ */
#include "ng_lldp.h"
#include "ng_config.h"
#include <WiFi.h>
#include <ETH.h>
#include "esp_eth.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

extern bool ngPhyRead(uint8_t reg, uint16_t* val);
extern bool ngPhyWrite(uint8_t reg, uint16_t val);

static NgSwitchInfo s_info;
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static esp_eth_handle_t s_eth = NULL;
static volatile bool s_flashing = false;
static volatile bool s_capturing = false;

static uint16_t be16(const uint8_t* b){ return ((uint16_t)b[0]<<8)|b[1]; }
static void copyStr(char* dst, size_t dstsz, const uint8_t* src, int len){
  if(len<0) len=0; if((size_t)len>=dstsz) len=dstsz-1;
  memcpy(dst,src,len); dst[len]=0;
}

static void parse_lldp(const uint8_t* p, uint32_t length){
  uint32_t i=14; char name[48]={0},port[48]={0}; int vlan=-1; bool got=false;
  while(i+2<=length){
    uint16_t hdr=be16(&p[i]); uint8_t type=(hdr>>9)&0x7F; uint16_t len=hdr&0x1FF;
    i+=2; if(i+len>length) break; const uint8_t* v=&p[i];
    if(type==0) break;
    else if(type==2){ if(len>1) copyStr(port,sizeof(port),v+1,len-1); got=true; }
    else if(type==5){ copyStr(name,sizeof(name),v,len); got=true; }
    else if(type==127 && len>=4){
      uint32_t oui=((uint32_t)v[0]<<16)|((uint32_t)v[1]<<8)|v[2]; uint8_t sub=v[3];
      if(oui==0x0080C2 && sub==0x01 && len>=6){ vlan=be16(&v[4]); got=true; }
    }
    i+=len;
  }
  if(got){
    portENTER_CRITICAL(&s_mux);
    s_info.have_lldp=true;
    if(name[0]) strncpy(s_info.sys_name,name,sizeof(s_info.sys_name));
    if(port[0]) strncpy(s_info.port_id,port,sizeof(s_info.port_id));
    if(vlan>=0) s_info.vlan=vlan;
    s_info.last_seen_ms=millis();
    portEXIT_CRITICAL(&s_mux);
  }
}
static void parse_cdp(const uint8_t* p, uint32_t length){
  uint32_t i=22+4; char name[48]={0},port[48]={0}; int vlan=-1; bool got=false;
  while(i+4<=length){
    uint16_t type=be16(&p[i]); uint16_t len=be16(&p[i+2]);
    if(len<4||i+len>length) break; uint16_t vlen=len-4; const uint8_t* v=&p[i+4];
    if(type==0x0001){ copyStr(name,sizeof(name),v,vlen); got=true; }
    else if(type==0x0003){ copyStr(port,sizeof(port),v,vlen); got=true; }
    else if(type==0x000A && vlen>=2){ vlan=be16(v); got=true; }
    i+=len;
  }
  if(got){
    portENTER_CRITICAL(&s_mux);
    s_info.have_cdp=true;
    if(name[0]) strncpy(s_info.sys_name,name,sizeof(s_info.sys_name));
    if(port[0]) strncpy(s_info.port_id,port,sizeof(s_info.port_id));
    if(vlan>=0) s_info.vlan=vlan;
    s_info.last_seen_ms=millis();
    portEXIT_CRITICAL(&s_mux);
  }
}

// Capture hook: while capturing, we CONSUME every frame (free it).
// Networking is intentionally paused during a capture window.
static esp_err_t capture_hook(esp_eth_handle_t h, uint8_t* buf, uint32_t len, void* priv){
  if(len>=14){
    uint16_t et=be16(&buf[12]);
    if(et==0x88CC) parse_lldp(buf,len);
    else if(buf[0]==0x01&&buf[1]==0x00&&buf[2]==0x0C&&buf[3]==0xCC&&buf[4]==0xCC&&buf[5]==0xCC)
      parse_cdp(buf,len);
  }
  free(buf);          // consume - we are not networking right now
  return ESP_OK;
}

void ngLldpBegin(){
  memset(&s_info,0,sizeof(s_info)); s_info.vlan=-1;
  s_eth = ETH.handle();
  s_capturing = false;
  Serial.println("[LLDP] ready (on-demand capture)");
}

// Start a capture window: install the consuming hook.
void ngLldpStart(){
  if(!s_eth) s_eth = ETH.handle();
  if(!s_eth){ Serial.println("[LLDP] no eth handle"); return; }
  // fresh results for this window
  portENTER_CRITICAL(&s_mux);
  s_info.have_lldp=false; s_info.have_cdp=false; s_info.vlan=-1;
  s_info.sys_name[0]=0; s_info.port_id[0]=0;
  portEXIT_CRITICAL(&s_mux);
  esp_eth_update_input_path(s_eth, capture_hook, NULL);
  s_capturing = true;
  Serial.println("[LLDP] capture started (networking paused)");
}

// Stop capture and RESTORE networking by restarting Ethernet.
void ngLldpStop(){
  if(!s_capturing) return;
  s_capturing = false;
  // Rebuild the Ethernet driver so the normal stack input is restored.
  // (Cleaner than trying to re-inject the default handler ourselves.)
  ETH.end();
  delay(50);
  ETH.begin(NG_ETH_PHY_TYPE, NG_ETH_PHY_ADDR, NG_ETH_MDC, NG_ETH_MDIO,
            NG_ETH_PHY_POWER, NG_ETH_CLK_MODE);
  s_eth = ETH.handle();
  Serial.println("[LLDP] capture stopped, networking restored");
}

// Call this from any other tool before it needs the network.
void ngLldpEnsureStopped(){ if(s_capturing) ngLldpStop(); }

bool ngLldpCapturing(){ return s_capturing; }

void ngLldpGet(NgSwitchInfo& out){
  portENTER_CRITICAL(&s_mux); out=s_info; portEXIT_CRITICAL(&s_mux);
}
bool ngLldpFresh(uint32_t max_age_ms){
  portENTER_CRITICAL(&s_mux);
  bool any=s_info.have_lldp||s_info.have_cdp;
  uint32_t age=millis()-s_info.last_seen_ms;
  portEXIT_CRITICAL(&s_mux);
  return any && (age<=max_age_ms);
}

// ---- port flash ----
static void flashTask(void* arg){
  int cycles=(int)(intptr_t)arg;
  Serial.printf("[FLASH] starting %d cycles (watch the switch port LED)\n", cycles);
  for(int i=0;i<cycles && s_flashing;i++){
    uint16_t bmcr=0;
    ngPhyRead(0,&bmcr);
    ngPhyWrite(0, bmcr | (1<<11));                 // power-down -> link DOWN
    Serial.printf("[FLASH] cycle %d: link DOWN\n", i+1);
    vTaskDelay(pdMS_TO_TICKS(2000));
    ngPhyRead(0,&bmcr);
    ngPhyWrite(0, (bmcr & ~(1<<11)) | (1<<9));      // power-up + restart AN
    Serial.printf("[FLASH] cycle %d: link UP\n", i+1);
    vTaskDelay(pdMS_TO_TICKS(2500));
  }
  Serial.println("[FLASH] done");
  s_flashing=false;
  vTaskDelete(NULL);
}
void ngLldpPortFlash(int cycles){
  if(s_flashing){ Serial.println("[FLASH] already running"); return; }
  if(!s_eth) s_eth = ETH.handle();
  if(!s_eth){ Serial.println("[FLASH] no eth handle"); return; }
  s_flashing=true;
  xTaskCreate(flashTask,"portflash",3072,(void*)(intptr_t)cycles,5,NULL);
}
