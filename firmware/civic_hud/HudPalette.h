#pragma once
#include <stdint.h>
namespace HudPalette {
constexpr uint16_t rgb565(uint8_t r,uint8_t g,uint8_t b){return uint16_t((r>>3)<<11)|uint16_t((g>>2)<<5)|uint16_t(b>>3);}
constexpr uint8_t levels[8]={0,36,73,109,146,182,219,255};
// Eight neutral levels plus the three selected status shades fit a 4-bit sprite.
inline void make(bool night,uint16_t out[16]){
 for(int i=0;i<16;++i)out[i]=0;
 for(int i=0;i<8;++i)out[i]=rgb565(levels[i],levels[i],levels[i]);
 uint8_t g=levels[night?5:4],y=levels[night?5:3],r=levels[night?6:5];
 out[8]=rgb565(0,g,0);out[9]=rgb565(y,y,0);out[10]=rgb565(r,0,0);
}
}
