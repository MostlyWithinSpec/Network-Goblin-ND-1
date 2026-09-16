/* LAN8742 TDR */
#include "ng_tdr.h"
#include <Arduino.h>
extern bool ngPhyRead(uint8_t reg, uint16_t* val);
extern bool ngPhyWrite(uint8_t reg, uint16_t val);
#define R_BMCR 0
#define R_MMD_CTRL 13
#define R_MMD_DATA 14
#define R_TDR 25
#define R_SCSI 27
#define TDR_ENABLE (1u<<15)
#define TDR_STATUS (1u<<8)
#define TDR_TYPE_SHIFT 9
#define TDR_TYPE_MASK 0x3
#define TDR_LEN_MASK 0xFF
#define SCSI_MDI 0x8000
#define SCSI_MDIX 0xA000
#define BMCR_FORCE_100FD 0x2100
#define MMD_DEVAD 30
#define TDR_MATCH_THR 0x0249
#define TDR_SHORTOPEN_THR 0x0132
#define P_OPEN 1.088f
#define P_SHORT 1.088f
static void mmdWrite(uint16_t addr,uint16_t data){
  ngPhyWrite(R_MMD_CTRL,MMD_DEVAD); ngPhyWrite(R_MMD_DATA,addr);
  ngPhyWrite(R_MMD_CTRL,(1u<<14)|MMD_DEVAD); ngPhyWrite(R_MMD_DATA,data);
}
static uint16_t mmdRead(uint16_t addr){
  uint16_t v=0; ngPhyWrite(R_MMD_CTRL,MMD_DEVAD); ngPhyWrite(R_MMD_DATA,addr);
  ngPhyWrite(R_MMD_CTRL,(1u<<14)|MMD_DEVAD); ngPhyRead(R_MMD_DATA,&v); return v;
}
static bool s_thr=false;
static void initThr(){
  mmdWrite(11,TDR_MATCH_THR); mmdWrite(12,TDR_SHORTOPEN_THR);
  Serial.printf("[TDR] thresholds: 30.11=0x%04X 30.12=0x%04X\n",mmdRead(11),mmdRead(12));
  s_thr=true;
}
static bool oneShot(uint16_t scsi,NgTdrPair& p){
  ngPhyWrite(R_BMCR,BMCR_FORCE_100FD); ngPhyWrite(R_SCSI,scsi);
  delay(300);
  ngPhyWrite(R_BMCR,BMCR_FORCE_100FD); ngPhyWrite(R_TDR,TDR_ENABLE);
  uint16_t v=0; bool done=false;
  for(int i=0;i<80;i++){ delay(10); if(!ngPhyRead(R_TDR,&v))continue; if(v==0xFFFF)continue; if(v&TDR_STATUS){done=true;break;} }
  p.raw25=v; if(!done||v==0xFFFF) return false;
  p.type=(NgTdrType)((v>>TDR_TYPE_SHIFT)&TDR_TYPE_MASK); p.rawLen=v&TDR_LEN_MASK;
  if(p.type==NG_TDR_OPEN||p.type==NG_TDR_SHORT){ float P=(p.type==NG_TDR_SHORT)?P_SHORT:P_OPEN; p.dist_m=p.rawLen*P; p.dist_ft=p.dist_m*3.28084f; }
  else { p.dist_m=0; p.dist_ft=0; }
  return true;
}
static bool tdrPair(uint16_t scsi,NgTdrPair& p){ for(int t=0;t<3;t++){ if(oneShot(scsi,p))return true; delay(50);} return false; }
bool ngTdrRun(NgTdrResult& out){
  out.ok=false; if(!s_thr) initThr();
  bool a=tdrPair(SCSI_MDI,out.mdi); bool b=tdrPair(SCSI_MDIX,out.mdix); out.ok=a&&b;
  ngPhyWrite(R_SCSI,0x0000); ngPhyWrite(R_BMCR,0x1200); return out.ok;
}
const char* ngTdrTypeStr(NgTdrType t){ switch(t){case NG_TDR_SHORT:return "SHORT";case NG_TDR_OPEN:return "OPEN";case NG_TDR_MATCH:return "GOOD";default:return "idle";} }
