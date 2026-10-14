#include "OdometerEngine.h"
#include "MaintenanceEngine.h"
#include <cassert>
#include <iostream>
int main(){
 OdometerEngine o; o.restore({298772,4175,true,0});
 MaintenanceEngine m; m.evaluate(o.getCurrentOdo(),0);
 int before=m.getItems()[0].kmLeft;
 o.updateFromPid31(4176);m.evaluate(o.getCurrentOdo(),0);
 assert(o.getCurrentOdo()==298773);assert(m.getItems()[0].kmLeft==before-1);
 // No response leaves the established baseline intact; next valid counter catches up.
 o.updateFromPid31(4178);m.evaluate(o.getCurrentOdo(),0);
 assert(o.getCurrentOdo()==298775);assert(m.getItems()[0].kmLeft==before-3);
 auto checkpoint=o.snapshot();OdometerEngine reboot;reboot.restore(checkpoint);reboot.updateFromPid31(4179);
 assert(reboot.getCurrentOdo()==298776);
 std::cout<<"Distance delta, countdown and checkpoint regression passed\n";
}
