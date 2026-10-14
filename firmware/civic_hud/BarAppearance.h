#pragma once
#include "MaintenanceItem.h"
#include "HudPalette.h"
#include <algorithm>
struct HudBarAppearance { uint16_t rgb;uint8_t family,level;bool blink; };
inline HudBarAppearance hudBarAppearance(const MaintenanceItem& item){
 static constexpr uint16_t colors[3][8]={
  {HudPalette::rgb565(0,109,0),HudPalette::rgb565(0,146,0),HudPalette::rgb565(0,182,0),HudPalette::rgb565(0,192,0),HudPalette::rgb565(0,208,0),HudPalette::rgb565(0,219,0),HudPalette::rgb565(0,237,0),HudPalette::rgb565(0,255,0)},
  {HudPalette::rgb565(255,255,0),HudPalette::rgb565(237,237,0),HudPalette::rgb565(219,219,0),HudPalette::rgb565(200,200,0),HudPalette::rgb565(182,182,0),HudPalette::rgb565(170,170,0),HudPalette::rgb565(158,158,0),HudPalette::rgb565(146,146,0)},
  {HudPalette::rgb565(255,146,146),HudPalette::rgb565(255,122,122),HudPalette::rgb565(255,98,98),HudPalette::rgb565(255,73,73),HudPalette::rgb565(237,36,36),HudPalette::rgb565(219,0,0),HudPalette::rgb565(200,0,0),HudPalette::rgb565(182,0,0)}
 };
 int p=int(std::max(0.f,std::min(100.f,item.usedPct))*100+.5f);uint8_t family=0;int level=0;
 if(p<6000)level=p*8/6000;
 else if(p<8500){family=1;level=(p-6000)*8/2500;}
 else{family=2;level=std::min(7,(p-8500)*8/1000);}
 bool blink=item.intervalKm>0&&item.kmLeft<=500;
 if(blink){family=2;level=7;}
 return {colors[family][level],family,uint8_t(level),blink};
}
