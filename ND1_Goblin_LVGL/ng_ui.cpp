/* ng_ui.cpp - HAND-CODED LVGL screens
 *   Home / Cable / Network / Autotest / Switch Info / Tools
 * ------------------------------------------------------------
 *  - dark-mode crash fix (deferred rebuild + screen deletion)
 *  - Switch Info screen: Identify (LLDP/CDP) + Flash Port LED
 *  - Tools kept as a placeholder for future use
 * ============================================================ */
#include "ng_config.h"
#include "ng_ui.h"
#include "ng_eth.h"
#include "ng_tdr.h"
#include "ng_pairs.h"
#include "ng_netcmd.h"
#include "ng_lldp.h"
#include <lvgl.h>
#include <Arduino.h>

static bool g_dark = false;
static lv_color_t C(uint32_t h){ return lv_color_hex(h); }
static lv_color_t col_bg()     { return C(g_dark?0x070b09:0xf2f5f4); }
static lv_color_t col_surf()   { return C(g_dark?0x101713:0xffffff); }
static lv_color_t col_text()   { return C(g_dark?0xd8ffe4:0x17211d); }
static lv_color_t col_muted()  { return C(g_dark?0x7ebc91:0x66736d); }
static lv_color_t col_primary(){ return C(g_dark?0x39ff77:0x176b4d); }
static lv_color_t col_border() { return C(g_dark?0x245d38:0xc9d4cf); }
static lv_color_t col_pass()   { return C(0x21b35b); }
static lv_color_t col_fail()   { return C(0xdd3f3f); }
static lv_color_t col_idle()   { return C(0x77847e); }

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

static lv_obj_t *scr_home=NULL,*scr_cable=NULL,*scr_net=NULL,*scr_auto=NULL,*scr_switch=NULL,*scr_tools=NULL;
static int cur=0;
static lv_obj_t *lbl_length,*lbl_fault,*badge_fault;
static lv_obj_t *lbl_p12,*lbl_p36,*lbl_p45,*lbl_p78;
static lv_obj_t *lbl_ip,*lbl_mask,*lbl_gw,*lbl_dns,*lbl_link,*badge_dhcp;
static lv_obj_t *lbl_netresult;
static lv_obj_t *lbl_a_cable,*lbl_a_len,*lbl_a_gw,*lbl_a_inet;
static lv_obj_t *lbl_sw_name,*lbl_sw_port,*lbl_sw_vlan,*lbl_sw_status;

static void buildAll();

// queued work
static volatile bool run_cable=false, run_net=false, run_auto2=false;
static volatile bool want_theme_rebuild=false;
static volatile int  queued_ping=0;   // 1=gateway 2=google
static volatile int  queued_trace=0;  // 1=gateway 2=google
static volatile bool queued_identify=false, queued_flash=false;

// ---------- small builders ----------
static lv_obj_t* card(lv_obj_t* p){
  lv_obj_t* o=lv_obj_create(p);
  lv_obj_set_width(o,lv_pct(100)); lv_obj_set_height(o,LV_SIZE_CONTENT);
  lv_obj_set_style_bg_color(o,col_surf(),0);
  lv_obj_set_style_border_color(o,col_border(),0);
  lv_obj_set_style_border_width(o,1,0);
  lv_obj_set_style_radius(o,12,0);
  lv_obj_set_style_pad_all(o,10,0);
  lv_obj_clear_flag(o,LV_OBJ_FLAG_SCROLLABLE);
  return o;
}
static lv_obj_t* label(lv_obj_t* p,const char* t,lv_color_t c,const lv_font_t* f){
  lv_obj_t* l=lv_label_create(p); lv_label_set_text(l,t);
  lv_obj_set_style_text_color(l,c,0);
  if(f) lv_obj_set_style_text_font(l,f,0);
  return l;
}
static lv_obj_t* badge(lv_obj_t* p,const char* t,lv_color_t bg){
  lv_obj_t* l=lv_label_create(p); lv_label_set_text(l,t);
  lv_obj_set_style_bg_color(l,bg,0); lv_obj_set_style_bg_opa(l,LV_OPA_COVER,0);
  lv_obj_set_style_text_color(l,C(0xffffff),0);
  lv_obj_set_style_radius(l,8,0);
  lv_obj_set_style_pad_hor(l,8,0); lv_obj_set_style_pad_ver(l,3,0);
  return l;
}
static void kvRow(lv_obj_t* parent,const char* key,lv_obj_t** valOut){
  lv_obj_t* row=card(parent);
  lv_obj_set_flex_flow(row,LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
  lv_obj_t* k=label(row,key,col_muted(),NULL); lv_obj_set_width(k,80);
  lv_obj_t* v=label(row,"--",col_text(),NULL); lv_obj_set_flex_grow(v,1);
  if(valOut) *valOut=v;
}
static void resultRow(lv_obj_t* parent,const char* key,lv_obj_t** valOut){
  lv_obj_t* row=card(parent);
  lv_obj_set_flex_flow(row,LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
  lv_obj_t* k=label(row,key,col_text(),NULL); lv_obj_set_flex_grow(k,1);
  lv_obj_t* v=label(row,"--",col_muted(),F_HEAD);
  if(valOut) *valOut=v;
}

// ---------- events ----------
static void ev_goHome(lv_event_t* e){(void)e; cur=0; lv_screen_load(scr_home);}
static void ev_goCable(lv_event_t* e){(void)e; cur=1; lv_screen_load(scr_cable);}
static void ev_goNet(lv_event_t* e){(void)e; cur=2; lv_screen_load(scr_net);}
static void ev_goAuto(lv_event_t* e){(void)e; cur=3; lv_screen_load(scr_auto);}
static void ev_goSwitch(lv_event_t* e){(void)e; cur=4; lv_screen_load(scr_switch);}
static void ev_goTools(lv_event_t* e){(void)e; cur=5; lv_screen_load(scr_tools);}
static void ev_theme(lv_event_t* e){
  g_dark = lv_obj_has_state((lv_obj_t*)lv_event_get_target(e), LV_STATE_CHECKED);
  want_theme_rebuild = true;   // defer rebuild - fixes the crash
}
static void ev_runCable(lv_event_t* e){(void)e; run_cable=true;}
static void ev_runNet(lv_event_t* e){(void)e; run_net=true;}
static void ev_runAuto(lv_event_t* e){(void)e; run_auto2=true;}
static void ev_pingGw(lv_event_t* e){(void)e; queued_ping=1;}
static void ev_pingGoog(lv_event_t* e){(void)e; queued_ping=2;}
static void ev_traceGw(lv_event_t* e){(void)e; queued_trace=1;}
static void ev_traceGoog(lv_event_t* e){(void)e; queued_trace=2;}
static void ev_identify(lv_event_t* e){(void)e; queued_identify=true;}
static void ev_flash(lv_event_t* e){(void)e; queued_flash=true;}

// ---------- shared chrome ----------
static lv_obj_t* header(lv_obj_t* scr,const char* title,bool back){
  lv_obj_t* h=lv_obj_create(scr);
  lv_obj_set_width(h,lv_pct(100)); lv_obj_set_height(h,58);
  lv_obj_set_style_bg_color(h,col_surf(),0);
  lv_obj_set_style_border_color(h,col_border(),0);
  lv_obj_set_style_border_width(h,1,0); lv_obj_set_style_radius(h,0,0);
  lv_obj_set_style_pad_hor(h,10,0);
  lv_obj_set_flex_flow(h,LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(h,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
  lv_obj_clear_flag(h,LV_OBJ_FLAG_SCROLLABLE);
  if(back){
    lv_obj_t* b=lv_button_create(h); lv_obj_set_size(b,40,40);
    lv_obj_set_style_bg_color(b,col_surf(),0);
    lv_obj_set_style_border_color(b,col_border(),0);
    lv_obj_set_style_border_width(b,1,0);
    lv_obj_t* bl=label(b,LV_SYMBOL_LEFT,col_primary(),NULL); lv_obj_center(bl);
    lv_obj_add_event_cb(b,ev_goHome,LV_EVENT_CLICKED,NULL);
  }
  lv_obj_t* t=label(h,title,col_text(),F_HEAD); lv_obj_set_flex_grow(t,1);
  lv_obj_t* sw=lv_switch_create(h);
  if(g_dark) lv_obj_add_state(sw,LV_STATE_CHECKED);
  lv_obj_add_event_cb(sw,ev_theme,LV_EVENT_VALUE_CHANGED,NULL);
  return h;
}
static lv_obj_t* body(lv_obj_t* scr){
  lv_obj_t* b=lv_obj_create(scr);
  lv_obj_set_width(b,lv_pct(100)); lv_obj_set_flex_grow(b,1);
  lv_obj_set_style_bg_opa(b,0,0); lv_obj_set_style_border_width(b,0,0);
  lv_obj_set_style_pad_all(b,12,0); lv_obj_set_style_pad_row(b,10,0);
  lv_obj_set_flex_flow(b,LV_FLEX_FLOW_COLUMN);
  return b;
}
static void runButton(lv_obj_t* scr,const char* txt,lv_event_cb_t cb){
  lv_obj_t* f=lv_obj_create(scr);
  lv_obj_set_width(f,lv_pct(100)); lv_obj_set_height(f,LV_SIZE_CONTENT);
  lv_obj_set_style_bg_color(f,col_surf(),0);
  lv_obj_set_style_border_color(f,col_border(),0);
  lv_obj_set_style_border_width(f,1,0); lv_obj_set_style_radius(f,0,0);
  lv_obj_set_style_pad_all(f,8,0); lv_obj_clear_flag(f,LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t* btn=lv_button_create(f);
  lv_obj_set_width(btn,lv_pct(100)); lv_obj_set_height(btn,44);
  lv_obj_set_style_bg_color(btn,col_primary(),0);
  lv_obj_t* l=label(btn,txt,C(0xffffff),F_HEAD); lv_obj_center(l);
  lv_obj_add_event_cb(btn,cb,LV_EVENT_CLICKED,NULL);
}
static void smallBtn(lv_obj_t* row,const char* txt,lv_event_cb_t cb){
  lv_obj_t* btn=lv_button_create(row);
  lv_obj_set_flex_grow(btn,1); lv_obj_set_height(btn,40);
  lv_obj_set_style_bg_color(btn,col_primary(),0);
  lv_obj_t* l=label(btn,txt,C(0xffffff),NULL); lv_obj_center(l);
  lv_obj_add_event_cb(btn,cb,LV_EVENT_CLICKED,NULL);
}
static void navBtn(lv_obj_t* parent,const char* title,const char* sub,lv_event_cb_t cb){
  lv_obj_t* b=lv_button_create(parent);
  lv_obj_set_width(b,lv_pct(100)); lv_obj_set_height(b,72);
  lv_obj_set_style_bg_color(b,col_surf(),0);
  lv_obj_set_style_border_color(b,col_border(),0);
  lv_obj_set_style_border_width(b,1,0); lv_obj_set_style_radius(b,12,0);
  lv_obj_set_flex_flow(b,LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(b,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_left(b,14,0);
  label(b,title,col_text(),F_HEAD); label(b,sub,col_muted(),NULL);
  lv_obj_add_event_cb(b,cb,LV_EVENT_CLICKED,NULL);
}

// ---------- HOME ----------
static void buildHome(){
  scr_home=lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scr_home,col_bg(),0);
  lv_obj_set_flex_flow(scr_home,LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(scr_home,0,0); lv_obj_set_style_pad_row(scr_home,0,0);
  lv_obj_t* h=lv_obj_create(scr_home);
  lv_obj_set_width(h,lv_pct(100)); lv_obj_set_height(h,58);
  lv_obj_set_style_bg_color(h,col_surf(),0);
  lv_obj_set_style_border_color(h,col_border(),0);
  lv_obj_set_style_border_width(h,1,0); lv_obj_set_style_pad_hor(h,10,0);
  lv_obj_set_flex_flow(h,LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(h,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
  lv_obj_clear_flag(h,LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t* stack=lv_obj_create(h);
  lv_obj_set_style_bg_opa(stack,0,0); lv_obj_set_style_border_width(stack,0,0);
  lv_obj_set_style_pad_all(stack,0,0); lv_obj_set_height(stack,LV_SIZE_CONTENT);
  lv_obj_set_flex_grow(stack,1); lv_obj_set_flex_flow(stack,LV_FLEX_FLOW_COLUMN);
  label(stack,"NETWORK GOBLIN",col_text(),F_HEAD); label(stack,"ND-1",col_muted(),NULL);
  lv_obj_t* sw=lv_switch_create(h);
  if(g_dark) lv_obj_add_state(sw,LV_STATE_CHECKED);
  lv_obj_add_event_cb(sw,ev_theme,LV_EVENT_VALUE_CHANGED,NULL);
  lv_obj_t* b=body(scr_home);
  navBtn(b,"AUTOTEST","Cable + network, one tap",ev_goAuto);
  navBtn(b,"CABLE TEST","Length / Fault / Status",ev_goCable);
  navBtn(b,"NETWORK TEST","DHCP / Ping / Trace",ev_goNet);
  navBtn(b,"SWITCH INFO","Identify port / VLAN / flash",ev_goSwitch);
  navBtn(b,"TOOLS","Settings / Logs / About",ev_goTools);
}

// ---------- CABLE ----------
static void buildCable(){
  scr_cable=lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scr_cable,col_bg(),0);
  lv_obj_set_flex_flow(scr_cable,LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(scr_cable,0,0); lv_obj_set_style_pad_row(scr_cable,0,0);
  header(scr_cable,"CABLE TEST",true);
  lv_obj_t* bd=body(scr_cable);
  lv_obj_t* len=card(bd);
  lv_obj_set_flex_flow(len,LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(len,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
  label(len,"CABLE LENGTH",col_muted(),NULL);
  lbl_length=label(len,"-- ft",col_text(),F_TITLE);
  lv_obj_t* fc=card(bd);
  lv_obj_set_flex_flow(fc,LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(fc,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
  lv_obj_t* fcol=lv_obj_create(fc);
  lv_obj_set_style_bg_opa(fcol,0,0); lv_obj_set_style_border_width(fcol,0,0);
  lv_obj_set_style_pad_all(fcol,0,0); lv_obj_set_height(fcol,LV_SIZE_CONTENT);
  lv_obj_set_flex_grow(fcol,1); lv_obj_set_flex_flow(fcol,LV_FLEX_FLOW_COLUMN);
  label(fcol,"Fault",col_text(),NULL);
  lbl_fault=label(fcol,"None detected",col_muted(),NULL);
  badge_fault=badge(fc," OK ",col_pass());
  label(bd,"PAIR STATUS",col_muted(),NULL);
  kvRow(bd,"Pair 1-2",&lbl_p12); kvRow(bd,"Pair 3-6",&lbl_p36);
  kvRow(bd,"Pair 4-5",&lbl_p45); kvRow(bd,"Pair 7-8",&lbl_p78);
  runButton(scr_cable,"RUN CABLE TEST",ev_runCable);
}

// ---------- NETWORK ----------
static void buildNet(){
  scr_net=lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scr_net,col_bg(),0);
  lv_obj_set_flex_flow(scr_net,LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(scr_net,0,0); lv_obj_set_style_pad_row(scr_net,0,0);
  header(scr_net,"NETWORK TEST",true);
  lv_obj_t* strip=lv_obj_create(scr_net);
  lv_obj_set_width(strip,lv_pct(100)); lv_obj_set_height(strip,34);
  lv_obj_set_style_bg_color(strip,col_idle(),0); lv_obj_set_style_radius(strip,0,0);
  lv_obj_clear_flag(strip,LV_OBJ_FLAG_SCROLLABLE);
  lbl_link=label(strip,"LINK: -- / --",C(0xffffff),F_HEAD); lv_obj_center(lbl_link);
  lv_obj_t* bd=body(scr_net);
  lv_obj_t* rd=card(bd);
  lv_obj_set_flex_flow(rd,LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(rd,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
  lv_obj_t* dl=label(rd,"DHCP",col_text(),NULL); lv_obj_set_flex_grow(dl,1);
  badge_dhcp=badge(rd," -- ",col_idle());
  kvRow(bd,"IP",&lbl_ip); kvRow(bd,"Subnet",&lbl_mask);
  kvRow(bd,"Gateway",&lbl_gw); kvRow(bd,"DNS",&lbl_dns);
  label(bd,"PING / TRACE",col_muted(),NULL);
  lv_obj_t* r1=lv_obj_create(bd);
  lv_obj_set_width(r1,lv_pct(100)); lv_obj_set_height(r1,LV_SIZE_CONTENT);
  lv_obj_set_style_bg_opa(r1,0,0); lv_obj_set_style_border_width(r1,0,0);
  lv_obj_set_style_pad_all(r1,0,0); lv_obj_set_style_pad_column(r1,8,0);
  lv_obj_set_flex_flow(r1,LV_FLEX_FLOW_ROW); lv_obj_clear_flag(r1,LV_OBJ_FLAG_SCROLLABLE);
  smallBtn(r1,"Ping GW",ev_pingGw); smallBtn(r1,"Ping Google",ev_pingGoog);
  lv_obj_t* r2=lv_obj_create(bd);
  lv_obj_set_width(r2,lv_pct(100)); lv_obj_set_height(r2,LV_SIZE_CONTENT);
  lv_obj_set_style_bg_opa(r2,0,0); lv_obj_set_style_border_width(r2,0,0);
  lv_obj_set_style_pad_all(r2,0,0); lv_obj_set_style_pad_column(r2,8,0);
  lv_obj_set_flex_flow(r2,LV_FLEX_FLOW_ROW); lv_obj_clear_flag(r2,LV_OBJ_FLAG_SCROLLABLE);
  smallBtn(r2,"Trace GW",ev_traceGw); smallBtn(r2,"Trace Google",ev_traceGoog);
  lv_obj_t* res=card(bd);
  lbl_netresult=label(res,"Results appear here.",col_muted(),NULL);
  lv_label_set_long_mode(lbl_netresult, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(lbl_netresult, lv_pct(100));
  runButton(scr_net,"RUN NETWORK TEST",ev_runNet);
}

// ---------- AUTOTEST ----------
static void buildAuto(){
  scr_auto=lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scr_auto,col_bg(),0);
  lv_obj_set_flex_flow(scr_auto,LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(scr_auto,0,0); lv_obj_set_style_pad_row(scr_auto,0,0);
  header(scr_auto,"AUTOTEST",true);
  lv_obj_t* bd=body(scr_auto);
  resultRow(bd,"Cable",   &lbl_a_cable);
  resultRow(bd,"Length",  &lbl_a_len);
  resultRow(bd,"Gateway", &lbl_a_gw);
  resultRow(bd,"Internet",&lbl_a_inet);
  runButton(scr_auto,"RUN AUTOTEST",ev_runAuto);
}

// ---------- SWITCH INFO ----------
static void buildSwitch(){
  scr_switch=lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scr_switch,col_bg(),0);
  lv_obj_set_flex_flow(scr_switch,LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(scr_switch,0,0); lv_obj_set_style_pad_row(scr_switch,0,0);
  header(scr_switch,"SWITCH INFO",true);
  lv_obj_t* bd=body(scr_switch);
  resultRow(bd,"Switch",&lbl_sw_name);
  resultRow(bd,"Port",  &lbl_sw_port);
  resultRow(bd,"VLAN",  &lbl_sw_vlan);
  lv_obj_t* st=card(bd);
  lbl_sw_status=label(st,"Tap Identify (~45s).\nNetworking pauses during capture.",col_muted(),NULL);
  lv_label_set_long_mode(lbl_sw_status,LV_LABEL_LONG_WRAP);
  lv_obj_set_width(lbl_sw_status,lv_pct(100));
  runButton(scr_switch,"IDENTIFY SWITCH",ev_identify);
  runButton(scr_switch,"FLASH PORT LED",ev_flash);
}

// ---------- TOOLS (placeholder) ----------
static void buildTools(){
  scr_tools=lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scr_tools,col_bg(),0);
  lv_obj_set_flex_flow(scr_tools,LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(scr_tools,0,0); lv_obj_set_style_pad_row(scr_tools,0,0);
  header(scr_tools,"TOOLS",true);
  lv_obj_t* bd=body(scr_tools);
  lv_obj_t* c=card(bd);
  lv_obj_t* l=label(c,"Coming soon:\n- Settings (theme, units)\n- TDR calibration\n- Logs / history\n- About",col_muted(),NULL);
  lv_label_set_long_mode(l,LV_LABEL_LONG_WRAP);
  lv_obj_set_width(l,lv_pct(100));
}

static void buildAll(){
  if(scr_home)  { lv_obj_del(scr_home);  scr_home=NULL; }
  if(scr_cable) { lv_obj_del(scr_cable); scr_cable=NULL; }
  if(scr_net)   { lv_obj_del(scr_net);   scr_net=NULL; }
  if(scr_auto)  { lv_obj_del(scr_auto);  scr_auto=NULL; }
  if(scr_switch){ lv_obj_del(scr_switch);scr_switch=NULL; }
  if(scr_tools) { lv_obj_del(scr_tools); scr_tools=NULL; }
  buildHome(); buildCable(); buildNet(); buildAuto(); buildSwitch(); buildTools();
}

void ngUiInit(){ buildAll(); }
void ngUiLoadHome(){ cur=0; lv_screen_load(scr_home); }

// ---------- publishers ----------
void ngUiPublishCable(const NgCable& c){
  char buf[24];
  if(c.length_valid) snprintf(buf,sizeof(buf),"~%.0f ft",c.length_ft);
  else if(c.link_up) snprintf(buf,sizeof(buf),"linked");
  else               snprintf(buf,sizeof(buf),"-- ft");
  if(lbl_length) lv_label_set_text(lbl_length,buf);
}
void ngUiPublishTdr(const NgTdrResult& r){
  NgTdrType t=r.mdi.type;
  if(t==NG_TDR_OPEN||t==NG_TDR_SHORT){
    char f[32]; snprintf(f,sizeof(f),"%s @ %.0f ft",ngTdrTypeStr(t),r.mdi.dist_ft);
    if(lbl_fault) lv_label_set_text(lbl_fault,f);
    if(badge_fault){ lv_label_set_text(badge_fault," FAULT "); lv_obj_set_style_bg_color(badge_fault,col_fail(),0); }
    if(lbl_length){ char b[24]; snprintf(b,sizeof(b),"%.0f ft",r.mdi.dist_ft); lv_label_set_text(lbl_length,b); }
  } else {
    if(lbl_fault) lv_label_set_text(lbl_fault,"None detected");
    if(badge_fault){ lv_label_set_text(badge_fault," OK "); lv_obj_set_style_bg_color(badge_fault,col_pass(),0); }
  }
}
static void colorPair(lv_obj_t* l,NgPairState s,float ft){
  if(!l) return; char b[24];
  if(s==NGP_OPEN||s==NGP_SHORT) snprintf(b,sizeof(b),"%s @ %.0f ft",ngPairStateStr(s),ft);
  else snprintf(b,sizeof(b),"%s",ngPairStateStr(s));
  lv_label_set_text(l,b);
  lv_color_t c=(s==NGP_OK)?col_pass():(s==NGP_NA)?col_idle():col_fail();
  lv_obj_set_style_text_color(l,c,0);
}
void ngUiPublishPairs(const NgWiremap& w){
  colorPair(lbl_p12,w.p12.state,w.p12.dist_ft);
  colorPair(lbl_p36,w.p36.state,w.p36.dist_ft);
  colorPair(lbl_p45,w.p45.state,0); colorPair(lbl_p78,w.p78.state,0);
}
void ngUiPublishNet(const NgNet& n){
  if(lbl_ip)   lv_label_set_text(lbl_ip,   n.got_ip? n.ip.c_str():"--");
  if(lbl_mask) lv_label_set_text(lbl_mask, n.got_ip? n.mask.c_str():"--");
  if(lbl_gw)   lv_label_set_text(lbl_gw,   n.got_ip? n.gw.c_str():"--");
  if(lbl_dns)  lv_label_set_text(lbl_dns,  n.got_ip? n.dns.c_str():"--");
  if(badge_dhcp){ lv_label_set_text(badge_dhcp, n.got_ip? " LEASED ":" NONE ");
    lv_obj_set_style_bg_color(badge_dhcp, n.got_ip? col_pass():col_fail(),0); }
  if(lbl_link){ char b[32]; snprintf(b,sizeof(b),"LINK: %s / %dM",n.link_up?"UP":"DOWN",n.speed_mbps);
    lv_label_set_text(lbl_link,b); }
}
static void setResult(const char* s){ if(lbl_netresult) lv_label_set_text(lbl_netresult,s); }
static void setPF(lv_obj_t* l,bool pass,const char* txt){
  if(!l) return; lv_label_set_text(l,txt);
  lv_obj_set_style_text_color(l, pass?col_pass():col_fail(), 0);
}

void ngUiOnCable(){ run_cable=true; }
void ngUiOnNetwork(){ run_net=true; }
void ngUiOnAutotest(){ run_auto2=true; }

static String g_trace;
static void traceHop(int hop,const char* ip,uint32_t rtt,bool reached){
  char line[48]; snprintf(line,sizeof(line)," %2d %-15s %lums%s\n",hop,ip,rtt,reached?" *":"");
  g_trace += line;
}

void ngUiService(){
  // deferred theme rebuild (crash fix)
  if(want_theme_rebuild){
    want_theme_rebuild=false;
    int keep=cur;
    buildAll();
    lv_screen_load(keep==0?scr_home:keep==1?scr_cable:keep==2?scr_net:
                   keep==3?scr_auto:keep==4?scr_switch:scr_tools);
  }

  if(run_cable){ run_cable=false;
    NgCable c; ngCableRun(c); ngUiPublishCable(c);
    NgTdrResult r; ngTdrRun(r); ngUiPublishTdr(r);
    NgWiremap w; ngPairsRun(w); ngUiPublishPairs(w);
  }
  if(run_net){ run_net=false;
    NgNet n; ngEthSnapshot(n); ngUiPublishNet(n);
  }
  if(run_auto2){ run_auto2=false;
    NgCable c; ngCableRun(c);
    NgTdrResult r; ngTdrRun(r);
    bool cableOk = (r.mdi.type!=NG_TDR_OPEN && r.mdi.type!=NG_TDR_SHORT);
    setPF(lbl_a_cable, cableOk, cableOk?"PASS":"FAULT");
    char lb[24];
    if(r.mdi.type==NG_TDR_OPEN||r.mdi.type==NG_TDR_SHORT) snprintf(lb,sizeof(lb),"%.0f ft (fault)",r.mdi.dist_ft);
    else if(c.length_valid) snprintf(lb,sizeof(lb),"~%.0f ft",c.length_ft);
    else snprintf(lb,sizeof(lb),"linked");
    if(lbl_a_len){ lv_label_set_text(lbl_a_len,lb); lv_obj_set_style_text_color(lbl_a_len,col_muted(),0); }
    if(cableOk){
      String gw=ngEthGateway();
      NgPingResult pg, pi;
      bool gwOk = gw.length() && ngPing(gw.c_str(),3,pg);
      bool inOk = ngPing("8.8.8.8",3,pi);
      setPF(lbl_a_gw,  gwOk, gwOk?"PASS":"FAIL");
      setPF(lbl_a_inet,inOk, inOk?"PASS":"FAIL");
    } else {
      setPF(lbl_a_gw,  false,"skipped");
      setPF(lbl_a_inet,false,"skipped");
    }
  }
  if(queued_ping){ int which=queued_ping; queued_ping=0;
    String tgt = (which==1)? ngEthGateway() : String("8.8.8.8");
    if(tgt.length()==0){ setResult("No gateway (no DHCP lease)."); }
    else {
      setResult(which==1? "Pinging gateway..." : "Pinging Google...");
      NgPingResult pr;
      if(ngPing(tgt.c_str(),4,pr)){
        char b[96]; snprintf(b,sizeof(b),"Ping %s\n%d/%d replies, avg %lums\n(min %lu / max %lu)",
          pr.resolved_ip.c_str(),pr.recv,pr.sent,pr.avg_ms,pr.min_ms,pr.max_ms);
        setResult(b);
      } else { char b[64]; snprintf(b,sizeof(b),"Ping %s: NO REPLY",tgt.c_str()); setResult(b); }
    }
  }
  if(queued_trace){ int which=queued_trace; queued_trace=0;
    String tgt = (which==1)? ngEthGateway() : String("8.8.8.8");
    if(tgt.length()==0){ setResult("No gateway (no DHCP lease)."); }
    else {
      setResult(which==1? "Tracing gateway..." : "Tracing Google...");
      g_trace = String("Trace ")+tgt+"\n";
      ngTraceroute(tgt.c_str(), (which==1?4:15), traceHop);
      setResult(g_trace.c_str());
    }
  }

  // ---- Switch Info ----
  if(queued_identify){ queued_identify=false;
    if(lbl_sw_status) lv_label_set_text(lbl_sw_status,"Listening for LLDP/CDP...\n(networking paused ~45s)");
    lv_refr_now(NULL);   // force the "listening" text to draw before we block
    ngLldpStart();
    uint32_t t0=millis();
    while(millis()-t0 < 45000 && !ngLldpFresh(60000)){ delay(200); }
    NgSwitchInfo si; ngLldpGet(si);
    ngLldpStop();        // restores networking
    if(si.have_lldp || si.have_cdp){
      if(lbl_sw_name) lv_label_set_text(lbl_sw_name, si.sys_name[0]?si.sys_name:"?");
      if(lbl_sw_port) lv_label_set_text(lbl_sw_port, si.port_id[0]?si.port_id:"?");
      char vb[16];
      if(si.vlan>=0) snprintf(vb,sizeof(vb),"%d",si.vlan); else snprintf(vb,sizeof(vb),"(untagged)");
      if(lbl_sw_vlan) lv_label_set_text(lbl_sw_vlan, vb);
      if(lbl_sw_status) lv_label_set_text(lbl_sw_status,"Found (via LLDP/CDP).\nNetworking restored.");
    } else {
      if(lbl_sw_status) lv_label_set_text(lbl_sw_status,"Nothing seen. Switch may not\nsend LLDP/CDP, or try again.");
    }
  }
  if(queued_flash){ queued_flash=false;
    ngLldpPortFlash(5);
    if(lbl_sw_status) lv_label_set_text(lbl_sw_status,"Flashing port LED (5 cycles).\nWatch the switch. (UniFi may not blink)");
  }
}
