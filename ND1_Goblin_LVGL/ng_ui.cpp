/* ng_ui.cpp - HAND-CODED LVGL screens
 *   Home / Cable / Network / Autotest / Switch Info / Tools
 * ------------------------------------------------------------
 *  - dark-mode crash fix (deferred rebuild + screen deletion)
 *  - Switch Info screen: Identify (LLDP/CDP) + Flash Port LED
 *  - Tools: units, theme, TDR calibration, logs, and About
 * ============================================================ */
#include "ng_config.h"
#include "ng_ui.h"
#include "ng_eth.h"
#include "ng_tdr.h"
#include "ng_pairs.h"
#include "ng_netcmd.h"
#include "ng_lldp.h"
#include "ng_goblin.h"
#include <lvgl.h>
#include <Arduino.h>
#include <Preferences.h>

static bool g_dark = false;
static bool g_extreme = false;
static lv_color_t C(uint32_t h){ return lv_color_hex(h); }
static lv_color_t col_bg()     { return C(g_dark?0x070b09:0xf2f5f4); }
static lv_color_t col_surf()   { return C(g_dark?0x101713:0xffffff); }
static lv_color_t col_text()   { return C(g_dark?0xd8ffe4:0x17211d); }
static lv_color_t col_muted()  { return C(g_dark?0x7ebc91:0x66736d); }
static lv_color_t col_primary(){ return C(g_extreme?0x00ff55:(g_dark?0x39ff77:0x176b4d)); }
static lv_color_t col_border() { return C(g_dark?0x245d38:0xc9d4cf); }
static lv_color_t col_pass()   { return C(0x21b35b); }
static lv_color_t col_fail()   { return C(0xdd3f3f); }
static lv_color_t col_idle()   { return C(0x77847e); }

#ifndef NG_FIRMWARE_VERSION
#define NG_FIRMWARE_VERSION "1.0.2"
#endif
#define NG_DEVICE_MODEL "Network Goblin ND-1"
#define NG_CREATOR_NAME "MostlyWithinSpec"
#define NG_LICENSE_NAME "MIT License"

static bool  g_use_meters = false;
static bool  g_logs_enabled = true;
static float g_tdr_calibration = 1.000f;

static Preferences g_prefs;
static void saveSettings()
{
    g_prefs.begin("ng-ui", false);

    g_prefs.putBool("dark", g_dark);
    g_prefs.putBool("meters", g_use_meters);
    g_prefs.putBool("logs", g_logs_enabled);
    g_prefs.putBool("extreme", g_extreme);

    g_prefs.putFloat("tdrcal", g_tdr_calibration);

    g_prefs.end();
}

static void loadSettings()
{
    g_prefs.begin("ng-ui", true);

    g_dark         = g_prefs.getBool("dark", false);
    g_use_meters   = g_prefs.getBool("meters", false);
    g_logs_enabled = g_prefs.getBool("logs", true);
    g_extreme      = g_prefs.getBool("extreme", false);

    g_tdr_calibration =
        g_prefs.getFloat("tdrcal", 1.000f);

    g_prefs.end();
}


#if LV_FONT_MONTSERRAT_20
  #define F_HEAD &lv_font_montserrat_20
#else
  #define F_HEAD LV_FONT_DEFAULT
#endif
#if LV_FONT_MONTSERRAT_28
  #define F_TITLE &lv_font_montserrat_28
#else
  #define F_TITLE LV_FONT_DEFAULT
#endif

static lv_obj_t *scr_home=NULL,*scr_cable=NULL,*scr_net=NULL,*scr_auto=NULL,*scr_switch=NULL,*scr_tools=NULL,*scr_splash=NULL,*scr_dev=NULL;
static int cur=0;
static lv_obj_t *lbl_length,*lbl_fault,*badge_fault;
static lv_obj_t *lbl_p12,*lbl_p36,*lbl_p45,*lbl_p78;
static lv_obj_t *lbl_ip,*lbl_mask,*lbl_gw,*lbl_dns,*lbl_link,*badge_dhcp;
static lv_obj_t *lbl_netresult;
static lv_obj_t *lbl_a_cable,*lbl_a_len,*lbl_a_gw,*lbl_a_inet;
static lv_obj_t *lbl_sw_name,*lbl_sw_port,*lbl_sw_vlan,*lbl_sw_status;
static lv_obj_t *dd_units=NULL,*dd_theme=NULL,*sw_logs=NULL;
static lv_obj_t *spin_tdr_cal=NULL,*lbl_cal_value=NULL,*about_overlay=NULL;
static lv_obj_t *lbl_goblin=NULL,*lbl_splash_status=NULL,*lbl_dev=NULL;
static uint8_t logo_taps=0; static uint32_t last_logo_tap=0;

static void buildAll();
static volatile bool run_cable=false, run_net=false, run_auto2=false;
static volatile bool want_theme_rebuild=false;
static volatile int  queued_ping=0;
static volatile int  queued_trace=0;
static volatile bool queued_identify=false, queued_flash=false;

static lv_obj_t* card(lv_obj_t* p){
  lv_obj_t* o=lv_obj_create(p);
  lv_obj_set_width(o,lv_pct(100)); lv_obj_set_height(o,LV_SIZE_CONTENT);
  lv_obj_set_style_bg_color(o,col_surf(),0); lv_obj_set_style_border_color(o,col_border(),0);
  lv_obj_set_style_border_width(o,1,0); lv_obj_set_style_radius(o,12,0);
  lv_obj_set_style_pad_all(o,10,0); lv_obj_clear_flag(o,LV_OBJ_FLAG_SCROLLABLE);
  return o;
}
static lv_obj_t* label(lv_obj_t* p,const char* t,lv_color_t c,const lv_font_t* f){
  lv_obj_t* l=lv_label_create(p); lv_label_set_text(l,t); lv_obj_set_style_text_color(l,c,0);
  if(f) lv_obj_set_style_text_font(l,f,0); return l;
}
static lv_obj_t* badge(lv_obj_t* p,const char* t,lv_color_t bg){
  lv_obj_t* l=lv_label_create(p); lv_label_set_text(l,t);
  lv_obj_set_style_bg_color(l,bg,0); lv_obj_set_style_bg_opa(l,LV_OPA_COVER,0);
  lv_obj_set_style_text_color(l,C(0xffffff),0); lv_obj_set_style_radius(l,8,0);
  lv_obj_set_style_pad_hor(l,8,0); lv_obj_set_style_pad_ver(l,3,0); return l;
}
static float calibratedFeet(float raw_ft){ return raw_ft * g_tdr_calibration; }
static void formatDistance(char* buf,size_t n,float raw_ft,bool approximate){
  float ft=calibratedFeet(raw_ft);
  if(g_use_meters) snprintf(buf,n,approximate?"~%.1f m":"%.1f m",ft*0.3048f);
  else snprintf(buf,n,approximate?"~%.0f ft":"%.0f ft",ft);
}
static void updateCalibrationLabel(){
  if(!lbl_cal_value) return; char b[32];
  snprintf(b,sizeof(b),"Multiplier: %.3f",g_tdr_calibration); lv_label_set_text(lbl_cal_value,b);
}
static void kvRow(lv_obj_t* parent,const char* key,lv_obj_t** valOut){
  lv_obj_t* row=card(parent); lv_obj_set_flex_flow(row,LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
  lv_obj_t* k=label(row,key,col_muted(),NULL); lv_obj_set_width(k,80);
  lv_obj_t* v=label(row,"--",col_text(),NULL); lv_obj_set_flex_grow(v,1); if(valOut)*valOut=v;
}
static void resultRow(lv_obj_t* parent,const char* key,lv_obj_t** valOut){
  lv_obj_t* row=card(parent); lv_obj_set_flex_flow(row,LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
  lv_obj_t* k=label(row,key,col_text(),NULL); lv_obj_set_flex_grow(k,1);
  lv_obj_t* v=label(row,"--",col_muted(),F_HEAD); if(valOut)*valOut=v;
}

static void ev_goHome(lv_event_t* e){(void)e;cur=0;lv_screen_load(scr_home);}
static void ev_goCable(lv_event_t* e){(void)e;cur=1;lv_screen_load(scr_cable);}
static void ev_goNet(lv_event_t* e){(void)e;cur=2;lv_screen_load(scr_net);}
static void ev_goAuto(lv_event_t* e){(void)e;cur=3;lv_screen_load(scr_auto);}
static void ev_goSwitch(lv_event_t* e){(void)e;cur=4;lv_screen_load(scr_switch);}
static void ev_goTools(lv_event_t* e){(void)e;cur=5;lv_screen_load(scr_tools);}
static void ev_theme(lv_event_t* e){g_dark=lv_obj_has_state((lv_obj_t*)lv_event_get_target(e),LV_STATE_CHECKED);saveSettings();want_theme_rebuild=true;}
static void ev_runCable(lv_event_t* e){(void)e;run_cable=true;}
static void ev_runNet(lv_event_t* e){(void)e;run_net=true;}
static void ev_runAuto(lv_event_t* e){(void)e;run_auto2=true;}
static void ev_pingGw(lv_event_t* e){(void)e;queued_ping=1;}
static void ev_pingGoog(lv_event_t* e){(void)e;queued_ping=2;}
static void ev_traceGw(lv_event_t* e){(void)e;queued_trace=1;}
static void ev_traceGoog(lv_event_t* e){(void)e;queued_trace=2;}
static void ev_identify(lv_event_t* e){(void)e;queued_identify=true;}
static void ev_flash(lv_event_t* e){(void)e;queued_flash=true;}
static void ev_units_changed(lv_event_t* e){g_use_meters=lv_dropdown_get_selected((lv_obj_t*)lv_event_get_target(e))==1;saveSettings();}
static void ev_tools_theme_changed(lv_event_t* e){
  bool dark=lv_dropdown_get_selected((lv_obj_t*)lv_event_get_target(e))==1;
  if(dark!=g_dark){g_dark=dark;saveSettings();want_theme_rebuild=true;}
}
static void ev_logs_changed(lv_event_t* e){g_logs_enabled=lv_obj_has_state((lv_obj_t*)lv_event_get_target(e),LV_STATE_CHECKED);saveSettings();}
static void ev_tdr_minus(lv_event_t* e){(void)e;int32_t v=lv_spinbox_get_value(spin_tdr_cal)-5;if(v<500)v=500;lv_spinbox_set_value(spin_tdr_cal,v);g_tdr_calibration=v/1000.0f;updateCalibrationLabel();saveSettings();}
static void ev_tdr_plus(lv_event_t* e){(void)e;int32_t v=lv_spinbox_get_value(spin_tdr_cal)+5;if(v>1500)v=1500;lv_spinbox_set_value(spin_tdr_cal,v);g_tdr_calibration=v/1000.0f;updateCalibrationLabel();saveSettings();}
static void ev_tdr_reset(lv_event_t* e){(void)e;g_tdr_calibration=1.0f;if(spin_tdr_cal)lv_spinbox_set_value(spin_tdr_cal,1000);updateCalibrationLabel();saveSettings();}
static void ev_about_close(lv_event_t* e){(void)e;if(about_overlay){lv_obj_del(about_overlay);about_overlay=NULL;}}
static void ev_about_open(lv_event_t* e){
  (void)e;if(about_overlay)return;
  about_overlay=lv_obj_create(lv_layer_top());lv_obj_set_size(about_overlay,lv_pct(100),lv_pct(100));
  lv_obj_set_style_bg_color(about_overlay,C(0x000000),0);lv_obj_set_style_bg_opa(about_overlay,LV_OPA_60,0);
  lv_obj_set_style_border_width(about_overlay,0,0);lv_obj_set_style_pad_all(about_overlay,12,0);
  lv_obj_t* w=lv_obj_create(about_overlay);lv_obj_set_width(w,lv_pct(92));lv_obj_set_height(w,LV_SIZE_CONTENT);lv_obj_center(w);
  lv_obj_set_style_bg_color(w,col_surf(),0);lv_obj_set_style_border_color(w,col_border(),0);lv_obj_set_style_border_width(w,1,0);
  lv_obj_set_style_radius(w,14,0);lv_obj_set_style_pad_all(w,14,0);lv_obj_set_style_pad_row(w,10,0);
  lv_obj_set_flex_flow(w,LV_FLEX_FLOW_COLUMN);lv_obj_set_flex_align(w,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
  label(w,"ABOUT",col_text(),F_HEAD);
  char details[360];snprintf(details,sizeof(details),"Device model:\n%s\n\nFirmware version:\n%s\n\nCreator:\n%s\n\nLicense:\n%s",NG_DEVICE_MODEL,NG_FIRMWARE_VERSION,NG_CREATOR_NAME,NG_LICENSE_NAME);
  lv_obj_t* d=label(w,details,col_muted(),NULL);lv_label_set_long_mode(d,LV_LABEL_LONG_WRAP);lv_obj_set_width(d,lv_pct(100));lv_obj_set_style_text_align(d,LV_TEXT_ALIGN_CENTER,0);
  lv_obj_t* b=lv_button_create(w);lv_obj_set_width(b,lv_pct(100));lv_obj_set_height(b,42);lv_obj_set_style_bg_color(b,col_primary(),0);
  lv_obj_t* bl=label(b,"CLOSE",C(0xffffff),F_HEAD);lv_obj_center(bl);lv_obj_add_event_cb(b,ev_about_close,LV_EVENT_CLICKED,NULL);
}

static void goblinStatus(NgGoblinEvent ev){ if(lbl_goblin) lv_label_set_text(lbl_goblin,ngGoblinLine(ev,g_extreme)); }
static void ev_logo_tap(lv_event_t* e){
  (void)e; uint32_t now=millis(); if(now-last_logo_tap>1200) logo_taps=0; last_logo_tap=now;
  if(++logo_taps>=3){ logo_taps=0; g_extreme=!g_extreme; g_dark=true;saveSettings(); want_theme_rebuild=true; }
}
static void refreshDev(){
  if(!lbl_dev)return; NgNet n; ngEthSnapshot(n); uint32_t phy=0; bool phyOk=ngPhyId(&phy);
  char b[420]; snprintf(b,sizeof(b),"Firmware: %s\nBuild: %s %s\nUptime: %lus\nFree heap: %lu bytes\nMAC: %s\nLink: %s / %dM\nPHY ID: %s0x%08lX\n\nRaw PHY registers",NG_FW,__DATE__,__TIME__,millis()/1000,(unsigned long)ESP.getFreeHeap(),n.mac.c_str(),n.link_up?"UP":"DOWN",n.speed_mbps,phyOk?"":"read failed ",(unsigned long)phy);
  lv_label_set_text(lbl_dev,b);
}
static void ev_dev_back(lv_event_t* e){(void)e;cur=0;lv_screen_load(scr_home);}
static void ev_dev_refresh(lv_event_t* e){(void)e;refreshDev();}
static void ev_open_dev(lv_event_t* e){(void)e;cur=6;refreshDev();lv_screen_load(scr_dev);}
static lv_obj_t* header(lv_obj_t* scr,const char* title,bool back){
  lv_obj_t* h=lv_obj_create(scr);lv_obj_set_width(h,lv_pct(100));lv_obj_set_height(h,58);
  lv_obj_set_style_bg_color(h,col_surf(),0);lv_obj_set_style_border_color(h,col_border(),0);lv_obj_set_style_border_width(h,1,0);lv_obj_set_style_radius(h,0,0);lv_obj_set_style_pad_hor(h,10,0);
  lv_obj_set_flex_flow(h,LV_FLEX_FLOW_ROW);lv_obj_set_flex_align(h,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);lv_obj_clear_flag(h,LV_OBJ_FLAG_SCROLLABLE);
  if(back){lv_obj_t* b=lv_button_create(h);lv_obj_set_size(b,40,40);lv_obj_set_style_bg_color(b,col_surf(),0);lv_obj_set_style_border_color(b,col_border(),0);lv_obj_set_style_border_width(b,1,0);lv_obj_t* bl=label(b,LV_SYMBOL_LEFT,col_primary(),NULL);lv_obj_center(bl);lv_obj_add_event_cb(b,ev_goHome,LV_EVENT_CLICKED,NULL);}
  lv_obj_t* t=label(h,title,col_text(),F_HEAD);lv_obj_set_flex_grow(t,1);lv_obj_t* sw=lv_switch_create(h);if(g_dark)lv_obj_add_state(sw,LV_STATE_CHECKED);lv_obj_add_event_cb(sw,ev_theme,LV_EVENT_VALUE_CHANGED,NULL);return h;
}
static lv_obj_t* body(lv_obj_t* scr){lv_obj_t* b=lv_obj_create(scr);lv_obj_set_width(b,lv_pct(100));lv_obj_set_flex_grow(b,1);lv_obj_set_style_bg_opa(b,0,0);lv_obj_set_style_border_width(b,0,0);lv_obj_set_style_pad_all(b,12,0);lv_obj_set_style_pad_row(b,10,0);lv_obj_set_flex_flow(b,LV_FLEX_FLOW_COLUMN);return b;}
static void runButton(lv_obj_t* scr,const char* txt,lv_event_cb_t cb){lv_obj_t* f=lv_obj_create(scr);lv_obj_set_width(f,lv_pct(100));lv_obj_set_height(f,LV_SIZE_CONTENT);lv_obj_set_style_bg_color(f,col_surf(),0);lv_obj_set_style_border_color(f,col_border(),0);lv_obj_set_style_border_width(f,1,0);lv_obj_set_style_radius(f,0,0);lv_obj_set_style_pad_all(f,8,0);lv_obj_clear_flag(f,LV_OBJ_FLAG_SCROLLABLE);lv_obj_t* btn=lv_button_create(f);lv_obj_set_width(btn,lv_pct(100));lv_obj_set_height(btn,44);lv_obj_set_style_bg_color(btn,col_primary(),0);lv_obj_t* l=label(btn,txt,C(0xffffff),F_HEAD);lv_obj_center(l);lv_obj_add_event_cb(btn,cb,LV_EVENT_CLICKED,NULL);}
static void smallBtn(lv_obj_t* row,const char* txt,lv_event_cb_t cb){lv_obj_t* btn=lv_button_create(row);lv_obj_set_flex_grow(btn,1);lv_obj_set_height(btn,40);lv_obj_set_style_bg_color(btn,col_primary(),0);lv_obj_t* l=label(btn,txt,C(0xffffff),NULL);lv_obj_center(l);lv_obj_add_event_cb(btn,cb,LV_EVENT_CLICKED,NULL);}
static void navBtn(lv_obj_t* parent,const char* title,const char* sub,lv_event_cb_t cb){lv_obj_t* b=lv_button_create(parent);lv_obj_set_width(b,lv_pct(100));lv_obj_set_height(b,72);lv_obj_set_style_bg_color(b,col_surf(),0);lv_obj_set_style_border_color(b,col_border(),0);lv_obj_set_style_border_width(b,1,0);lv_obj_set_style_radius(b,12,0);lv_obj_set_flex_flow(b,LV_FLEX_FLOW_COLUMN);lv_obj_set_flex_align(b,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_START);lv_obj_set_style_pad_left(b,14,0);label(b,title,col_text(),F_HEAD);label(b,sub,col_muted(),NULL);lv_obj_add_event_cb(b,cb,LV_EVENT_CLICKED,NULL);}

static void initScreen(lv_obj_t*& scr,const char* title){scr=lv_obj_create(NULL);lv_obj_set_style_bg_color(scr,col_bg(),0);lv_obj_set_flex_flow(scr,LV_FLEX_FLOW_COLUMN);lv_obj_set_style_pad_all(scr,0,0);lv_obj_set_style_pad_row(scr,0,0);if(title)header(scr,title,true);}
static void buildHome(){
  initScreen(scr_home,NULL); lv_obj_t* h=lv_obj_create(scr_home); lv_obj_set_width(h,lv_pct(100)); lv_obj_set_height(h,58);
  lv_obj_set_style_bg_color(h,col_surf(),0); lv_obj_set_style_border_color(h,col_border(),0); lv_obj_set_style_border_width(h,1,0); lv_obj_set_style_pad_hor(h,10,0);
  lv_obj_set_flex_flow(h,LV_FLEX_FLOW_ROW); lv_obj_set_flex_align(h,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER); lv_obj_clear_flag(h,LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t* stack=lv_obj_create(h); lv_obj_set_style_bg_opa(stack,0,0); lv_obj_set_style_border_width(stack,0,0); lv_obj_set_style_pad_all(stack,0,0); lv_obj_set_height(stack,LV_SIZE_CONTENT); lv_obj_set_flex_grow(stack,1); lv_obj_set_flex_flow(stack,LV_FLEX_FLOW_COLUMN);
  lv_obj_t* logo=label(stack,"NETWORK GOBLIN",col_text(),F_HEAD); lv_obj_add_flag(logo,LV_OBJ_FLAG_CLICKABLE); lv_obj_add_event_cb(logo,ev_logo_tap,LV_EVENT_CLICKED,NULL); lv_obj_add_event_cb(logo,ev_open_dev,LV_EVENT_LONG_PRESSED,NULL);
  label(stack,g_extreme?"ND-1 / MAXIMUM":"ND-1",col_muted(),NULL); lv_obj_t* sw=lv_switch_create(h); if(g_dark)lv_obj_add_state(sw,LV_STATE_CHECKED); lv_obj_add_event_cb(sw,ev_theme,LV_EVENT_VALUE_CHANGED,NULL);
  lv_obj_t* b=body(scr_home); navBtn(b,"AUTOTEST","Cable + network, one tap",ev_goAuto); navBtn(b,"CABLE TEST","Length / Fault / Status",ev_goCable); navBtn(b,"NETWORK TEST","DHCP / Ping / Trace",ev_goNet); navBtn(b,"SWITCH INFO","Identify port / VLAN / flash",ev_goSwitch); navBtn(b,"TOOLS","Settings / Logs / About",ev_goTools);
  lbl_goblin=label(scr_home,ngGoblinLine(NGG_IDLE,g_extreme),col_primary(),NULL); lv_obj_set_width(lbl_goblin,lv_pct(100)); lv_obj_set_style_text_align(lbl_goblin,LV_TEXT_ALIGN_CENTER,0); lv_obj_set_style_pad_ver(lbl_goblin,6,0);
}
static void buildCable(){initScreen(scr_cable,"CABLE TEST");lv_obj_t* bd=body(scr_cable);lv_obj_t* len=card(bd);lv_obj_set_flex_flow(len,LV_FLEX_FLOW_COLUMN);lv_obj_set_flex_align(len,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);label(len,"CABLE LENGTH",col_muted(),NULL);lbl_length=label(len,g_use_meters?"-- m":"-- ft",col_text(),F_TITLE);lv_obj_t* fc=card(bd);lv_obj_set_flex_flow(fc,LV_FLEX_FLOW_ROW);lv_obj_set_flex_align(fc,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);lv_obj_t* fcol=lv_obj_create(fc);lv_obj_set_style_bg_opa(fcol,0,0);lv_obj_set_style_border_width(fcol,0,0);lv_obj_set_style_pad_all(fcol,0,0);lv_obj_set_height(fcol,LV_SIZE_CONTENT);lv_obj_set_flex_grow(fcol,1);lv_obj_set_flex_flow(fcol,LV_FLEX_FLOW_COLUMN);label(fcol,"Fault",col_text(),NULL);lbl_fault=label(fcol,"None detected",col_muted(),NULL);badge_fault=badge(fc," OK ",col_pass());label(bd,"PAIR STATUS",col_muted(),NULL);kvRow(bd,"Pair 1-2",&lbl_p12);kvRow(bd,"Pair 3-6",&lbl_p36);kvRow(bd,"Pair 4-5",&lbl_p45);kvRow(bd,"Pair 7-8",&lbl_p78);runButton(scr_cable,"RUN CABLE TEST",ev_runCable);}
static void buildNet(){initScreen(scr_net,"NETWORK TEST");lv_obj_t* strip=lv_obj_create(scr_net);lv_obj_set_width(strip,lv_pct(100));lv_obj_set_height(strip,34);lv_obj_set_style_bg_color(strip,col_idle(),0);lv_obj_set_style_radius(strip,0,0);lv_obj_clear_flag(strip,LV_OBJ_FLAG_SCROLLABLE);lbl_link=label(strip,"LINK: -- / --",C(0xffffff),F_HEAD);lv_obj_center(lbl_link);lv_obj_t* bd=body(scr_net);lv_obj_t* rd=card(bd);lv_obj_set_flex_flow(rd,LV_FLEX_FLOW_ROW);lv_obj_set_flex_align(rd,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);lv_obj_t* dl=label(rd,"DHCP",col_text(),NULL);lv_obj_set_flex_grow(dl,1);badge_dhcp=badge(rd," -- ",col_idle());kvRow(bd,"IP",&lbl_ip);kvRow(bd,"Subnet",&lbl_mask);kvRow(bd,"Gateway",&lbl_gw);kvRow(bd,"DNS",&lbl_dns);label(bd,"PING / TRACE",col_muted(),NULL);for(int i=0;i<2;i++){lv_obj_t* r=lv_obj_create(bd);lv_obj_set_width(r,lv_pct(100));lv_obj_set_height(r,LV_SIZE_CONTENT);lv_obj_set_style_bg_opa(r,0,0);lv_obj_set_style_border_width(r,0,0);lv_obj_set_style_pad_all(r,0,0);lv_obj_set_style_pad_column(r,8,0);lv_obj_set_flex_flow(r,LV_FLEX_FLOW_ROW);lv_obj_clear_flag(r,LV_OBJ_FLAG_SCROLLABLE);if(i==0){smallBtn(r,"Ping GW",ev_pingGw);smallBtn(r,"Ping Google",ev_pingGoog);}else{smallBtn(r,"Trace GW",ev_traceGw);smallBtn(r,"Trace Google",ev_traceGoog);}}lv_obj_t* res=card(bd);lbl_netresult=label(res,"Results appear here.",col_muted(),NULL);lv_label_set_long_mode(lbl_netresult,LV_LABEL_LONG_WRAP);lv_obj_set_width(lbl_netresult,lv_pct(100));runButton(scr_net,"RUN NETWORK TEST",ev_runNet);}
static void buildAuto(){initScreen(scr_auto,"AUTOTEST");lv_obj_t* bd=body(scr_auto);resultRow(bd,"Cable",&lbl_a_cable);resultRow(bd,"Length",&lbl_a_len);resultRow(bd,"Gateway",&lbl_a_gw);resultRow(bd,"Internet",&lbl_a_inet);runButton(scr_auto,"RUN AUTOTEST",ev_runAuto);}
static void buildSwitch(){initScreen(scr_switch,"SWITCH INFO");lv_obj_t* bd=body(scr_switch);resultRow(bd,"Switch",&lbl_sw_name);resultRow(bd,"Port",&lbl_sw_port);resultRow(bd,"VLAN",&lbl_sw_vlan);lv_obj_t* st=card(bd);lbl_sw_status=label(st,"Tap Identify (~45s).\nNetworking pauses during capture.",col_muted(),NULL);lv_label_set_long_mode(lbl_sw_status,LV_LABEL_LONG_WRAP);lv_obj_set_width(lbl_sw_status,lv_pct(100));runButton(scr_switch,"IDENTIFY SWITCH",ev_identify);runButton(scr_switch,"FLASH PORT LED",ev_flash);}
static void buildTools(){
  initScreen(scr_tools,"TOOLS");lv_obj_t* bd=body(scr_tools);lv_obj_add_flag(bd,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_scroll_dir(bd,LV_DIR_VER);
  label(bd,"DISPLAY",col_muted(),NULL);lv_obj_t* dc=card(bd);lv_obj_set_flex_flow(dc,LV_FLEX_FLOW_COLUMN);lv_obj_set_style_pad_row(dc,10,0);
  label(dc,"Distance units",col_text(),NULL);dd_units=lv_dropdown_create(dc);lv_dropdown_set_options(dd_units,"Feet\nMeters");lv_dropdown_set_selected(dd_units,g_use_meters?1:0);lv_obj_set_width(dd_units,lv_pct(100));lv_obj_add_event_cb(dd_units,ev_units_changed,LV_EVENT_VALUE_CHANGED,NULL);
  label(dc,"View mode",col_text(),NULL);dd_theme=lv_dropdown_create(dc);lv_dropdown_set_options(dd_theme,"Light\nGoblin");lv_dropdown_set_selected(dd_theme,g_dark?1:0);lv_obj_set_width(dd_theme,lv_pct(100));lv_obj_add_event_cb(dd_theme,ev_tools_theme_changed,LV_EVENT_VALUE_CHANGED,NULL);
  label(bd,"TDR CALIBRATION",col_muted(),NULL);lv_obj_t* cc=card(bd);lv_obj_set_flex_flow(cc,LV_FLEX_FLOW_COLUMN);lv_obj_set_style_pad_row(cc,8,0);lv_obj_t* help=label(cc,"Adjust all displayed TDR distances.\n1.000 = no correction",col_muted(),NULL);lv_label_set_long_mode(help,LV_LABEL_LONG_WRAP);lv_obj_set_width(help,lv_pct(100));lbl_cal_value=label(cc,"Multiplier: 1.000",col_text(),F_HEAD);updateCalibrationLabel();spin_tdr_cal=lv_spinbox_create(cc);lv_spinbox_set_range(spin_tdr_cal,500,1500);lv_spinbox_set_digit_format(spin_tdr_cal,4,1);lv_spinbox_set_value(spin_tdr_cal,(int32_t)(g_tdr_calibration*1000.0f));lv_obj_set_width(spin_tdr_cal,lv_pct(100));
  lv_obj_t* cr=lv_obj_create(cc);lv_obj_set_width(cr,lv_pct(100));lv_obj_set_height(cr,LV_SIZE_CONTENT);lv_obj_set_style_bg_opa(cr,0,0);lv_obj_set_style_border_width(cr,0,0);lv_obj_set_style_pad_all(cr,0,0);lv_obj_set_style_pad_column(cr,6,0);lv_obj_set_flex_flow(cr,LV_FLEX_FLOW_ROW);lv_obj_clear_flag(cr,LV_OBJ_FLAG_SCROLLABLE);smallBtn(cr,"- 0.005",ev_tdr_minus);smallBtn(cr,"+ 0.005",ev_tdr_plus);smallBtn(cr,"RESET",ev_tdr_reset);
  lv_obj_t* sep=lv_obj_create(bd);lv_obj_set_width(sep,lv_pct(100));lv_obj_set_height(sep,1);lv_obj_set_style_bg_color(sep,col_border(),0);lv_obj_set_style_bg_opa(sep,LV_OPA_COVER,0);lv_obj_set_style_border_width(sep,0,0);lv_obj_set_style_pad_all(sep,0,0);
  label(bd,"LOGS",col_muted(),NULL);lv_obj_t* lc=card(bd);lv_obj_set_flex_flow(lc,LV_FLEX_FLOW_ROW);lv_obj_set_flex_align(lc,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);lv_obj_t* lt=label(lc,"Enable diagnostic logs",col_text(),NULL);lv_obj_set_flex_grow(lt,1);sw_logs=lv_switch_create(lc);if(g_logs_enabled)lv_obj_add_state(sw_logs,LV_STATE_CHECKED);lv_obj_add_event_cb(sw_logs,ev_logs_changed,LV_EVENT_VALUE_CHANGED,NULL);
  label(bd,"DEVICE",col_muted(),NULL);lv_obj_t* ab=lv_button_create(bd);lv_obj_set_width(ab,lv_pct(100));lv_obj_set_height(ab,48);lv_obj_set_style_bg_color(ab,col_primary(),0);lv_obj_t* al=label(ab,"ABOUT NETWORK GOBLIN",C(0xffffff),F_HEAD);lv_obj_center(al);lv_obj_add_event_cb(ab,ev_about_open,LV_EVENT_CLICKED,NULL);
}

static void buildSplash(){
  scr_splash=lv_obj_create(NULL); lv_obj_set_style_bg_color(scr_splash,C(0x070b09),0); lv_obj_set_flex_flow(scr_splash,LV_FLEX_FLOW_COLUMN); lv_obj_set_flex_align(scr_splash,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
  label(scr_splash,"NETWORK GOBLIN",C(0x39ff77),F_TITLE); label(scr_splash,NG_MODEL,C(0xd8ffe4),F_HEAD); label(scr_splash,NG_TAGLINE,C(0x7ebc91),NULL);
  lbl_splash_status=label(scr_splash,"Waking goblin...",C(0x39ff77),NULL); lv_obj_set_style_pad_top(lbl_splash_status,28,0);
}
static void buildDev(){
  initScreen(scr_dev,"DEVELOPER DIAGNOSTICS"); lv_obj_t* bd=body(scr_dev); lbl_dev=label(bd,"Loading...",col_text(),NULL); lv_label_set_long_mode(lbl_dev,LV_LABEL_LONG_WRAP); lv_obj_set_width(lbl_dev,lv_pct(100));
  lv_obj_t* r=lv_obj_create(bd); lv_obj_set_width(r,lv_pct(100)); lv_obj_set_height(r,LV_SIZE_CONTENT); lv_obj_set_style_bg_opa(r,0,0); lv_obj_set_style_border_width(r,0,0); lv_obj_set_style_pad_all(r,0,0); lv_obj_set_style_pad_column(r,8,0); lv_obj_set_flex_flow(r,LV_FLEX_FLOW_ROW); smallBtn(r,"REFRESH",ev_dev_refresh); smallBtn(r,"HOME",ev_dev_back);
}
static void buildAll(){
  if(about_overlay){lv_obj_del(about_overlay);about_overlay=NULL;}
  lv_obj_t** all[]={&scr_home,&scr_cable,&scr_net,&scr_auto,&scr_switch,&scr_tools,&scr_splash,&scr_dev};
  for(auto pp:all){if(*pp){lv_obj_del(*pp);*pp=NULL;}}
  lbl_goblin=NULL; lbl_splash_status=NULL; lbl_dev=NULL;
  buildHome();buildCable();buildNet();buildAuto();buildSwitch();buildTools();buildSplash();buildDev();
}
void ngUiInit(){loadSettings();buildAll();}void ngUiLoadHome(){cur=0;lv_screen_load(scr_home);}

void ngUiPublishCable(const NgCable& c){char b[32];if(c.length_valid)formatDistance(b,sizeof(b),c.length_ft,true);else if(c.link_up)snprintf(b,sizeof(b),"linked");else snprintf(b,sizeof(b),g_use_meters?"-- m":"-- ft");if(lbl_length)lv_label_set_text(lbl_length,b);}
void ngUiPublishTdr(const NgTdrResult& r){NgTdrType t=r.mdi.type;if(t==NG_TDR_OPEN||t==NG_TDR_SHORT){char d[24],f[48];formatDistance(d,sizeof(d),r.mdi.dist_ft,false);snprintf(f,sizeof(f),"%s @ %s",ngTdrTypeStr(t),d);if(lbl_fault)lv_label_set_text(lbl_fault,f);if(badge_fault){lv_label_set_text(badge_fault," FAULT ");lv_obj_set_style_bg_color(badge_fault,col_fail(),0);}if(lbl_length)lv_label_set_text(lbl_length,d);}else{if(lbl_fault)lv_label_set_text(lbl_fault,"None detected");if(badge_fault){lv_label_set_text(badge_fault," OK ");lv_obj_set_style_bg_color(badge_fault,col_pass(),0);}}}
static void colorPair(lv_obj_t* l,NgPairState s,float ft){if(!l)return;char b[48];if(s==NGP_OPEN||s==NGP_SHORT){char d[24];formatDistance(d,sizeof(d),ft,false);snprintf(b,sizeof(b),"%s @ %s",ngPairStateStr(s),d);}else snprintf(b,sizeof(b),"%s",ngPairStateStr(s));lv_label_set_text(l,b);lv_color_t c=(s==NGP_OK)?col_pass():(s==NGP_NA)?col_idle():col_fail();lv_obj_set_style_text_color(l,c,0);}
void ngUiPublishPairs(const NgWiremap& w){colorPair(lbl_p12,w.p12.state,w.p12.dist_ft);colorPair(lbl_p36,w.p36.state,w.p36.dist_ft);colorPair(lbl_p45,w.p45.state,0);colorPair(lbl_p78,w.p78.state,0);}
void ngUiPublishNet(const NgNet& n){if(lbl_ip)lv_label_set_text(lbl_ip,n.got_ip?n.ip.c_str():"--");if(lbl_mask)lv_label_set_text(lbl_mask,n.got_ip?n.mask.c_str():"--");if(lbl_gw)lv_label_set_text(lbl_gw,n.got_ip?n.gw.c_str():"--");if(lbl_dns)lv_label_set_text(lbl_dns,n.got_ip?n.dns.c_str():"--");if(badge_dhcp){lv_label_set_text(badge_dhcp,n.got_ip?" LEASED ":" NONE ");lv_obj_set_style_bg_color(badge_dhcp,n.got_ip?col_pass():col_fail(),0);}if(lbl_link){char b[32];snprintf(b,sizeof(b),"LINK: %s / %dM",n.link_up?"UP":"DOWN",n.speed_mbps);lv_label_set_text(lbl_link,b);}}
static void setResult(const char* s){if(lbl_netresult)lv_label_set_text(lbl_netresult,s);}static void setPF(lv_obj_t* l,bool pass,const char* txt){if(!l)return;lv_label_set_text(l,txt);lv_obj_set_style_text_color(l,pass?col_pass():col_fail(),0);}
void ngUiOnCable(){run_cable=true;}void ngUiOnNetwork(){run_net=true;}void ngUiOnAutotest(){run_auto2=true;}
static String g_trace;static void traceHop(int hop,const char* ip,uint32_t rtt,bool reached){char line[48];snprintf(line,sizeof(line)," %2d %-15s %lums%s\n",hop,ip,rtt,reached?" *":"");g_trace+=line;}

void ngUiService(){
  if(want_theme_rebuild){want_theme_rebuild=false;int keep=cur;buildAll();lv_screen_load(keep==0?scr_home:keep==1?scr_cable:keep==2?scr_net:keep==3?scr_auto:keep==4?scr_switch:keep==5?scr_tools:scr_dev);}
  if(run_cable){run_cable=false;NgCable c;ngCableRun(c);ngUiPublishCable(c);NgTdrResult r;ngTdrRun(r);ngUiPublishTdr(r);NgWiremap w;ngPairsRun(w);ngUiPublishPairs(w);goblinStatus((r.mdi.type==NG_TDR_OPEN||r.mdi.type==NG_TDR_SHORT)?NGG_CABLE_FAULT:NGG_CABLE_OK);}
  if(run_net){run_net=false;NgNet n;ngEthSnapshot(n);ngUiPublishNet(n);goblinStatus(n.got_ip?NGG_NET_OK:NGG_NET_FAIL);}
  if(run_auto2){run_auto2=false;NgCable c;ngCableRun(c);NgTdrResult r;ngTdrRun(r);bool cableOk=(r.mdi.type!=NG_TDR_OPEN&&r.mdi.type!=NG_TDR_SHORT);setPF(lbl_a_cable,cableOk,cableOk?"PASS":"FAULT");char lb[40];if(r.mdi.type==NG_TDR_OPEN||r.mdi.type==NG_TDR_SHORT){char d[24];formatDistance(d,sizeof(d),r.mdi.dist_ft,false);snprintf(lb,sizeof(lb),"%s (fault)",d);}else if(c.length_valid)formatDistance(lb,sizeof(lb),c.length_ft,true);else snprintf(lb,sizeof(lb),"linked");if(lbl_a_len){lv_label_set_text(lbl_a_len,lb);lv_obj_set_style_text_color(lbl_a_len,col_muted(),0);}if(cableOk){String gw=ngEthGateway();NgPingResult pg,pi;bool gwOk=gw.length()&&ngPing(gw.c_str(),3,pg);bool inOk=ngPing("8.8.8.8",3,pi);setPF(lbl_a_gw,gwOk,gwOk?"PASS":"FAIL");setPF(lbl_a_inet,inOk,inOk?"PASS":"FAIL");}else{setPF(lbl_a_gw,false,"skipped");setPF(lbl_a_inet,false,"skipped");}}
  if(queued_ping){int which=queued_ping;queued_ping=0;String tgt=(which==1)?ngEthGateway():String("8.8.8.8");if(tgt.length()==0)setResult("No gateway (no DHCP lease).");else{setResult(which==1?"Pinging gateway...":"Pinging Google...");NgPingResult pr;if(ngPing(tgt.c_str(),4,pr)){char b[96];snprintf(b,sizeof(b),"Ping %s\n%d/%d replies, avg %lums\n(min %lu / max %lu)",pr.resolved_ip.c_str(),pr.recv,pr.sent,pr.avg_ms,pr.min_ms,pr.max_ms);setResult(b);goblinStatus(NGG_NET_OK);}else{char b[64];snprintf(b,sizeof(b),"Ping %s: NO REPLY",tgt.c_str());setResult(b);goblinStatus(NGG_NET_FAIL);}}}
  if(queued_trace){int which=queued_trace;queued_trace=0;String tgt=(which==1)?ngEthGateway():String("8.8.8.8");if(tgt.length()==0)setResult("No gateway (no DHCP lease).");else{setResult(which==1?"Tracing gateway...":"Tracing Google...");g_trace=String("Trace ")+tgt+"\n";ngTraceroute(tgt.c_str(),which==1?4:15,traceHop);setResult(g_trace.c_str());}}
  if(queued_identify){queued_identify=false;if(lbl_sw_status)lv_label_set_text(lbl_sw_status,"Listening for LLDP/CDP...\n(networking paused ~45s)");lv_refr_now(NULL);ngLldpStart();uint32_t t0=millis();while(millis()-t0<45000&&!ngLldpFresh(60000)){delay(200);}NgSwitchInfo si;ngLldpGet(si);ngLldpStop();if(si.have_lldp||si.have_cdp){if(lbl_sw_name)lv_label_set_text(lbl_sw_name,si.sys_name[0]?si.sys_name:"?");if(lbl_sw_port)lv_label_set_text(lbl_sw_port,si.port_id[0]?si.port_id:"?");char vb[16];if(si.vlan>=0)snprintf(vb,sizeof(vb),"%d",si.vlan);else snprintf(vb,sizeof(vb),"(untagged)");if(lbl_sw_vlan)lv_label_set_text(lbl_sw_vlan,vb);if(lbl_sw_status)lv_label_set_text(lbl_sw_status,"Found (via LLDP/CDP).\nNetworking restored.");goblinStatus(NGG_SWITCH_OK);}else if(lbl_sw_status)lv_label_set_text(lbl_sw_status,"Nothing seen. Switch may not\nsend LLDP/CDP, or try again.");goblinStatus(NGG_SWITCH_FAIL);}
  if(queued_flash){queued_flash=false;ngLldpPortFlash(5);if(lbl_sw_status)lv_label_set_text(lbl_sw_status,"Flashing port LED (5 cycles).\nWatch the switch. (UniFi may not blink)");}
}

void ngUiShowSplash(){cur=7;lv_screen_load(scr_splash);lv_refr_now(NULL);}
void ngUiSplashStatus(const char* text){if(lbl_splash_status){lv_label_set_text(lbl_splash_status,text);lv_refr_now(NULL);}}
void ngUiSplashAwake(){ngUiSplashStatus(ngGoblinBootLine());}
bool ngUiExtremeGoblin(){return g_extreme;}
bool ngUiLogsEnabled(){return g_logs_enabled;}
float ngUiTdrCalibration(){return g_tdr_calibration;}
bool ngUiUsesMeters(){return g_use_meters;}
