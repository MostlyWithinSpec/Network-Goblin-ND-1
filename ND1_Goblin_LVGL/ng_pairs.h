/* ng_pairs.h - per-pair cable status */
#pragma once
#include <Arduino.h>
#include "ng_tdr.h"
enum NgPairState { NGP_OK=0, NGP_OPEN, NGP_SHORT, NGP_NA };
struct NgPair { const char* name; NgPairState state; float dist_ft; bool testable; };
struct NgWiremap { NgPair p12, p36, p45, p78; bool linked; };
void ngPairsRun(NgWiremap& out);
const char* ngPairStateStr(NgPairState s);
