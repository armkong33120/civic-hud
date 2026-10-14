#pragma once
#include "ThaiFontData.h"
#include <string>
#include <cstring>
class HudThaiFont {
public:
 static uint32_t next(const std::string& s,size_t& i) {
  unsigned char c=s[i++]; if(c<128)return c;
  int n=(c>=0xc2&&c<=0xdf)?1:(c>=0xe0&&c<=0xef)?2:(c>=0xf0&&c<=0xf4)?3:0;
  if(!n)return '?'; uint32_t cp=c&((1<<(6-n))-1);
  for(int k=0;k<n;++k){if(i>=s.size()||((unsigned char)s[i]&0xc0)!=0x80)return '?';cp=(cp<<6)|((unsigned char)s[i++]&63);}
  if((n==1&&cp<128)||(n==2&&cp<2048)||(n==3&&cp<65536)||cp>0x10ffff||(cp>=0xd800&&cp<=0xdfff))return '?';return cp;
 }
 // Presentation only: keep stored names/history intact, omit decorative emoji.
 static std::string plainName(const std::string& s){std::string out;for(size_t i=0;i<s.size();){size_t begin=i;uint32_t cp=next(s,i);if(cp==0xfe0f||cp==0xfe0e||cp==0x200d||(cp>=0x2600&&cp<=0x27bf)||(cp>=0x1f000&&cp<=0x1faff))continue;if(out.empty()&&cp==' ')continue;out.append(s,begin,i-begin);}return out;}
 static HudGlyph glyph(uint32_t cp){
  size_t idx=cp>=32&&cp<=126?cp-32:cp>=0xe01&&cp<=0xe5b?95+cp-0xe01:'?'-32; HudGlyph g;
#ifdef ARDUINO
  memcpy_P(&g,&kHudGlyphs[idx],sizeof(g));
#else
  g=kHudGlyphs[idx];
#endif
  return g;
 }
 static bool above(uint32_t c){return c==0xe31||(c>=0xe34&&c<=0xe37)||c==0xe47||c==0xe4d||c==0xe4e;}
 static bool tone(uint32_t c){return c>=0xe48&&c<=0xe4c;}
 static bool below(uint32_t c){return c>=0xe38&&c<=0xe3a;}
 // All text is clipped to its region. Combining marks overlay the preceding base;
 // tone marks rise one tier above an existing upper vowel, tall bases shift marks left.
 template<class Canvas> static int draw(Canvas& out,const std::string& s,int x,int y,uint16_t color,int maxWidth){
  int cursor=x,base=x;uint32_t prev=0;bool upper=false;bool haveBase=false;bool trimTail=false;
  auto paint=[&](uint32_t cp,int gx,int gy){auto g=glyph(cp);for(int r=0;r<28;++r)for(int col=0;col<16;++col)if((!trimTail||r<22)&&(g.rows[r]&(0x8000>>col))){int px=gx+col;if(px>=x&&px<x+maxWidth&&gy+r>=y&&gy+r<y+28)out.drawPixel(px,gy+r,color);}return (int)g.advance;};
  for(size_t i=0;i<s.size();){uint32_t cp=next(s,i);if(cp==0xfe0f||cp==0xfe0e||cp==0x200d||cp>0xffff)continue;
   if(cp==0xe33){if(haveBase)paint(0xe4d,base,y);cp=0xe32;}
   if(above(cp)||tone(cp)||below(cp)){if(!haveBase)continue;int shift=(prev==0xe1b||prev==0xe1d||prev==0xe1f)?-2:0;size_t peek=i;uint32_t following=peek<s.size()?next(s,peek):0;paint(cp,base+shift,y-(tone(cp)&&(upper||following==0xe33)?4:0));if(above(cp))upper=true;continue;}
   auto g=glyph(cp);if(cursor+g.advance>x+maxWidth){if(maxWidth>=9)paint('.',x+maxWidth-7,y);break;}
   size_t peek=i;uint32_t following=peek<s.size()?next(s,peek):0;trimTail=(cp==0xe0d||cp==0xe10)&&below(following);base=cursor;cursor+=paint(cp,cursor,y);trimTail=false;prev=cp;upper=false;haveBase=true;
  }return cursor-x;
 }
};
