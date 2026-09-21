/* ============================================================
 *  Network Goblin ND-1  -  Arduino + LVGL firmware
 *  "Here be packets."
 * ------------------------------------------------------------
 *  Serial commands: id | link | cable | tdr | pairs | switch 
                  | Flash | ping <host> | trace <host> | help
 * ============================================================ */
#include "ng_config.h"
#include "ng_eth.h"
#include "ng_tdr.h"
#include "ng_pairs.h"
#include "ng_netcmd.h"
#include "ng_display.h"
#include "ng_ui.h"
#include "ng_lldp.h" 
#include "ng_goblin.h"
#include "Preferences.h"

static void banner(){
  Serial.println();
  Serial.println("=====================================");
  Serial.printf (" NETWORK GOBLIN %s  fw %s\n", NG_MODEL, NG_FW);
  Serial.println(" " NG_TAGLINE);
  Serial.println("=====================================");
}

static void serialCmd(){
  static char buf[40]; static uint8_t n=0;
  while(Serial.available()){
    char c=Serial.read();
    if(c=='\n'||c=='\r'){ buf[n]=0;
      if(!strcmp(buf,"id")){ uint32_t id; if(ngPhyId(&id)) Serial.printf("[PHY] ID=0x%08X OK\n",id); else Serial.println("[PHY] read FAILED"); }
      else if(!strcmp(buf,"link")){ NgCable c2; ngCableRun(c2); Serial.printf("[LINK] %s %dMbps %s\n",c2.state,c2.speed_mbps,c2.full_duplex?"FULL":"HALF"); }
      else if(!strcmp(buf,"cable")){ NgCable c2; ngCableRun(c2); Serial.printf("[CABLE] %s len=%s cbln=0x%04X\n",c2.state,c2.length_valid?String(c2.length_ft,0).c_str():"--",c2.raw_cbln); }
      else if(!strcmp(buf,"tdr")){ NgTdrResult r; ngTdrRun(r);
        Serial.printf("MDI : %-6s ~%.1f ft [0x%04X]\n",ngTdrTypeStr(r.mdi.type),r.mdi.dist_ft,r.mdi.raw25);
        Serial.printf("MDIX: %-6s ~%.1f ft [0x%04X]\n",ngTdrTypeStr(r.mdix.type),r.mdix.dist_ft,r.mdix.raw25); }
      else if(!strcmp(buf,"pairs")){ NgWiremap w; ngPairsRun(w);
        Serial.printf("Pair 1-2: %s\n",ngPairStateStr(w.p12.state));
        Serial.printf("Pair 3-6: %s\n",ngPairStateStr(w.p36.state));
        Serial.printf("Pair 4-5: %s\n",ngPairStateStr(w.p45.state));
        Serial.printf("Pair 7-8: %s\n",ngPairStateStr(w.p78.state)); }
      else if(!strncmp(buf,"ping ",5)){ NgPingResult pr;
        if(ngPing(buf+5,4,pr)) Serial.printf("ping %s (%s): %d/%d avg %lums (min %lu/max %lu)\n",buf+5,pr.resolved_ip.c_str(),pr.recv,pr.sent,pr.avg_ms,pr.min_ms,pr.max_ms);
        else Serial.printf("ping %s: no reply\n",buf+5); }
      else if(!strncmp(buf,"trace ",6)){ Serial.printf("traceroute to %s:\n",buf+6);
        ngTraceroute(buf+6,20,[](int hop,const char* ip,uint32_t rtt,bool reached){ Serial.printf(" %2d  %-16s %lums%s\n",hop,ip,rtt,reached?"  <-- dest":""); }); }
    else if(!strcmp(buf,"switch")){
  Serial.println("[SWITCH] listening 45s (networking paused)...");
  ngLldpStart();
  uint32_t t0=millis();
  while(millis()-t0 < 45000 && !ngLldpFresh(60000)){ delay(200); }
  NgSwitchInfo si; ngLldpGet(si);
  if(si.have_lldp||si.have_cdp){
    Serial.printf("[SWITCH] name: %s\n", si.sys_name[0]?si.sys_name:"?");
    Serial.printf("[SWITCH] port: %s\n", si.port_id[0]?si.port_id:"?");
    Serial.printf("[SWITCH] vlan: %d\n", si.vlan);
  } else Serial.println("[SWITCH] nothing seen (try again / check LLDP enabled)");
  ngLldpStop();   // <-- restores networking
}
else if(!strcmp(buf,"flash")){ ngLldpPortFlash(5); }
else if(!strcmp(buf,"netcheck")){ ngLldpEnsureStopped();
  NgNet n; ngEthSnapshot(n); Serial.printf("IP: %s\n", n.got_ip?n.ip.c_str():"none"); }
      else if(!strcmp(buf,"help")){ Serial.println("cmds: id | link | cable | tdr | pairs | switch | Flash | ping <host> | trace <host> | help"); }
      else if(n>0) Serial.println("? help");
      n=0;
    } else if(n<sizeof(buf)-1) buf[n++]=c;
  }
}

void setup(){
  Serial.begin(115200);
  delay(300);

  banner();

  ngGoblinInit();

  Serial.println("[SYS] display...");
  ngDisplayBegin();

  Serial.println("[SYS] ui...");
  ngUiInit();

  ngUiShowSplash();
  ngUiSplashStatus("Initializing Ethernet...");

  Serial.println("[SYS] ethernet...");
  ngEthBegin();

  ngLldpBegin();

  uint32_t id;

  if(ngPhyId(&id)){
    Serial.printf("[PHY] ID=0x%08X swap OK\n",id);

    ngUiSplashStatus(
      "PHY........OK\n"
      "Display....OK\n"
      "Network....OK"
    );
  }

  delay(500);

  ngUiSplashAwake();

  delay(1000);

  ngUiLoadHome();

  Serial.println("[SYS] up. type 'help'. Goblin awake.");
}

void loop(){
  ngDisplayTick();
  ngUiService();
  serialCmd();
}
