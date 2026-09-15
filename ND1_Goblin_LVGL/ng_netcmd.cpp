/* ping + traceroute (ESP-IDF ping_sock) */
#include "ng_netcmd.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/ip_addr.h"
#include "ping/ping_sock.h"
#include <string.h>
static bool resolveHost(const char* host, ip_addr_t& addr, String& ipStr) {
  if (ipaddr_aton(host, &addr)) { ipStr = host; return true; }
  struct addrinfo hints = {}; hints.ai_family = AF_INET; hints.ai_socktype = SOCK_RAW;
  struct addrinfo* res = nullptr;
  if (getaddrinfo(host, nullptr, &hints, &res) != 0 || !res) return false;
  struct sockaddr_in* a = (struct sockaddr_in*)res->ai_addr;
  char buf[16]; inet_ntoa_r(a->sin_addr, buf, sizeof(buf)); ipStr = buf;
  ip_addr_t tmp; bool okp = ipaddr_aton(buf, &tmp); addr = tmp; freeaddrinfo(res); return okp;
}
struct PingCtx { int recv=0, sent=0; uint32_t sum=0, mn=0xFFFFFFFF, mx=0; SemaphoreHandle_t done; };
static void on_success(esp_ping_handle_t h, void* args){ PingCtx* c=(PingCtx*)args; uint32_t t=0; esp_ping_get_profile(h,ESP_PING_PROF_TIMEGAP,&t,sizeof(t)); c->recv++; c->sum+=t; if(t<c->mn)c->mn=t; if(t>c->mx)c->mx=t; }
static void on_end(esp_ping_handle_t h, void* args){ PingCtx* c=(PingCtx*)args; uint32_t tx=0; esp_ping_get_profile(h,ESP_PING_PROF_REQUEST,&tx,sizeof(tx)); c->sent=tx; xSemaphoreGive(c->done); }
bool ngPing(const char* host, int count, NgPingResult& out) {
  memset(&out,0,sizeof(out));
  ip_addr_t target; String ipStr; if(!resolveHost(host,target,ipStr)) return false;
  out.resolved_ip=ipStr;
  PingCtx ctx; ctx.done=xSemaphoreCreateBinary();
  esp_ping_config_t cfg=ESP_PING_DEFAULT_CONFIG(); cfg.target_addr=target; cfg.count=count; cfg.timeout_ms=1000; cfg.interval_ms=200;
  esp_ping_callbacks_t cbs={}; cbs.on_ping_success=on_success; cbs.on_ping_end=on_end; cbs.cb_args=&ctx;
  esp_ping_handle_t h; if(esp_ping_new_session(&cfg,&cbs,&h)!=ESP_OK){ vSemaphoreDelete(ctx.done); return false; }
  esp_ping_start(h); xSemaphoreTake(ctx.done, pdMS_TO_TICKS((count+2)*1500)); esp_ping_delete_session(h); vSemaphoreDelete(ctx.done);
  out.sent=ctx.sent?ctx.sent:count; out.recv=ctx.recv; out.ok=ctx.recv>0;
  out.avg_ms=ctx.recv?(ctx.sum/ctx.recv):0; out.min_ms=(ctx.mn==0xFFFFFFFF)?0:ctx.mn; out.max_ms=ctx.mx;
  return out.ok;
}
struct HopCtx { volatile bool got; uint32_t rtt; char ip[16]; SemaphoreHandle_t done; };
static void hop_success(esp_ping_handle_t h, void* args){ HopCtx* c=(HopCtx*)args; uint32_t t=0; ip_addr_t ra; esp_ping_get_profile(h,ESP_PING_PROF_TIMEGAP,&t,sizeof(t)); esp_ping_get_profile(h,ESP_PING_PROF_IPADDR,&ra,sizeof(ra)); c->rtt=t; c->got=true; ipaddr_ntoa_r(&ra,c->ip,sizeof(c->ip)); }
static void hop_end(esp_ping_handle_t h, void* args){ (void)h; HopCtx* c=(HopCtx*)args; xSemaphoreGive(c->done); }
bool ngTraceroute(const char* host, int max_hops, NgHopCb hopCb) {
  ip_addr_t target; String ipStr; if(!resolveHost(host,target,ipStr)) return false;
  for(int ttl=1; ttl<=max_hops; ttl++){
    HopCtx ctx; ctx.got=false; ctx.rtt=0; ctx.ip[0]=0; ctx.done=xSemaphoreCreateBinary();
    esp_ping_config_t cfg=ESP_PING_DEFAULT_CONFIG(); cfg.target_addr=target; cfg.count=1; cfg.timeout_ms=1000; cfg.ttl=ttl;
    esp_ping_callbacks_t cbs={}; cbs.on_ping_success=hop_success; cbs.on_ping_end=hop_end; cbs.cb_args=&ctx;
    esp_ping_handle_t h; if(esp_ping_new_session(&cfg,&cbs,&h)!=ESP_OK){ vSemaphoreDelete(ctx.done); return false; }
    esp_ping_start(h); xSemaphoreTake(ctx.done, pdMS_TO_TICKS(2000)); esp_ping_delete_session(h); vSemaphoreDelete(ctx.done);
    bool reached = ctx.got && (strcmp(ctx.ip, ipStr.c_str())==0);
    if(hopCb) hopCb(ttl, ctx.got?ctx.ip:"*", ctx.rtt, reached);
    if(reached) return true;
  }
  return false;
}
