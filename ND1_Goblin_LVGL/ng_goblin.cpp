/* ng_goblin.cpp - harmless chaos, context-aware */
#include "ng_goblin.h"
#include <esp_system.h>
static const char* pick(const char* const* a, size_t n){ return a[esp_random()%n]; }
void ngGoblinInit(){ (void)esp_random(); }
const char* ngGoblinBootLine(){
  static const char* const normal[]={"Goblin awake.","Sniffing wires...","Here be packets.","Layer 1 awaits."};
  static const char* const rare[]={"PATCH PANEL GREMLINS DETECTED","YOU SHOULD LABEL YOUR CABLES","DNS IS PROBABLY PLOTTING","CERTIFIED CAT5E ENJOYER"};
  return (esp_random()%25==0)?pick(rare,4):pick(normal,4);
}
const char* ngGoblinLine(NgGoblinEvent e,bool extreme){
  static const char* const idle[]={"Goblin: Listening...","Goblin: Awaiting cable.","Goblin: Packets welcome."};
  static const char* const cableOk[]={"Goblin: Cable appears healthy.","Goblin: Layer 1 approves.","Goblin: Copper tastes acceptable."};
  static const char* const cableBad[]={"Goblin: Found the break.","Goblin: Cable suspicious.","Goblin: Somebody used staples again."};
  static const char* const netOk[]={"Goblin: Packets returned safely.","Goblin: Internet appears edible.","Goblin: Gateway answers."};
  static const char* const netBad[]={"Goblin: Packets escaped captivity.","Goblin: DNS plotting again.","Goblin: Check layer 1. Then DNS."};
  static const char* const swOk[]={"Goblin: Switch identified.","Goblin: Port located.","Goblin sees all."};
  static const char* const swBad[]={"Goblin: Switch is hiding.","Goblin: No LLDP tribute received.","Goblin: Blame the contractor."};
  static const char* const maxed[]={"Goblin Level: MAXIMUM","This VLAN feels wrong.","Spanning tree fears the Goblin.","Packets acquired."};
  if(extreme && (esp_random()%3==0)) return pick(maxed,4);
  switch(e){case NGG_CABLE_OK:return pick(cableOk,3);case NGG_CABLE_FAULT:return pick(cableBad,3);case NGG_NET_OK:return pick(netOk,3);case NGG_NET_FAIL:return pick(netBad,3);case NGG_SWITCH_OK:return pick(swOk,3);case NGG_SWITCH_FAIL:return pick(swBad,3);default:return pick(idle,3);}
}
