#include "../../firmware/civic_hud/ThaiFont.h"
#include <fstream>
struct Canvas {unsigned char p[240*168]={};void drawPixel(int x,int y,uint16_t){if(x>=0&&x<240&&y>=0&&y<168)p[y*240+x]=255;}};
int main(){Canvas c;const char* text[]={"น้ำมันเครื่อง + กรอง","น้ำมันเกียร์ CVT","กรองแอร์ห้องโดยสาร","ครั้งก่อน: 12 ส.ค. 26","ปี่ ฝุ่น ฟ้า ญู ฐุ","รูดเพื่อยืนยัน"};for(int n=0;n<6;++n)HudThaiFont::draw(c,text[n],0,n*28,1,240);std::ofstream out("work/thai_font_preview.pgm",std::ios::binary);out<<"P5\n240 168\n255\n";out.write((char*)c.p,sizeof(c.p));}
