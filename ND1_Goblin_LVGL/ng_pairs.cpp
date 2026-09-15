/* ng_pairs.cpp - per-pair status (2 active pairs on 10/100) */
#include "ng_pairs.h"
#include "ng_eth.h"
static NgPairState fromTdr(NgTdrType t){ switch(t){case NG_TDR_OPEN:return NGP_OPEN;case NG_TDR_SHORT:return NGP_SHORT;default:return NGP_OK;} }
void ngPairsRun(NgWiremap& w){
  NgCable c; ngCableRun(c); w.linked=c.link_up;
  NgTdrResult r; ngTdrRun(r);
  w.p12.name="1-2"; w.p12.testable=true; w.p12.state=fromTdr(r.mdi.type);
  w.p12.dist_ft=(w.p12.state==NGP_OPEN||w.p12.state==NGP_SHORT)?r.mdi.dist_ft:0;
  w.p36.name="3-6"; w.p36.testable=true; w.p36.state=fromTdr(r.mdix.type);
  w.p36.dist_ft=(w.p36.state==NGP_OPEN||w.p36.state==NGP_SHORT)?r.mdix.dist_ft:0;
  if(w.linked){ w.p12.state=NGP_OK; w.p12.dist_ft=0; w.p36.state=NGP_OK; w.p36.dist_ft=0; }
  w.p45.name="4-5"; w.p45.testable=false; w.p45.state=NGP_NA; w.p45.dist_ft=0;
  w.p78.name="7-8"; w.p78.testable=false; w.p78.state=NGP_NA; w.p78.dist_ft=0;
}
const char* ngPairStateStr(NgPairState s){ switch(s){case NGP_OK:return "OK";case NGP_OPEN:return "OPEN";case NGP_SHORT:return "SHORT";default:return "n/a";} }
