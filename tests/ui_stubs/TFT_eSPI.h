#pragma once
#include <vector>
#include <cstdint>
#include <cstdio>
#include <cassert>
extern uint32_t clockMs;
inline uint32_t millis(){return clockMs;}
struct ResourceMonitor{MonitorRange freeRam,ramUsedTenths,cpu[2];uint32_t largest=0;};
struct Push{int y,h;};
class TFT_eSPI{public:int rects=0;void fillRect(int,int,int,int,uint16_t){++rects;}int clears=0;uint16_t clearColor=0;std::vector<Push> pushes;void fillScreen(uint16_t c){clearColor=c;++clears;}};
class TFT_eSprite{TFT_eSPI* t;uint16_t palette[16]={};public:explicit TFT_eSprite(TFT_eSPI* x):t(x){}void setColorDepth(int b){assert(b==4);}void* createSprite(int w,int h){assert(w==240&&h==48);return this;}void createPalette(uint16_t* p,int n){assert(n==16);for(int i=0;i<16;++i)palette[i]=p[i];}uint16_t getPaletteColor(uint16_t i){assert(i<16);return palette[i];}void fillSprite(uint16_t c){assert(c<16);}void drawPixel(int,int,uint16_t c){assert(c<16);}void fillRoundRect(int,int,int,int,int,uint16_t c){assert(c<16);}void drawRoundRect(int,int,int,int,int,uint16_t c){assert(c<16);}void fillRect(int,int,int,int,uint16_t c){assert(c<16);}void fillCircle(int,int,int,uint16_t c){assert(c<16);}void pushSprite(int,int y){t->pushes.push_back({y,48});}bool pushSprite(int,int y,int,int,int,int h){t->pushes.push_back({y,h});return true;}};
