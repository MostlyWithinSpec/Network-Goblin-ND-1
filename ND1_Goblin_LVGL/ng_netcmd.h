/* ng_netcmd.h - ping + traceroute */
#pragma once
#include <Arduino.h>
struct NgPingResult { bool ok; int sent; int recv; uint32_t avg_ms; uint32_t min_ms; uint32_t max_ms; String resolved_ip; };
bool ngPing(const char* host, int count, NgPingResult& out);
typedef void (*NgHopCb)(int hop, const char* ip, uint32_t rtt_ms, bool reached);
bool ngTraceroute(const char* host, int max_hops, NgHopCb hopCb);
