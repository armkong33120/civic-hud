#include "../../firmware/civic_hud/BarAppearance.h"
#include <cassert>
#include <set>
#include <iostream>
int main(){MaintenanceItem i{};i.intervalKm=10000;i.kmLeft=9000;std::set<uint16_t> all;
for(int family=0;family<3;++family)for(int level=0;level<8;++level){i.usedPct=family==0?float((level+.25)*60/8):family==1?float(60+(level+.25)*25/8):float(85+(level+.25)*10/8);auto a=hudBarAppearance(i);assert(a.family==family&&a.level==level&&!a.blink);all.insert(a.rgb);}assert(all.size()==24);
i.usedPct=94.99;i.kmLeft=501;assert(!hudBarAppearance(i).blink);i.kmLeft=500;assert(hudBarAppearance(i).blink&&hudBarAppearance(i).level==7&&hudBarAppearance(i).family==2);i.kmLeft=-100;assert(hudBarAppearance(i).blink);i.intervalKm=0;assert(!hudBarAppearance(i).blink);i.intervalKm=10000;i.kmLeft=10000;i.usedPct=0;assert(!hudBarAppearance(i).blink&&hudBarAppearance(i).family==0&&hudBarAppearance(i).level==0);std::cout<<"24 distinct RGB565 shades, colour order, 501/500 km, overdue, time-only and service reset passed\n";}
